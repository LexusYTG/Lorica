/* gl31_vk.c - puente GLES<->Vulkan: sincronizacion por fence nativo y queries de tiempo */
#define _GNU_SOURCE
#include "gl31_vk.h"
#include "../../vk/lvk.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static gl31_loader_fn g_loader;
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;

void gl31_vk_set_loader(gl31_loader_fn loader) { g_loader = loader; }

/* ---------- EGL: fence nativo ---------- */
#define L_EGL_SYNC_NATIVE_FENCE_ANDROID 0x3144
#define L_EGL_EXTENSIONS 0x3055
typedef void *(*fn_getcur)(void);
typedef void *(*fn_createsync)(void *dpy, unsigned type, const int *attribs);
typedef unsigned (*fn_destroysync)(void *dpy, void *sync);
typedef int (*fn_dupfd)(void *dpy, void *sync);
typedef const char *(*fn_qs)(void *dpy, int name);

static struct { int probed, ok; fn_getcur cur; fn_createsync create; fn_destroysync destroy; fn_dupfd dup; fn_qs qs; } E;

static void *egl_sym(const char *name)
{
    void *p = NULL; static void *h; static int tried;
    if (g_loader) p = g_loader(name);
    if (p) return p;
    if (!tried) {
        const char *env = getenv("LIBGL_EGL");
        tried = 1;
        if (env) h = dlopen(env, RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
        if (!h) h = dlopen("libEGL.so.1", RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
        if (!h) h = dlopen("libEGL.so", RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
    }
    return h ? dlsym(h, name) : NULL;
}

static int egl_fence_probe(void)
{
    void *dpy; const char *x;
    if (E.probed) return E.ok;
    E.probed = 1;
    E.cur = (fn_getcur)egl_sym("eglGetCurrentDisplay");
    E.qs = (fn_qs)egl_sym("eglQueryString");
    E.create = (fn_createsync)egl_sym("eglCreateSyncKHR");
    E.destroy = (fn_destroysync)egl_sym("eglDestroySyncKHR");
    E.dup = (fn_dupfd)egl_sym("eglDupNativeFenceFDANDROID");
    if (!E.cur || !E.qs || !E.create || !E.destroy || !E.dup) return 0;
    dpy = E.cur();
    if (!dpy) { E.probed = 0; return 0; }      /* sin display actual: reintentar mas tarde */
    x = E.qs(dpy, L_EGL_EXTENSIONS);
    E.ok = (x && strstr(x, "EGL_ANDROID_native_fence_sync") != NULL);
    return E.ok;
}

int gl31_vk_gl_fence_fd(void)
{
    void *dpy, *sync; int fd;
    if (!egl_fence_probe()) return -1;
    dpy = E.cur();
    if (!dpy) return -1;
    sync = E.create(dpy, L_EGL_SYNC_NATIVE_FENCE_ANDROID, NULL);
    if (!sync) return -1;
    BE(glFlush)();                               /* el fence nativo solo existe tras el flush */
    fd = E.dup(dpy, sync);
    E.destroy(dpy, sync);
    return fd >= 0 ? fd : -1;
}

int gl31_vk_gl_sync_for_vk(void)
{
    int fd = gl31_vk_gl_fence_fd();
    if (fd < 0) BE(glFinish)();                  /* sin fence nativo: sincronizacion en host */
    return fd;
}

/* ---------- queries de tiempo ---------- */
typedef struct { GLuint id; int kind; int s0, s1; } vkq_t;   /* kind 1 = TIMESTAMP, 2 = ELAPSED */
static vkq_t *g_q; static int g_nq, g_cap, g_state;          /* g_state: 0 sin probar, 1 ok, -1 no */

int gl31_vkq_ready(void)
{
    pthread_mutex_lock(&g_mu);
    if (g_state == 0) g_state = (lvk_init() && lvk_caps()->timestamps) ? 1 : -1;
    pthread_mutex_unlock(&g_mu);
    return g_state == 1;
}

static vkq_t *find(GLuint id) { for (int i = 0; i < g_nq; i++) if (g_q[i].id == id) return &g_q[i]; return NULL; }

static void drop(vkq_t *q)
{
    if (q->s0 >= 0) lvk_ts_release(q->s0);
    if (q->s1 >= 0) lvk_ts_release(q->s1);
    *q = g_q[--g_nq];
}

static vkq_t *slot_for(GLuint id, int kind)
{
    vkq_t *q = find(id);
    if (q) { if (q->s0 >= 0) lvk_ts_release(q->s0); if (q->s1 >= 0) lvk_ts_release(q->s1); }
    else {
        if (g_nq == g_cap) {
            int nc = g_cap ? g_cap * 2 : 32; vkq_t *n = (vkq_t *)realloc(g_q, (size_t)nc * sizeof *n);
            if (!n) return NULL;
            g_q = n; g_cap = nc;
        }
        q = &g_q[g_nq++];
    }
    q->id = id; q->kind = kind; q->s0 = q->s1 = -1;
    return q;
}

static int mark_now(void) { return lvk_ts_mark(gl31_vk_gl_sync_for_vk()); }

void gl31_vkq_counter(GLuint id)
{
    vkq_t *q; int s;
    pthread_mutex_lock(&g_mu);
    s = mark_now(); q = slot_for(id, 1);
    if (q) q->s0 = s; else if (s >= 0) lvk_ts_release(s);
    pthread_mutex_unlock(&g_mu);
}

void gl31_vkq_begin(GLuint id)
{
    vkq_t *q; int s;
    pthread_mutex_lock(&g_mu);
    s = mark_now(); q = slot_for(id, 2);
    if (q) q->s0 = s; else if (s >= 0) lvk_ts_release(s);
    pthread_mutex_unlock(&g_mu);
}

void gl31_vkq_end(GLuint id)
{
    vkq_t *q; int s;
    pthread_mutex_lock(&g_mu);
    q = find(id); s = mark_now();
    if (q && q->kind == 2) q->s1 = s; else if (s >= 0) lvk_ts_release(s);
    pthread_mutex_unlock(&g_mu);
}

int gl31_vkq_owns(GLuint id)
{
    int r;
    pthread_mutex_lock(&g_mu); r = find(id) != NULL; pthread_mutex_unlock(&g_mu);
    return r;
}

void gl31_vkq_get(GLuint id, int want_result, GLuint64* out)
{
    vkq_t *q; uint64_t t0 = 0, t1 = 0; int a, b = 1;
    *out = 0;
    pthread_mutex_lock(&g_mu);
    q = find(id);
    if (!q) { pthread_mutex_unlock(&g_mu); return; }
    if (q->kind == 2 && q->s1 < 0) {         /* ELAPSED sin End: no hay resultado aun */
        pthread_mutex_unlock(&g_mu); return;
    }
    a = lvk_ts_get(q->s0, &t0, want_result);
    if (q->kind == 2) b = lvk_ts_get(q->s1, &t1, want_result);
    if (!want_result) *out = (a == 1 && b == 1) ? 1 : 0;
    else if (a == 1 && b == 1) *out = (q->kind == 2) ? (t1 >= t0 ? t1 - t0 : 0) : t0;
    pthread_mutex_unlock(&g_mu);
}

void gl31_vkq_delete(GLuint id)
{
    vkq_t *q;
    pthread_mutex_lock(&g_mu);
    q = find(id); if (q) drop(q);
    pthread_mutex_unlock(&g_mu);
}

void gl31_vkq_reset(void)
{
    pthread_mutex_lock(&g_mu);
    while (g_nq) drop(&g_q[g_nq - 1]);
    free(g_q); g_q = NULL; g_cap = 0;
    if (g_state == 1) lvk_shutdown();        /* los slots de timestamp no sobreviven al contexto */
    g_state = 0; E.probed = 0; E.ok = 0;
    pthread_mutex_unlock(&g_mu);
}

void gl31_vkq_finish(void)
{
    uint64_t t;
    pthread_mutex_lock(&g_mu);
    for (int i = 0; i < g_nq; i++) {
        if (g_q[i].s0 >= 0) lvk_ts_get(g_q[i].s0, &t, 1);
        if (g_q[i].s1 >= 0) lvk_ts_get(g_q[i].s1, &t, 1);
    }
    pthread_mutex_unlock(&g_mu);
}

GLint64 gl31_vkq_now(void)
{
    uint64_t t = 0; int s = mark_now();
    if (s < 0) return 0;
    if (lvk_ts_get(s, &t, 1) != 1) t = 0;
    lvk_ts_release(s);
    return (GLint64)t;
}
