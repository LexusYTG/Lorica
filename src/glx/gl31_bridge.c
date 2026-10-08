/* gl31_bridge.c - puente GL4ES <-> GLADIATOR (pasos 1 a 3). Ver gl31_bridge.h.
 *
 * No se incluye gl31.h: sus tipos GLES chocan con los de GL4ES; se declaran solo las 3 funciones usadas. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gl31_bridge.h"
#include "hardext.h"
#include "../gl/loader.h"
#include "../gl/init.h"
#include "../gl/logs.h"

#ifndef NOEGL

#ifndef EGL_CONTEXT_MINOR_VERSION_KHR
#define EGL_CONTEXT_MINOR_VERSION_KHR 0x30FB
#endif
#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x00000040
#endif

extern int  gl31_init(void* (*loader)(const char* name));
extern void gl31_shutdown(void);
extern void gl31_set_max_version(int major, int minor);
extern void* gl31_get_proc_address(const char* name);
extern int  gl4es_glx_current_is_gl31(void);   /* glx.c */

/* Mayor minor de ES 3.x que se intentara pedir. Baja cuando el driver rechaza uno;
 * -1 = ES 3.x no disponible (se deja de intentar). Solo se baja si la config SI tenia el bit ES3,
 * para no culpar a la version de un fallo que es de la config. */
static int es3_try_minor = 2;

static int gl31_live   = 0;   /* contextos ES3 contados */
static int gl31_ready  = 0;   /* gl31_init OK */
static int gl31_failed = 0;   /* gl31_init fallo: no reintentar en cada MakeCurrent */

static int config_has_es3(EGLDisplay dpy, EGLConfig cfg)
{
    LOAD_EGL(eglGetConfigAttrib);
    EGLint rt = 0;
    if (!egl_eglGetConfigAttrib) return 0;
    if (!egl_eglGetConfigAttrib(dpy, cfg, EGL_RENDERABLE_TYPE, &rt)) return 0;
    return (rt & EGL_OPENGL_ES3_BIT) ? 1 : 0;
}

EGLContext gl4es_gl31_create_backend_context(EGLDisplay dpy, EGLConfig* configs, int nconfigs, int* cfgidx,
                                             EGLContext shared, int gl_version, int es2only,
                                             int* es_major, int* es_minor)
{
    static const EGLint attrib_es2[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    static const EGLint attrib_none[] = { EGL_NONE };
    EGLContext ctx = EGL_NO_CONTEXT;
    int idx = cfgidx ? *cfgidx : 0;

    LOAD_EGL(eglCreateContext);

    if (es_major) *es_major = (hardext.esversion == 1) ? 1 : 2;
    if (es_minor) *es_minor = (hardext.esversion == 1) ? 1 : 0;

    /* Paso 4, framebuffer por defecto. Con LIBGL_FB=2 (globals4es.usefbo) el "framebuffer 0" de GL4ES es un
     * FBO propio (main FBO) que se vuelca en cada glXSwapBuffers con el pipeline de GL4ES (shaders, VAO,
     * texturas): pisaria el estado que GLADIATOR cree tener y GLADIATOR dibujaria en el framebuffer real,
     * no en el main FBO. Sin usefbo el framebuffer por defecto es la superficie EGL y GLADIATOR ya lo trata
     * (GL_BACK*, sin attach, etc.), asi que solo ese caso se deja en ES2. */
    if (globals4es.usefbo && !es2only && gl_version >= GL31_BRIDGE_MIN_GL_VERSION && hardext.esversion >= 2)
        LOGE("LIBGL: GL %d.%d pedido con LIBGL_FB=2 (main FBO): contexto ES2 (GLADIATOR inactivo)\n",
             gl_version / 10, gl_version % 10);

    if (hardext.esversion >= 2 && !es2only && !globals4es.usefbo &&
        gl_version >= GL31_BRIDGE_MIN_GL_VERSION && es3_try_minor >= 0) {
        int i, found = -1;
        /* config con bit ES3: la actual si lo tiene, si no la primera de la lista que lo tenga */
        if (idx >= 0 && idx < nconfigs && config_has_es3(dpy, configs[idx])) {
            found = idx;
        } else {
            for (i = 0; i < nconfigs; i++)
                if (config_has_es3(dpy, configs[i])) { found = i; break; }
        }
        if (found < 0) {
            LOGE("LIBGL: GL %d.%d pedido pero ninguna config EGL tiene el bit ES3: contexto ES2 (GLADIATOR inactivo)\n",
                 gl_version / 10, gl_version % 10);
        } else {
            int minor;
            for (minor = es3_try_minor; minor >= 0; minor--) {
                EGLint attr[8];
                int n = 0;
                attr[n++] = EGL_CONTEXT_CLIENT_VERSION; attr[n++] = 3;
                /* ES 3.0 se pide sin atributo de minor: no exige EGL_KHR_create_context */
                if (minor > 0) { attr[n++] = EGL_CONTEXT_MINOR_VERSION_KHR; attr[n++] = minor; }
                attr[n++] = EGL_NONE;
                ctx = egl_eglCreateContext(dpy, configs[found], shared, attr);
                if (ctx != EGL_NO_CONTEXT) {
                    if (cfgidx) *cfgidx = found;
                    if (es_major) *es_major = 3;
                    if (es_minor) *es_minor = minor;
                    es3_try_minor = minor;      /* la proxima vez no se vuelven a probar las que fallaron */
                    LOGD("LIBGL: contexto de backend ES 3.%d para GL %d.%d\n", minor, gl_version / 10, gl_version % 10);
                    if ((gl_version >= 43 && minor < 2) || (gl_version >= 40 && minor < 1))
                        LOGD("LIBGL: aviso: GL %d.%d necesita ES 3.%d; con ES 3.%d GLADIATOR anunciara menos\n",
                             gl_version / 10, gl_version % 10, gl_version >= 43 ? 2 : 1, minor);
                    return ctx;
                }
            }
            /* ni ES 3.0 se pudo con una config que tiene el bit ES3: ES3 no esta disponible */
            es3_try_minor = -1;
            LOGE("LIBGL: el driver no crea contextos ES 3.x: contexto ES2 (GLADIATOR inactivo)\n");
        }
    }

    /* camino de siempre */
    ctx = egl_eglCreateContext(dpy, configs[idx], shared,
                               (hardext.esversion == 1) ? attrib_none : attrib_es2);
    return ctx;
}

/* ---- arranque de GLADIATOR ---- */

/* Cargador del backend: primero la libreria GLES (simbolos core), luego eglGetProcAddress
 * (las variantes EXT/OES/KHR que gl31_init prueba con sufijo no estan exportadas). */
static void* bridge_loader(const char* name)
{
    void* p = NULL;
    if (gles) p = proc_address(gles, name);
    if (!p) {
        LOAD_EGL(eglGetProcAddress);
        if (egl_eglGetProcAddress) p = (void*)egl_eglGetProcAddress(name);
    }
    return p;
}

void gl4es_gl31_bridge_retain(void)
{
    gl31_live++;
}

void gl4es_gl31_bridge_release(void)
{
    if (gl31_live > 0) gl31_live--;
    if (gl31_live == 0) {
        if (gl31_ready) { gl31_shutdown(); gl31_ready = 0; }
        gl31_failed = 0;
    }
}

int gl4es_gl31_bridge_ready(void)
{
    return gl31_ready;
}

int gl4es_gl31_bridge_ensure(void)
{
    EGLContext ctx;
    EGLDisplay dpy;
    EGLint ver = 0;

    if (gl31_ready) return 1;
    if (gl31_failed || gl31_live <= 0) return 0;

    LOAD_EGL(eglGetCurrentContext);
    LOAD_EGL(eglGetCurrentDisplay);
    LOAD_EGL(eglQueryContext);
    if (!egl_eglGetCurrentContext || !egl_eglGetCurrentDisplay || !egl_eglQueryContext) return 0;

    ctx = egl_eglGetCurrentContext();
    dpy = egl_eglGetCurrentDisplay();
    if (ctx == EGL_NO_CONTEXT || dpy == EGL_NO_DISPLAY) return 0;
    /* gl31_init consulta GL_MAJOR_VERSION / glGetStringi: solo vale con un contexto ES3 actual */
    if (!egl_eglQueryContext(dpy, ctx, EGL_CONTEXT_CLIENT_VERSION, &ver) || ver < 3) return 0;

    if (gl31_init(bridge_loader) != 0) {
        gl31_failed = 1;
        LOGE("LIBGL: gl31_init fallo (backend sin funciones ES 3.0 requeridas): GLADIATOR inactivo\n");
        return 0;
    }
    /* El tope de GLADIATOR sigue a LIBGL_GL, salvo que el usuario fije LORICA_GL_MAX_VERSION
     * (gl31_init ya la leyo). */
    if (!getenv("LORICA_GL_MAX_VERSION") && globals4es.gl >= GL31_BRIDGE_MIN_GL_VERSION)
        gl31_set_max_version(globals4es.gl / 10, globals4es.gl % 10);

    gl31_ready = 1;
    LOGD("LIBGL: GLADIATOR iniciado (backend ES 3.x)\n");
    return 1;
}

/* ---- despacho (paso 3): glGetProcAddress ---- */

/* Devuelve la funcion gl31_* para `name` si el contexto actual es un contexto core GL>=3.1 con
 * GLADIATOR iniciado; NULL en cualquier otro caso (el llamador sigue con la tabla de GL4ES).
 * Los nombres con sufijo ARB se resuelven al nombre base si este esta en la tabla de GLADIATOR
 * (glBindBufferARB -> glBindBuffer): mezclar nombres ARB de GL4ES con nombres core de GLADIATOR
 * dejaria el estado de buffers/texturas repartido en dos sitios. El sufijo EXT NO se traduce:
 * en varias funciones (DSA) cambia la firma. */
void* gl4es_gl31_proc_lookup(const char* name)
{
    void* p;
    size_t n;
    if (!name || !gl31_ready || !gl4es_glx_current_is_gl31()) return NULL;
    p = gl31_get_proc_address(name);
    if (p) return p;
    n = strlen(name);
    if (n > 3 && n < 96 && strcmp(name + n - 3, "ARB") == 0) {
        char base[96];
        memcpy(base, name, n - 3);
        base[n - 3] = '\0';
        return gl31_get_proc_address(base);
    }
    return NULL;
}

/* ---- despacho indirecto de los simbolos GL exportados (paso 3b) ----
 * Cada simbolo exportado salta a traves de un slot (ver _AliasExport_ en gl/attributes.h). Aqui se
 * repuntan: a GLADIATOR con un contexto core GL>=3.1, a GL4ES en cualquier otro caso. Las entradas
 * estan en la seccion lorica_disp. Se desactiva con -DLORICA_NO_DISPATCH. */
#if defined(__linux__) && !defined(LORICA_NO_DISPATCH) && (defined(__x86_64__) || defined(__aarch64__))
typedef struct { const char* name; void** slot; void* def; } lorica_disp_t;
extern lorica_disp_t __start_lorica_disp[] __attribute__((weak, visibility("hidden")));
extern lorica_disp_t __stop_lorica_disp[]  __attribute__((weak, visibility("hidden")));
static int disp_mode = -1;   /* -1 desconocido, 0 GL4ES, 1 GLADIATOR */

void gl4es_gl31_dispatch_update(void)
{
    lorica_disp_t* e;
    int mode = (gl31_ready && gl4es_glx_current_is_gl31()) ? 1 : 0;
    if (mode == disp_mode) return;
    for (e = __start_lorica_disp; e < __stop_lorica_disp; e++) {
        void* p = mode ? gl4es_gl31_proc_lookup(e->name) : NULL;
        *e->slot = p ? p : e->def;
    }
    disp_mode = mode;
}
#else
void gl4es_gl31_dispatch_update(void) {}
#endif

#endif /* NOEGL */
