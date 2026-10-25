/* test_gl31_bridge.c - prueba de gl31_bridge.c con un EGL falso (sin GPU ni X11).
 * Cubre: eleccion de contexto ES 3.x / ES2, caida de minor, config sin bit ES3, retain/release y
 * el filtro de gl4es_gl31_bridge_ensure (solo arranca GLADIATOR con un contexto ES3 actual) y el despacho
 * de glGetProcAddress (gl4es_gl31_proc_lookup).
 * gl31_init / gl31_shutdown / gl31_set_max_version se sustituyen por contadores.
 *
 * Compilar (desde la raiz del proyecto):
 *   gcc -std=gnu99 -Wall -DNO_GBM -DEGL_NO_X11 -DDEFAULT_ES=2 -Iinclude -Isrc -Isrc/gl -Isrc/glx \
 *       -Isrc/gl/wrap -Isrc/gl/math src/glx/gl31_bridge.c src/glx/tests/test_gl31_bridge.c -o /tmp/t_bridge && /tmp/t_bridge
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "gl31_bridge.h"
#include "../gl/loader.h"
#include "../gl/init.h"
#include "hardext.h"

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALLO linea %d: %s\n", __LINE__, #c); } } while (0)

/* ---- piezas de gl4es que el puente usa y no queremos enlazar ---- */
globals4es_t globals4es;
hardext_t hardext;
void *gles = (void*)1, *egl = (void*)1;
void *bcm_host, *vcos, *gbm, *drm;
void* (APIENTRY_GL4ES *gles_getProcAddress)(const char *name);
void LogPrintf(const char *fmt, ...) { (void)fmt; }
void LogFPrintf(FILE *fp, const char *fmt, ...) { (void)fp; (void)fmt; }

/* ---- EGL falso ---- */
static int max_es_minor = 2;       /* el driver soporta hasta ES 3.<max_es_minor>; -1: no soporta ES3 */
static int minor_attr_ok = 1;      /* acepta EGL_CONTEXT_MINOR_VERSION_KHR */
static int cfg_rt[4];              /* RENDERABLE_TYPE por config */
static int create_calls, last_client_ver, last_minor;
static int current_ver = 0;        /* version del contexto EGL actual (0 = ninguno) */
static EGLContext cur_ctx = EGL_NO_CONTEXT;

static EGLContext f_create(EGLDisplay d, EGLConfig c, EGLContext s, const EGLint* a) {
    int ver = 1, minor = 0, i;
    (void)d; (void)s;
    create_calls++;
    for (i = 0; a[i] != EGL_NONE; i += 2) {
        if (a[i] == EGL_CONTEXT_CLIENT_VERSION) ver = a[i+1];
        else if (a[i] == 0x30FB) { if (!minor_attr_ok) return EGL_NO_CONTEXT; minor = a[i+1]; }
    }
    last_client_ver = ver; last_minor = minor;
    if (ver == 3) {
        if (!(cfg_rt[(intptr_t)c] & EGL_OPENGL_ES3_BIT)) return EGL_NO_CONTEXT;
        if (max_es_minor < 0 || minor > max_es_minor) return EGL_NO_CONTEXT;
    }
    return (EGLContext)(intptr_t)(0x1000 + ver * 16 + minor);
}
static EGLBoolean f_cfgattr(EGLDisplay d, EGLConfig c, EGLint attr, EGLint* v) {
    (void)d;
    if (attr == EGL_RENDERABLE_TYPE) { *v = cfg_rt[(intptr_t)c]; return EGL_TRUE; }
    return EGL_FALSE;
}
static EGLContext f_curctx(void) { return cur_ctx; }
static EGLDisplay f_curdpy(void) { return cur_ctx ? (EGLDisplay)1 : EGL_NO_DISPLAY; }
static EGLBoolean f_qctx(EGLDisplay d, EGLContext c, EGLint attr, EGLint* v) {
    (void)d; (void)c;
    if (attr == EGL_CONTEXT_CLIENT_VERSION) { *v = current_ver; return EGL_TRUE; }
    return EGL_FALSE;
}
static void* f_getproc(const char* n) { (void)n; return NULL; }

void* APIENTRY_GL4ES proc_address(void *lib, const char *name) {
    (void)lib;
    if (!strcmp(name, "eglCreateContext"))      return (void*)f_create;
    if (!strcmp(name, "eglGetConfigAttrib"))    return (void*)f_cfgattr;
    if (!strcmp(name, "eglGetCurrentContext"))  return (void*)f_curctx;
    if (!strcmp(name, "eglGetCurrentDisplay"))  return (void*)f_curdpy;
    if (!strcmp(name, "eglQueryContext"))       return (void*)f_qctx;
    if (!strcmp(name, "eglGetProcAddress"))     return (void*)f_getproc;
    return NULL;
}

/* ---- GLADIATOR sustituido ---- */
static int init_calls, shutdown_calls, init_result, max_a, max_b;
int  gl31_init(void* (*loader)(const char*)) { (void)loader; init_calls++; return init_result; }
void gl31_shutdown(void) { shutdown_calls++; }
void gl31_set_max_version(int a, int b) { max_a = a; max_b = b; }

/* despacho (paso 3): tabla de GLADIATOR y contexto GLX actual falsos */
static int cur_is_gl31 = 0;
int gl4es_glx_current_is_gl31(void) { return cur_is_gl31; }
static int fn_bind_buffer, fn_draw, fn_tex_ext;
void* gl31_get_proc_address(const char* n)
{
    if (!n) return NULL;
    if (!strcmp(n, "glBindBuffer")) return &fn_bind_buffer;
    if (!strcmp(n, "glDrawArrays")) return &fn_draw;
    if (!strcmp(n, "glTextureParameteri")) return &fn_tex_ext;   /* DSA core: firma distinta de ...EXT */
    return NULL;
}

#define MK(gl, es2only) \
    do { create_calls = 0; idx = 0; \
         ctx = gl4es_gl31_create_backend_context((EGLDisplay)1, cfgs, 4, &idx, EGL_NO_CONTEXT, (gl), (es2only), &maj, &min); } while (0)

int main(void)
{
    EGLConfig cfgs[4] = { (EGLConfig)(intptr_t)0, (EGLConfig)(intptr_t)1, (EGLConfig)(intptr_t)2, (EGLConfig)(intptr_t)3 };
    EGLContext ctx; int idx, maj, min;
    int i;

    unsetenv("LORICA_GL_MAX_VERSION");
    hardext.esversion = 2;
    for (i = 0; i < 4; i++) cfg_rt[i] = EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT;

    /* 1. contexto legacy (gl_version 0 o < 31): ES2, un solo intento, sin tocar nada mas */
    MK(0, 0);
    CHECK(ctx != EGL_NO_CONTEXT && maj == 2 && min == 0 && create_calls == 1 && last_client_ver == 2);
    MK(30, 0);
    CHECK(maj == 2 && create_calls == 1);
    /* perfil ES2 pedido por la app: ES2 aunque pida 4.2 */
    MK(42, 1);
    CHECK(maj == 2 && create_calls == 1);
    /* backend ES1: nunca ES3 */
    hardext.esversion = 1;
    MK(42, 0);
    CHECK(maj == 1 && create_calls == 1 && last_client_ver == 1);
    hardext.esversion = 2;

    /* 2. GL 4.2 sobre driver ES 3.2: pide 3.2 a la primera */
    max_es_minor = 2;
    MK(42, 0);
    CHECK(ctx != EGL_NO_CONTEXT && maj == 3 && min == 2 && create_calls == 1 && last_minor == 2);

    /* paso 4: con LIBGL_FB=2 (usefbo) no se piden contextos ES3, aunque la app pida GL 4.2 */
    globals4es.usefbo = 1;
    MK(42, 0);
    CHECK(ctx != EGL_NO_CONTEXT && maj == 2 && create_calls == 1 && last_client_ver == 2);
    MK(31, 0);
    CHECK(maj == 2 && create_calls == 1);
    globals4es.usefbo = 0;
    MK(42, 0);
    CHECK(maj == 3 && min == 2 && create_calls == 1);              /* sin usefbo vuelve a ES 3.2 */


    /* 3. driver ES 3.1: 3.2 falla, 3.1 sirve; luego ya no vuelve a probar 3.2 */
    max_es_minor = 1;
    MK(33, 0);
    CHECK(maj == 3 && min == 1 && create_calls == 2);
    MK(33, 0);
    CHECK(maj == 3 && min == 1 && create_calls == 1);

    /* 4. config actual sin bit ES3: usa otra de la lista que si lo tenga */
    cfg_rt[0] = EGL_OPENGL_ES2_BIT;
    cfg_rt[1] = EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT;
    create_calls = 0; idx = 0;
    ctx = gl4es_gl31_create_backend_context((EGLDisplay)1, cfgs, 4, &idx, EGL_NO_CONTEXT, 31, 0, &maj, &min);
    CHECK(maj == 3 && idx == 1);

    /* 5. ninguna config con bit ES3: cae a ES2, el indice no cambia, y NO se marca ES3 como roto */
    for (i = 0; i < 4; i++) cfg_rt[i] = EGL_OPENGL_ES2_BIT;
    create_calls = 0; idx = 2;
    ctx = gl4es_gl31_create_backend_context((EGLDisplay)1, cfgs, 4, &idx, EGL_NO_CONTEXT, 42, 0, &maj, &min);
    CHECK(ctx != EGL_NO_CONTEXT && maj == 2 && idx == 2 && create_calls == 1 && last_client_ver == 2);
    for (i = 0; i < 4; i++) cfg_rt[i] = EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT;
    MK(42, 0);
    CHECK(maj == 3);

    /* 6. driver sin EGL_KHR_create_context (no acepta minor) pero con ES 3.0 */
    minor_attr_ok = 0; max_es_minor = 0;
    MK(31, 0);       /* intenta 3.1 (rechazado) y baja a 3.0 sin atributo de minor */
    CHECK(maj == 3 && min == 0);
    minor_attr_ok = 1;

    /* 7. driver sin ES3: cae a ES2 y deja de intentar (un solo create en adelante) */
    max_es_minor = -1;
    MK(42, 0);
    CHECK(ctx != EGL_NO_CONTEXT && maj == 2);
    MK(42, 0);
    CHECK(maj == 2 && create_calls == 1);

    /* 8. ensure: sin contextos contados no hace nada */
    init_calls = shutdown_calls = 0; init_result = 0;
    cur_ctx = (EGLContext)0x1; current_ver = 3;
    CHECK(gl4es_gl31_bridge_ensure() == 0 && init_calls == 0);

    /* 9. ensure con contexto contado pero el actual es ES2: no arranca */
    gl4es_gl31_bridge_retain();
    current_ver = 2;
    CHECK(gl4es_gl31_bridge_ensure() == 0 && init_calls == 0);
    cur_ctx = EGL_NO_CONTEXT;
    CHECK(gl4es_gl31_bridge_ensure() == 0 && init_calls == 0);

    /* 10. contexto ES3 actual: arranca una sola vez y fija el tope con LIBGL_GL */
    cur_ctx = (EGLContext)0x1; current_ver = 3;
    globals4es.gl = 42;
    CHECK(gl4es_gl31_bridge_ensure() == 1 && init_calls == 1 && gl4es_gl31_bridge_ready());
    CHECK(max_a == 4 && max_b == 2);
    CHECK(gl4es_gl31_bridge_ensure() == 1 && init_calls == 1);

    /* 11. dos contextos: solo el ultimo release apaga GLADIATOR */
    gl4es_gl31_bridge_retain();
    gl4es_gl31_bridge_release();
    CHECK(shutdown_calls == 0 && gl4es_gl31_bridge_ready());
    gl4es_gl31_bridge_release();
    CHECK(shutdown_calls == 1 && !gl4es_gl31_bridge_ready());

    /* 12. reinicio tras apagar */
    gl4es_gl31_bridge_retain();
    CHECK(gl4es_gl31_bridge_ensure() == 1 && init_calls == 2);
    gl4es_gl31_bridge_release();

    /* 13. gl31_init falla: no se reintenta en cada llamada; tras liberar todo se puede reintentar */
    init_result = -1; init_calls = 0;
    gl4es_gl31_bridge_retain();
    CHECK(gl4es_gl31_bridge_ensure() == 0 && init_calls == 1);
    CHECK(gl4es_gl31_bridge_ensure() == 0 && init_calls == 1);
    gl4es_gl31_bridge_release();
    init_result = 0;
    gl4es_gl31_bridge_retain();
    CHECK(gl4es_gl31_bridge_ensure() == 1 && init_calls == 2);
    gl4es_gl31_bridge_release();

    /* 14. LORICA_GL_MAX_VERSION manda sobre LIBGL_GL */
    setenv("LORICA_GL_MAX_VERSION", "3.3", 1);
    max_a = max_b = 0;
    gl4es_gl31_bridge_retain();
    CHECK(gl4es_gl31_bridge_ensure() == 1 && max_a == 0 && max_b == 0);
    gl4es_gl31_bridge_release();

    /* 15. despacho de glGetProcAddress (paso 3). Estado: LORICA_GL_MAX_VERSION ya fijada, sin efecto aqui */
    init_result = 0;
    cur_is_gl31 = 1;
    CHECK(gl4es_gl31_proc_lookup("glDrawArrays") == NULL);         /* GLADIATOR no iniciado */
    gl4es_gl31_bridge_retain();
    CHECK(gl4es_gl31_bridge_ensure() == 1);
    CHECK(gl4es_gl31_proc_lookup("glDrawArrays") == &fn_draw);     /* contexto core GL3.1+ y GLADIATOR listo */
    CHECK(gl4es_gl31_proc_lookup("glBindBufferARB") == &fn_bind_buffer);   /* ARB -> nombre base */
    CHECK(gl4es_gl31_proc_lookup("glTextureParameteriEXT") == NULL);       /* EXT no se traduce */
    CHECK(gl4es_gl31_proc_lookup("glBegin") == NULL);              /* no esta en GLADIATOR: sigue GL4ES */
    CHECK(gl4es_gl31_proc_lookup("ARB") == NULL && gl4es_gl31_proc_lookup("") == NULL);
    CHECK(gl4es_gl31_proc_lookup(NULL) == NULL);
    cur_is_gl31 = 0;                                               /* contexto legacy/compat/ES2 actual */
    CHECK(gl4es_gl31_proc_lookup("glDrawArrays") == NULL);
    CHECK(gl4es_gl31_proc_lookup("glBindBufferARB") == NULL);
    cur_is_gl31 = 1;
    gl4es_gl31_bridge_release();                                   /* se apaga GLADIATOR */
    CHECK(gl4es_gl31_proc_lookup("glDrawArrays") == NULL);

    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
