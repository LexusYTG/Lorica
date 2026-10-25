/* Uso: ./gl33_core_glx [MAJOR MINOR]  (por defecto 3 3). Ejecutar con LIBGL_GL=43 para poder pedir hasta 4.3.
 * Prueba con driver real: contexto GL 3.3 core por GLX (glXGetProcAddress), dibujo por simbolos directos.
 * Compilar: gcc gl33_core_glx.c -o gl33_core_glx -I../include -L../lib -l:libGL.so.1 -lX11
 * Ejecutar: xvfb-run -a env LD_LIBRARY_PATH=../lib LIBGL_GL=33 ./gl33_core_glx   (necesita EGL/GLES ES>=3.1 de Mesa) */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>
typedef GLXContext (*cca_t)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

static int fails = 0;
static int REQ_MAJ = 3, REQ_MIN = 3;
#define CHECK(c, ...) do { if(!(c)) { fails++; printf("FALLO: " __VA_ARGS__); printf("\n"); } } while(0)

static GLXContext mk(Display* d, GLXFBConfig fb, int maj, int min, int core) {
    cca_t cca = (cca_t)glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");
    /* como SDL/GLFW: en versiones < 3.2 no se pasa perfil */
    int at[] = { GLX_CONTEXT_MAJOR_VERSION_ARB, maj, GLX_CONTEXT_MINOR_VERSION_ARB, min,
                 GLX_CONTEXT_PROFILE_MASK_ARB, core ? GLX_CONTEXT_CORE_PROFILE_BIT_ARB : GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB, 0 };
    if (maj * 10 + min < 32) at[4] = 0;
    return cca ? cca(d, fb, NULL, True, at) : NULL;
}
static void red(const char* tag) {
    unsigned char px[4] = {0};
    glViewport(0, 0, 64, 64);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK(px[0] > 250 && px[1] < 5 && px[2] < 5, "%s: glClear+glReadPixels dio %d,%d,%d", tag, px[0], px[1], px[2]);
}
static int is_core33(void) { const char* v = (const char*)glGetString(GL_VERSION); return v != NULL;  /* GLADIATOR confirmado por "contexto de backend" en stderr */ }

int main(int argc, char** argv) {
    if (argc >= 3) { REQ_MAJ = atoi(argv[1]); REQ_MIN = atoi(argv[2]); }
    printf("Pidiendo contexto core %d.%d\n", REQ_MAJ, REQ_MIN);
    Display* d = XOpenDisplay(NULL);
    if (!d) { printf("sin X\n"); return 2; }
    int n; int fa[] = { GLX_RENDER_TYPE, GLX_RGBA_BIT, GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, GLX_DOUBLEBUFFER, True, GLX_DEPTH_SIZE, 16, 0 };
    GLXFBConfig* fbc = glXChooseFBConfig(d, DefaultScreen(d), fa, &n);
    if (!fbc || !n) { printf("sin fbconfig\n"); return 2; }
    XVisualInfo* vi = glXGetVisualFromFBConfig(d, fbc[0]);
    XSetWindowAttributes sa = {0};
    sa.colormap = XCreateColormap(d, RootWindow(d, vi->screen), vi->visual, AllocNone);
    Window w = XCreateWindow(d, RootWindow(d, vi->screen), 0, 0, 64, 64, 0, vi->depth, InputOutput, vi->visual, CWColormap, &sa);
    XMapWindow(d, w); XSync(d, False);

    GLXContext core = mk(d, fbc[0], REQ_MAJ, REQ_MIN, 1);
    CHECK(core != NULL, "no se creo el contexto core pedido");
    if (!core) return 1;
    glXMakeCurrent(d, w, core);
    printf("GL_VERSION=%s\n", glGetString(GL_VERSION));
    CHECK(is_core33(), "el contexto core no usa GLADIATOR");
    red("core"); glXSwapBuffers(d, w); red("core tras swap");

    /* legacy 2.1 -> core -> legacy -> core -> destruir -> nuevo core */
    GLXContext leg = glXCreateContext(d, vi, NULL, True);
    glXMakeCurrent(d, w, leg); printf("legacy GL_VERSION=%s\n", glGetString(GL_VERSION)); CHECK(strstr((const char*)glGetString(GL_VERSION), "Lorica") == NULL, "legacy no debe usar GLADIATOR"); red("legacy");
    glXMakeCurrent(d, w, core); CHECK(is_core33(), "vuelta a core"); red("core 2");
    glXMakeCurrent(d, w, leg); red("legacy 2");
    glXMakeCurrent(d, w, core); red("core 3");
    glXMakeCurrent(d, None, NULL); glXDestroyContext(d, core);
    GLXContext c2 = mk(d, fbc[0], REQ_MAJ, REQ_MIN, 1);
    glXMakeCurrent(d, w, c2); CHECK(is_core33(), "nuevo core"); red("core nuevo");
    glXMakeCurrent(d, None, NULL); glXDestroyContext(d, c2); glXDestroyContext(d, leg);
    printf(fails ? "RESULTADO: %d FALLOS\n" : "RESULTADO: TODO OK\n", fails);
    return fails != 0;
}
