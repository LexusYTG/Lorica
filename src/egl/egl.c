
#include "egl.h"
#include "../gl/loader.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef AliasExport
#define AliasExport(name)   __attribute__((alias(name))) __attribute__((visibility("default")))
#endif

/* ---- Puente EGL -> GLX. EGLDisplay = Display*, EGLConfig = GLXFBConfig,
 * EGLContext = GLXContext, EGLSurface = XID (ventana) o XID|B_PB (pbuffer). ---- */

typedef unsigned long B_XID;
#define B_PB (1UL << 40)

static struct {
    void *(*GetFBConfigs)(void *, int, int *);
    int   (*GetFBConfigAttrib)(void *, void *, int, int *);
    void *(*CreateContextAttribsARB)(void *, void *, void *, int, const int *);
    void *(*CreateNewContext)(void *, void *, int, void *, int);
    void  (*DestroyContext)(void *, void *);
    int   (*MakeCurrent)(void *, B_XID, void *);
    void  (*SwapBuffers)(void *, B_XID);
    B_XID (*CreatePbuffer)(void *, void *, const int *);
    void  (*DestroyPbuffer)(void *, B_XID);
    void  (*QueryDrawable)(void *, B_XID, int, unsigned *);
    void  (*SwapIntervalEXT)(void *, B_XID, int);
    int   (*SwapIntervalSGI)(int);
    void  (*WaitGL)(void);
    void  (*WaitX)(void);
    void *(*XOpenDisplay)(const char *);
    int   (*XDefaultScreen)(void *);
    int   (*XGetGeometry)(void *, B_XID, B_XID *, int *, int *, unsigned *, unsigned *, unsigned *, unsigned *);
} G;

EGLBoolean gl4es_eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);

static pthread_once_t b_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t b_mx = PTHREAD_MUTEX_INITIALIZER;
static __thread EGLint b_err;
static __thread void *b_cur_dpy, *b_cur_ctx;
static __thread B_XID b_cur_draw, b_cur_read;

static void *b_sym(const char *name) {
    static void *lib, *x11;
    static void *(*gpa)(const char *);
    void *p = NULL;
    if (!lib) {
        lib = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
        if (lib) gpa = (void *(*)(const char *))dlsym(lib, "glXGetProcAddress");
    }
    if (name[0] == 'X') {
        if (lib) p = dlsym(lib, name);   /* libGL ya enlaza con X11 */
        if (p) return p;
        if (!x11) x11 = dlopen("libX11.so", RTLD_NOW | RTLD_GLOBAL);
        if (!x11) x11 = dlopen("libX11.so.6", RTLD_NOW | RTLD_GLOBAL);
        return x11 ? dlsym(x11, name) : NULL;
    }
    if (lib) p = dlsym(lib, name);
    if (!p && gpa) p = gpa(name);
    return p;
}

#define B_LOAD(f) G.f = (__typeof__(G.f))b_sym("glX" #f)
#define B_LOADX(f) G.X##f = (__typeof__(G.X##f))b_sym("X" #f)

static void b_init_real(void) {
    B_LOAD(GetFBConfigs); B_LOAD(GetFBConfigAttrib); B_LOAD(CreateContextAttribsARB);
    B_LOAD(CreateNewContext); B_LOAD(DestroyContext); B_LOAD(MakeCurrent); B_LOAD(SwapBuffers);
    B_LOAD(CreatePbuffer); B_LOAD(DestroyPbuffer); B_LOAD(QueryDrawable);
    G.SwapIntervalEXT = (void (*)(void *, B_XID, int))b_sym("glXSwapIntervalEXT");
    G.SwapIntervalSGI = (int (*)(int))b_sym("glXSwapIntervalSGI");
    B_LOAD(WaitGL); B_LOAD(WaitX);
    B_LOADX(OpenDisplay); B_LOADX(DefaultScreen); B_LOADX(GetGeometry);
}

static void b_init(void) {
    pthread_once(&b_once, b_init_real);
}

#define B_MAXCTX 64
static struct { void *ctx; void *fb; B_XID pb; } b_ctxs[B_MAXCTX];

static void *b_first_valid(void *dpy);

static void b_ctx_add(void *ctx, void *fb) {
    int i;
    pthread_mutex_lock(&b_mx);
    for (i = 0; i < B_MAXCTX; i++) if (!b_ctxs[i].ctx) { b_ctxs[i].ctx = ctx; b_ctxs[i].fb = fb; b_ctxs[i].pb = 0; break; }
    pthread_mutex_unlock(&b_mx);
}
static int b_ctx_find(void *ctx) {
    int i;
    for (i = 0; i < B_MAXCTX; i++) if (b_ctxs[i].ctx == ctx) return i;
    return -1;
}

static B_XID b_xid(EGLSurface s) { return ((B_XID)(uintptr_t)s) & ~B_PB; }
static int   b_is_pb(EGLSurface s) { return (((B_XID)(uintptr_t)s) & B_PB) != 0; }

static int b_glxattr(void *dpy, void *fb, int attr, int *v) {
    *v = 0;
    return G.GetFBConfigAttrib && G.GetFBConfigAttrib(dpy, fb, attr, v) == 0;
}

/* Devuelve 1 si el atributo EGL es conocido. */
static int b_cfg_attrib(void *dpy, void *fb, EGLint attr, EGLint *out) {
    int v = 0, g = -1;
    switch (attr) {
        case 0x3020: g = 2; break;        /* BUFFER_SIZE */
        case 0x3021: g = 11; break;       /* ALPHA_SIZE */
        case 0x3022: g = 10; break;       /* BLUE_SIZE */
        case 0x3023: g = 9; break;        /* GREEN_SIZE */
        case 0x3024: g = 8; break;        /* RED_SIZE */
        case 0x3025: g = 12; break;       /* DEPTH_SIZE */
        case 0x3026: g = 13; break;       /* STENCIL_SIZE */
        case 0x3028: g = 0x8013; break;   /* CONFIG_ID */
        case 0x3031: g = 100001; break;   /* SAMPLES */
        case 0x3032: g = 100000; break;   /* SAMPLE_BUFFERS */
        case 0x302E: g = 0x800B; break;   /* NATIVE_VISUAL_ID */
        case 0x3033: {                    /* SURFACE_TYPE */
            int t = 0, r = 0;
            b_glxattr(dpy, fb, 0x8010, &t);
            if (t & 1) r |= 4;            /* WINDOW */
            if (t & 2) r |= 2;            /* PIXMAP */
            if (t & 4) r |= 1;            /* PBUFFER */
            *out = r; return 1;
        }
        case 0x3040: case 0x3042: *out = 0x8; return 1;   /* RENDERABLE/CONFORMANT = OPENGL_BIT */
        case 0x302F: *out = 4; return 1;                  /* NATIVE_VISUAL_TYPE (TrueColor) */
        case 0x302D: *out = 1; return 1;                  /* NATIVE_RENDERABLE */
        case 0x302A: case 0x302B: *out = 4096; return 1;  /* MAX_PBUFFER_W/H */
        case 0x302C: *out = 4096 * 4096; return 1;        /* MAX_PBUFFER_PIXELS */
        case 0x3027: case 0x3034: *out = 0x3038; return 1;/* CAVEAT, TRANSPARENT_TYPE = NONE */
        case 0x303F: *out = 0x308E; return 1;             /* COLOR_BUFFER_TYPE = RGB */
        case 0x303C: *out = 10; return 1;                 /* MAX_SWAP_INTERVAL */
        case 0x3339: *out = 0x333A; return 1;             /* COLOR_COMPONENT_TYPE_EXT = FIXED */
        default: *out = 0; return 1;                      /* resto (LEVEL, BIND_TO_TEXTURE_*, ...) = 0 */
    }
    b_glxattr(dpy, fb, g, &v);
    *out = v;
    return 1;
}

static int b_cfg_valid(void *dpy, void *fb) {
    EGLint id = 0, r = 0, g = 0, b = 0;
    b_cfg_attrib(dpy, fb, 0x3028, &id);
    b_cfg_attrib(dpy, fb, 0x3024, &r);
    b_cfg_attrib(dpy, fb, 0x3023, &g);
    b_cfg_attrib(dpy, fb, 0x3022, &b);
    return id != 0 && (r + g + b) > 0;
}

static int b_cfg_match(void *dpy, void *fb, const EGLint *al) {
    int i;
    if (!al) return 1;
    for (i = 0; al[i] != 0x3038; i += 2) {
        EGLint a = al[i], want = al[i + 1], have = 0;
        if (want == -1) continue;   /* EGL_DONT_CARE */
        switch (a) {
            case 0x3033: case 0x3040: case 0x3042:
                if (!b_cfg_attrib(dpy, fb, a, &have) || (want & ~have)) return 0;
                break;
            case 0x3020: case 0x3021: case 0x3022: case 0x3023: case 0x3024:
            case 0x3025: case 0x3026: case 0x3031: case 0x3032:
                if (want > 0 && (!b_cfg_attrib(dpy, fb, a, &have) || have < want)) return 0;
                break;
            case 0x3028: case 0x3029: case 0x3027: case 0x3034: case 0x303F:
                if (!b_cfg_attrib(dpy, fb, a, &have) || have != want) return 0;
                break;
            default: break;
        }
    }
    return 1;
}

static int b_all_configs(void *dpy, void ***out) {
    int n = 0;
    void **l;
    b_init();
    if (!dpy || !G.GetFBConfigs || !G.XDefaultScreen) return 0;
    l = (void **)G.GetFBConfigs(dpy, G.XDefaultScreen(dpy), &n);
    if (!l || n <= 0) return 0;
    *out = l;
    return n;
}

static void *b_first_valid(void *dpy) {
    void **all = NULL;
    int n = b_all_configs(dpy, &all), i;
    for (i = 0; i < n; i++) if (b_cfg_valid(dpy, all[i])) return all[i];
    return NULL;
}

EGLint gl4es_eglGetError(void) {
    EGLint e = b_err ? b_err : 0x3000;
    b_err = 0x3000;
    return e;
}

EGLDisplay gl4es_eglGetDisplay(EGLNativeDisplayType display_id) {
    b_init();
    if (!display_id && G.XOpenDisplay) display_id = (EGLNativeDisplayType)G.XOpenDisplay(NULL);
    if (!display_id) b_err = 0x3008;   /* BAD_DISPLAY */
    return (EGLDisplay)display_id;
}

EGLBoolean gl4es_eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor) {
    if (!dpy) { b_err = 0x3008; return 0; }
    if (major) *major = 1;
    if (minor) *minor = 5;
    b_err = 0x3000;
    return 1;
}

EGLBoolean gl4es_eglTerminate(EGLDisplay dpy) {
    return 1;
}

const char * gl4es_eglQueryString(EGLDisplay dpy, EGLint name) {
    switch (name) {
        case 0x3053: return "Lorica";
        case 0x3054: return "1.5 Lorica";
        case 0x308D: return "OpenGL";
        case 0x3055:
            if (!dpy)
                return "EGL_KHR_client_get_all_proc_addresses EGL_EXT_platform_base EGL_EXT_platform_x11 EGL_KHR_platform_x11";
            return "EGL_KHR_create_context EGL_KHR_create_context_no_error EGL_KHR_surfaceless_context EGL_KHR_no_config_context EGL_KHR_get_all_proc_addresses EGL_KHR_fence_sync EGL_KHR_wait_sync EGL_KHR_image_base";
    }
    b_err = 0x300C;   /* BAD_PARAMETER */
    return NULL;
}

EGLBoolean gl4es_eglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config) {
    return gl4es_eglChooseConfig(dpy, NULL, configs, config_size, num_config);
}

EGLBoolean gl4es_eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config) {
    void **all = NULL;
    int n, i, out = 0;
    if (!num_config) { b_err = 0x300C; return 0; }
    n = b_all_configs(dpy, &all);
    for (i = 0; i < n; i++) {
        if (!b_cfg_valid(dpy, all[i]) || !b_cfg_match(dpy, all[i], attrib_list)) continue;
        if (configs && out < config_size) configs[out] = (EGLConfig)all[i];
        out++;
    }
    if (configs && out > config_size) out = config_size;
    *num_config = out;
    b_err = 0x3000;
    return 1;
}

EGLBoolean gl4es_eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value) {
    b_init();
    if (!config || !value) { b_err = 0x3005; return 0; }   /* BAD_CONFIG */
    if (!b_cfg_attrib(dpy, config, attribute, value)) { b_err = 0x3004; return 0; }  /* BAD_ATTRIBUTE */
    b_err = 0x3000;
    return 1;
}

EGLSurface gl4es_eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list) {
    B_XID w = (B_XID)(uintptr_t)win;
    if (!w) { b_err = 0x300B; return (EGLSurface)0; }      /* BAD_NATIVE_WINDOW */
    b_err = 0x3000;
    return (EGLSurface)(uintptr_t)w;
}

EGLSurface gl4es_eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list) {
    int pa[8], k = 0, i, w = 1, h = 1;
    B_XID pb;
    b_init();
    for (i = 0; attrib_list && attrib_list[i] != 0x3038; i += 2) {
        if (attrib_list[i] == 0x3057) w = attrib_list[i + 1];
        if (attrib_list[i] == 0x3056) h = attrib_list[i + 1];
    }
    pa[k++] = 0x8041; pa[k++] = w;     /* GLX_PBUFFER_WIDTH */
    pa[k++] = 0x8040; pa[k++] = h;     /* GLX_PBUFFER_HEIGHT */
    pa[k++] = 0;
    if (!G.CreatePbuffer || !(pb = G.CreatePbuffer(dpy, config, pa))) { b_err = 0x3003; return (EGLSurface)0; }
    b_err = 0x3000;
    return (EGLSurface)(uintptr_t)(pb | B_PB);
}

EGLSurface gl4es_eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list) {
    b_err = 0x300A;   /* BAD_NATIVE_PIXMAP */
    return (EGLSurface)0;
}

EGLBoolean gl4es_eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
    b_init();
    if (b_is_pb(surface) && G.DestroyPbuffer) G.DestroyPbuffer(dpy, b_xid(surface));
    b_err = 0x3000;
    return 1;
}

EGLBoolean gl4es_eglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value) {
    unsigned w = 0, h = 0, u;
    B_XID root;
    int x, y;
    b_init();
    if (attribute != 0x3057 && attribute != 0x3056) { b_err = 0x3004; return 0; }
    if (b_is_pb(surface)) {
        if (!G.QueryDrawable) { b_err = 0x300D; return 0; }
        G.QueryDrawable(dpy, b_xid(surface), attribute == 0x3057 ? 0x801D : 0x801E, &u);
        *value = (EGLint)u;
    } else {
        if (!G.XGetGeometry || !G.XGetGeometry(dpy, b_xid(surface), &root, &x, &y, &w, &h, &u, &u)) { b_err = 0x300D; return 0; }
        *value = (EGLint)(attribute == 0x3057 ? w : h);
    }
    b_err = 0x3000;
    return 1;
}

EGLBoolean gl4es_eglBindAPI(EGLenum api) {
    if (api != 0x30A2) { b_err = 0x300C; return 0; }
    return 1;
}

EGLenum gl4es_eglQueryAPI(void) {
    return 0x30A2;
}

EGLBoolean gl4es_eglWaitClient(void) {
    b_init();
    if (G.WaitGL) G.WaitGL();
    return 1;
}

EGLBoolean gl4es_eglReleaseThread(void) {
    b_init();
    if (b_cur_dpy && G.MakeCurrent) G.MakeCurrent(b_cur_dpy, 0, NULL);
    b_cur_ctx = NULL; b_cur_draw = b_cur_read = 0;
    return 1;
}

EGLSurface gl4es_eglCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list) {
    b_err = 0x300C;
    return (EGLSurface)0;
}

EGLBoolean gl4es_eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value) {
    return 1;
}

EGLBoolean gl4es_eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer) {
    return 1;
}

EGLBoolean gl4es_eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer) {
    return 1;
}

EGLBoolean gl4es_eglSwapInterval(EGLDisplay dpy, EGLint interval) {
    b_init();
    if (G.SwapIntervalEXT && b_cur_draw) G.SwapIntervalEXT(dpy, b_cur_draw, interval);
    else if (G.SwapIntervalSGI) G.SwapIntervalSGI(interval);
    return 1;
}

EGLContext gl4es_eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list) {
    int major = 0, minor = 0, flags = 0, profile = 0, i, k = 0, ga[10];
    void *ctx = NULL;
    for (i = 0; attrib_list && attrib_list[i] != 0x3038; i += 2) {
        switch (attrib_list[i]) {
            case 0x3098: major = attrib_list[i + 1]; break;   /* MAJOR_VERSION */
            case 0x30FB: minor = attrib_list[i + 1]; break;   /* MINOR_VERSION */
            case 0x30FC: flags = attrib_list[i + 1]; break;   /* FLAGS */
            case 0x30FD: profile = attrib_list[i + 1]; break; /* PROFILE_MASK */
        }
    }
    b_init();
    if (!config) config = b_first_valid(dpy);
    if (major && G.CreateContextAttribsARB) {
        ga[k++] = 0x2091; ga[k++] = major;
        ga[k++] = 0x2092; ga[k++] = minor;
        if (flags)   { ga[k++] = 0x2094; ga[k++] = flags; }
        if (profile) { ga[k++] = 0x9126; ga[k++] = profile; }
        ga[k] = 0;
        ctx = G.CreateContextAttribsARB(dpy, config, share_context, 1, ga);
    } else if (G.CreateNewContext) {
        ctx = G.CreateNewContext(dpy, config, 0x8014, share_context, 1);
    }
    if (!ctx) { b_err = 0x3009; return (EGLContext)0; }   /* BAD_MATCH */
    b_ctx_add(ctx, config);
    b_err = 0x3000;
    return (EGLContext)ctx;
}

EGLBoolean gl4es_eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
    int i;
    b_init();
    i = b_ctx_find(ctx);
    if (i >= 0) {
        if (b_ctxs[i].pb && G.DestroyPbuffer) G.DestroyPbuffer(dpy, b_ctxs[i].pb);
        pthread_mutex_lock(&b_mx);
        b_ctxs[i].ctx = NULL; b_ctxs[i].fb = NULL; b_ctxs[i].pb = 0;
        pthread_mutex_unlock(&b_mx);
    }
    if (G.DestroyContext && ctx) G.DestroyContext(dpy, ctx);
    return 1;
}

EGLBoolean gl4es_eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) {
    B_XID d = b_xid(draw);
    int ok;
    b_init();
    if (!G.MakeCurrent) { b_err = 0x3002; return 0; }
    if (ctx && !d) {   /* surfaceless: GLX no lo permite, usamos un pbuffer 1x1 por contexto */
        int i = b_ctx_find(ctx);
        if (i < 0) { b_err = 0x3006; return 0; }   /* BAD_CONTEXT */
        if (!b_ctxs[i].pb && G.CreatePbuffer) {
            int pa[5] = { 0x8041, 1, 0x8040, 1, 0 };
            b_ctxs[i].pb = G.CreatePbuffer(dpy, b_ctxs[i].fb, pa);
        }
        d = b_ctxs[i].pb;
        if (!d) { b_err = 0x3002; return 0; }
    }
    if (!ctx) ok = G.MakeCurrent(dpy, 0, NULL);
    else ok = G.MakeCurrent(dpy, d, ctx);
    if (!ok) { b_err = 0x3002; return 0; }   /* BAD_ACCESS */
    b_cur_dpy = dpy; b_cur_ctx = ctx; b_cur_draw = d; b_cur_read = b_xid(read);
    b_err = 0x3000;
    return 1;
}

EGLContext gl4es_eglGetCurrentContext(void) {
    return (EGLContext)b_cur_ctx;
}

EGLSurface gl4es_eglGetCurrentSurface(EGLint readdraw) {
    return (EGLSurface)(uintptr_t)(readdraw == 0x305A ? b_cur_read : b_cur_draw);
}

EGLDisplay gl4es_eglGetCurrentDisplay(void) {
    return (EGLDisplay)b_cur_dpy;
}

EGLDisplay gl4es_eglGetPlatformDisplay(EGLenum platform, void *native_display, const EGLAttrib *attrib_list) {
    if (platform != 0x31D5) { b_err = 0x300C; return (EGLDisplay)0; }   /* EGL_PLATFORM_X11_KHR */
    return gl4es_eglGetDisplay((EGLNativeDisplayType)native_display);
}

EGLBoolean gl4es_eglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value) {
    int i = b_ctx_find(ctx);
    if (!value) { b_err = 0x300C; return 0; }
    switch (attribute) {
        case 0x3028:   /* CONFIG_ID */
            if (i < 0) { b_err = 0x3006; return 0; }
            b_init();
            b_cfg_attrib(dpy, b_ctxs[i].fb, 0x3028, value);
            break;
        case 0x3097: *value = 0x30A2; break;   /* CONTEXT_CLIENT_TYPE = OPENGL_API */
        default: b_err = 0x3004; return 0;
    }
    b_err = 0x3000;
    return 1;
}

EGLBoolean gl4es_eglWaitGL(void) {
    return gl4es_eglWaitClient();
}

EGLBoolean gl4es_eglWaitNative(EGLint engine) {
    b_init();
    if (G.WaitX) G.WaitX();
    return 1;
}

EGLBoolean gl4es_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    b_init();
    if (!G.SwapBuffers || !surface) { b_err = 0x300D; return 0; }   /* BAD_SURFACE */
    G.SwapBuffers(dpy, b_xid(surface));
    b_err = 0x3000;
    return 1;
}

EGLBoolean gl4es_eglCopyBuffers(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target) {
    b_err = 0x300A;
    return 0;
}

static int b_sync_dummy;

EGLSyncKHR gl4es_eglCreateSyncKHR(EGLDisplay dpy, EGLenum type, const EGLint *attrib_list) {
    b_err = 0x3000;
    return (EGLSyncKHR)&b_sync_dummy;
}

EGLSyncKHR gl4es_eglDestroySyncKHR(EGLDisplay dpy, EGLSyncKHR sync) {
    return (EGLSyncKHR)(uintptr_t)1;
}

EGLint gl4es_eglClientWaitSyncKHR(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout) {
    b_init();
    if (G.WaitGL) G.WaitGL();   /* glFinish: lo mas cercano a esperar el fence */
    return 0x30F6;              /* EGL_CONDITION_SATISFIED */
}

NativePixmapType gl4es_egl_create_pixmap_ID_mapping(void *pixmap)
{
    LOAD_EGL(egl_create_pixmap_ID_mapping);
    return egl_egl_create_pixmap_ID_mapping(pixmap);
}

NativePixmapType gl4es_egl_destroy_pixmap_ID_mapping(int id)
{
    LOAD_EGL(egl_destroy_pixmap_ID_mapping);
    return egl_egl_destroy_pixmap_ID_mapping(id);
}

AliasExport(EGLint, eglGetError,,(void));
AliasExport(EGLDisplay, eglGetDisplay,,(EGLNativeDisplayType display_id));
AliasExport(EGLBoolean, eglInitialize,,(EGLDisplay dpy, EGLint *major, EGLint *minor));
AliasExport(EGLBoolean, eglTerminate,,(EGLDisplay dpy));
AliasExport(const char *, eglQueryString,,(EGLDisplay dpy, EGLint name));
AliasExport(EGLBoolean, eglGetConfigs,,(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config));
AliasExport(EGLBoolean, eglChooseConfig,,(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config));
AliasExport(EGLBoolean, eglGetConfigAttrib,,(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value));
AliasExport(EGLSurface, eglCreateWindowSurface,,(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list));
AliasExport(EGLSurface, eglCreatePbufferSurface,,(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list));
AliasExport(EGLSurface, eglCreatePixmapSurface,,(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list));
AliasExport(EGLBoolean, eglDestroySurface,,(EGLDisplay dpy, EGLSurface surface));
AliasExport(EGLBoolean, eglQuerySurface,,(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value));
AliasExport(EGLBoolean, eglBindAPI,,(EGLenum api));
AliasExport(EGLenum, eglQueryAPI,,(void));
AliasExport(EGLBoolean, eglWaitClient,,(void));
AliasExport(EGLBoolean, eglReleaseThread,,(void));
AliasExport(EGLSurface, eglCreatePbufferFromClientBuffer,,(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list));
AliasExport(EGLBoolean, eglSurfaceAttrib,,(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value));
AliasExport(EGLBoolean, eglBindTexImage,,(EGLDisplay dpy, EGLSurface surface, EGLint buffer));
AliasExport(EGLBoolean, eglReleaseTexImage,,(EGLDisplay dpy, EGLSurface surface, EGLint buffer));
AliasExport(EGLBoolean, eglSwapInterval,,(EGLDisplay dpy, EGLint interval));
AliasExport(EGLContext, eglCreateContext,,(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list));
AliasExport(EGLBoolean, eglDestroyContext,,(EGLDisplay dpy, EGLContext ctx));
AliasExport(EGLBoolean, eglMakeCurrent,,(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx));
AliasExport(EGLContext, eglGetCurrentContext,,(void));
AliasExport(EGLSurface, eglGetCurrentSurface,,(EGLint readdraw));
AliasExport(EGLDisplay, eglGetCurrentDisplay,,(void));
AliasExport(EGLDisplay, eglGetPlatformDisplay,, (EGLenum platform, void *native_display, const EGLAttrib *attrib_list));
AliasExport(EGLDisplay, eglGetPlatformDisplay, EXT, (EGLenum platform, void *native_display, const EGLAttrib *attrib_list));
AliasExport(EGLBoolean, eglQueryContext,,(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value));
AliasExport(EGLBoolean, eglWaitGL,,(void));
AliasExport(EGLBoolean, eglWaitNative,,(EGLint engine));
AliasExport(EGLBoolean, eglSwapBuffers,,(EGLDisplay dpy, EGLSurface surface));
AliasExport(EGLBoolean, eglCopyBuffers,,(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target));
AliasExport(EGLSyncKHR, eglCreateSyncKHR,,(EGLDisplay dpy, EGLenum type, const EGLint *attrib_list));
AliasExport(EGLSyncKHR, eglDestroySyncKHR,,(EGLDisplay dpy, EGLSyncKHR sync));
AliasExport(EGLint, eglClientWaitSyncKHR,,(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout));

AliasExport(NativePixmapType, egl_create_pixmap_ID_mapping,,(void *pixmap));
AliasExport(NativePixmapType, egl_destroy_pixmap_ID_mapping,,(int id));


/* ---- EGL 1.5 / EXT: con nombre C propio y simbolo exportado por asm label ---- */
#define B_EXP __attribute__((visibility("default")))

B_EXP void *b_eglCreateSync(void *d, unsigned type, const intptr_t *a) __asm__("eglCreateSync");
void *b_eglCreateSync(void *d, unsigned type, const intptr_t *a) { b_err = 0x3000; return (void *)&b_sync_dummy; }

B_EXP unsigned b_eglDestroySync(void *d, void *sync) __asm__("eglDestroySync");
unsigned b_eglDestroySync(void *d, void *sync) { return 1; }

B_EXP int b_eglClientWaitSync(void *d, void *sync, int flags, uint64_t timeout) __asm__("eglClientWaitSync");
int b_eglClientWaitSync(void *d, void *sync, int flags, uint64_t timeout) { b_init(); if (G.WaitGL) G.WaitGL(); return 0x30F6; }

B_EXP unsigned b_eglWaitSync(void *d, void *sync, int flags) __asm__("eglWaitSync");
unsigned b_eglWaitSync(void *d, void *sync, int flags) { return 1; }

B_EXP void *b_eglCreateImage(void *d, void *c, unsigned t, void *b, const intptr_t *a) __asm__("eglCreateImage");
void *b_eglCreateImage(void *d, void *c, unsigned t, void *b, const intptr_t *a) { b_err = 0x300C; return NULL; }

B_EXP unsigned b_eglDestroyImage(void *d, void *i) __asm__("eglDestroyImage");
unsigned b_eglDestroyImage(void *d, void *i) { return 1; }

/* En X11, native_window es un puntero a Window (no el Window en si). */
B_EXP void *b_eglCreatePlatformWindowSurface(void *d, void *cfg, void *native_window, const intptr_t *a) __asm__("eglCreatePlatformWindowSurface");
void *b_eglCreatePlatformWindowSurface(void *d, void *cfg, void *native_window, const intptr_t *a) {
    B_XID w;
    if (!native_window) { b_err = 0x300B; return NULL; }
    w = *(B_XID *)native_window;
    if (!w) { b_err = 0x300B; return NULL; }
    b_err = 0x3000;
    return (void *)(uintptr_t)w;
}

B_EXP void *b_eglCreatePlatformWindowSurfaceEXT(void *d, void *cfg, void *native_window, const int *a) __asm__("eglCreatePlatformWindowSurfaceEXT");
void *b_eglCreatePlatformWindowSurfaceEXT(void *d, void *cfg, void *native_window, const int *a) {
    return b_eglCreatePlatformWindowSurface(d, cfg, native_window, NULL);
}

B_EXP void *b_eglCreatePlatformPixmapSurface(void *d, void *cfg, void *np, const intptr_t *a) __asm__("eglCreatePlatformPixmapSurface");
void *b_eglCreatePlatformPixmapSurface(void *d, void *cfg, void *np, const intptr_t *a) { b_err = 0x300A; return NULL; }

/* Lo usa lookup.c: devuelve la implementacion propia, nunca la de Android. */
B_EXP void *lorica_egl_lookup(const char *n) {
    if (!n) return NULL;
    if (!strcmp(n, "eglCreateSync")) return (void *)b_eglCreateSync;
    if (!strcmp(n, "eglDestroySync")) return (void *)b_eglDestroySync;
    if (!strcmp(n, "eglClientWaitSync")) return (void *)b_eglClientWaitSync;
    if (!strcmp(n, "eglWaitSync")) return (void *)b_eglWaitSync;
    if (!strcmp(n, "eglCreateImage")) return (void *)b_eglCreateImage;
    if (!strcmp(n, "eglDestroyImage")) return (void *)b_eglDestroyImage;
    if (!strcmp(n, "eglCreatePlatformWindowSurface")) return (void *)b_eglCreatePlatformWindowSurface;
    if (!strcmp(n, "eglCreatePlatformWindowSurfaceEXT")) return (void *)b_eglCreatePlatformWindowSurfaceEXT;
    if (!strcmp(n, "eglCreatePlatformPixmapSurface")) return (void *)b_eglCreatePlatformPixmapSurface;
    return NULL;
}
