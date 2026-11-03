/* ctx.h - contexto GLES 3.2 sin ventana (EGL surfaceless de Mesa) para las pruebas. Comprueba cada paso. */
#ifndef HYBRID_CTX_H
#define HYBRID_CTX_H
#include <stdio.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
static void *ctx_loader(const char *n) { return (void *)eglGetProcAddress(n); }
static int ctx_make(void)
{
    PFNEGLGETPLATFORMDISPLAYEXTPROC gpd = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    EGLDisplay d = gpd ? gpd(0x31DD /*EGL_PLATFORM_SURFACELESS_MESA*/, EGL_DEFAULT_DISPLAY, NULL) : eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint M, m, n = 0; EGLConfig c = NULL; EGLContext x;
    const EGLint ca[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, 0, EGL_NONE };
    const EGLint xa[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE };
    if (d == EGL_NO_DISPLAY || !eglInitialize(d, &M, &m)) { fprintf(stderr, "ctx: eglInitialize fallo\n"); return 0; }
    eglBindAPI(EGL_OPENGL_ES_API);
    if (!eglChooseConfig(d, ca, &c, 1, &n) || n < 1) { fprintf(stderr, "ctx: sin EGLConfig\n"); return 0; }
    x = eglCreateContext(d, c, EGL_NO_CONTEXT, xa);
    if (x == EGL_NO_CONTEXT) { fprintf(stderr, "ctx: eglCreateContext fallo 0x%x\n", eglGetError()); return 0; }
    if (!eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, x)) { fprintf(stderr, "ctx: eglMakeCurrent fallo\n"); return 0; }
    return 1;
}
#endif
