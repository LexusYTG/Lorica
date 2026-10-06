#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "no display\n"); return 1; }
    int scr = DefaultScreen(dpy);
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0, 64, 64, 0, 0, 0);

    int attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 16, GLX_DOUBLEBUFFER, None };
    XVisualInfo *vi = glXChooseVisual(dpy, scr, attribs);
    if (!vi) { fprintf(stderr, "no visual\n"); return 1; }

    GLXContext ctx = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    if (!ctx) { fprintf(stderr, "no context\n"); return 1; }
    glXMakeCurrent(dpy, win, ctx);

    printf("GL_VERSION:  %s\n", glGetString(GL_VERSION));
    printf("GL_VENDOR:   %s\n", glGetString(GL_VENDOR));
    printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
    printf("GLSL:        %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    GLint maj = 0, min = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &maj);
    glGetIntegerv(GL_MINOR_VERSION, &min);
    printf("GL_MAJOR_VERSION=%d  GL_MINOR_VERSION=%d\n", maj, min);

    return 0;
}
