// Presentacion cero-copia: AHardwareBuffer + DRI3 (modifier 1256) + Present
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <android/hardware_buffer.h>

#define G_FB 0x8D40
#define G_READ_FB 0x8CA8
#define G_DRAW_FB 0x8CA9
#define G_READ_FB_BIND 0x8CAA
#define G_DRAW_FB_BIND 0x8CA6
#define G_COLOR0 0x8CE0
#define G_FB_COMPLETE 0x8CD5
#define G_TEX2D 0x0DE1
#define G_TEX2D_BIND 0x8069
#define G_COLOR_BIT 0x4000
#define G_NEAREST 0x2600
#define G_SCISSOR 0x0C11
#define E_NATIVE_BUF 0x3140
#define E_PRESERVED 0x30D2
#define MOD_FLIPPED 1256ULL

typedef struct { AHardwareBuffer *ahb; void *img; unsigned tex, fbo; xcb_pixmap_t pix; } buf_t;
static struct { int init, dead, ready, first, cur, w, h; Window win; xcb_connection_t *c; buf_t b[2]; uint32_t serial; } S;

static void *(*gpa)(const char*);
static void (*f_GenTex)(int, unsigned*);
static void (*f_DelTex)(int, const unsigned*);
static void (*f_BindTex)(unsigned, unsigned);
static void (*f_GetInt)(unsigned, int*);
static void (*f_GenFb)(int, unsigned*);
static void (*f_DelFb)(int, const unsigned*);
static void (*f_BindFb)(unsigned, unsigned);
static void (*f_FbTex)(unsigned, unsigned, unsigned, unsigned, int);
static unsigned (*f_FbStatus)(unsigned);
static void (*f_Blit)(int,int,int,int,int,int,int,int,unsigned,unsigned);
static void (*f_Finish)(void);
static unsigned char (*f_IsEn)(unsigned);
static void (*f_En)(unsigned);
static void (*f_Dis)(unsigned);
static void (*f_Target)(unsigned, void*);
static void *(*f_CreateImg)(EGLDisplay, EGLContext, unsigned, void*, const EGLint*);
static unsigned (*f_DestroyImg)(EGLDisplay, void*);
static void *(*f_NativeBuf)(const AHardwareBuffer*);
static int (*a_alloc)(const AHardwareBuffer_Desc*, AHardwareBuffer**);
static void (*a_release)(AHardwareBuffer*);
static int (*a_send)(const AHardwareBuffer*, int);

static int resolve(void) {
    const char *e = getenv("LIBGL_EGL");
    void *h = dlopen(e ? e : "libEGL.so", RTLD_NOW);
    void *n = dlopen("libnativewindow.so", RTLD_NOW);
    if (!h || !n) return 0;
    *(void**)&gpa = dlsym(h, "eglGetProcAddress");
    if (!gpa) return 0;
#define R(v, s) *(void**)&v = gpa(s); if (!v) return 0;
#define A(v, s) *(void**)&v = dlsym(n, s); if (!v) return 0;
    R(f_GenTex, "glGenTextures") R(f_DelTex, "glDeleteTextures") R(f_BindTex, "glBindTexture")
    R(f_GetInt, "glGetIntegerv") R(f_GenFb, "glGenFramebuffers") R(f_DelFb, "glDeleteFramebuffers")
    R(f_BindFb, "glBindFramebuffer") R(f_FbTex, "glFramebufferTexture2D")
    R(f_FbStatus, "glCheckFramebufferStatus") R(f_Blit, "glBlitFramebuffer") R(f_Finish, "glFinish")
    R(f_IsEn, "glIsEnabled") R(f_En, "glEnable") R(f_Dis, "glDisable")
    R(f_Target, "glEGLImageTargetTexture2DOES") R(f_CreateImg, "eglCreateImageKHR")
    R(f_DestroyImg, "eglDestroyImageKHR") R(f_NativeBuf, "eglGetNativeClientBufferANDROID")
    A(a_alloc, "AHardwareBuffer_allocate") A(a_release, "AHardwareBuffer_release")
    A(a_send, "AHardwareBuffer_sendHandleToUnixSocket")
    return 1;
}

static void rel(buf_t *b, EGLDisplay ed) {
    if (b->pix && S.c) xcb_free_pixmap(S.c, b->pix);
    if (b->fbo) f_DelFb(1, &b->fbo);
    if (b->tex) f_DelTex(1, &b->tex);
    if (b->img) f_DestroyImg(ed, b->img);
    if (b->ahb) a_release(b->ahb);
    memset(b, 0, sizeof(*b));
}
static void rel_all(EGLDisplay ed) {
    rel(&S.b[0], ed); rel(&S.b[1], ed);
    if (S.c) xcb_flush(S.c);
    S.ready = 0;
}

static int mk(buf_t *b, Window win, int w, int h, EGLDisplay ed) {
    AHardwareBuffer_Desc d; memset(&d, 0, sizeof(d));
    d.width = w; d.height = h; d.layers = 1;
    d.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    d.usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    if (a_alloc(&d, &b->ahb) != 0 || !b->ahb) { fprintf(stderr, "LIBGL: AHB: allocate fallo\n"); return 0; }
    EGLint at[] = { E_PRESERVED, EGL_TRUE, EGL_NONE };
    b->img = f_CreateImg(ed, EGL_NO_CONTEXT, E_NATIVE_BUF, f_NativeBuf(b->ahb), at);
    if (!b->img) { fprintf(stderr, "LIBGL: AHB: eglCreateImage fallo\n"); return 0; }
    int ot = 0, of = 0;
    f_GetInt(G_TEX2D_BIND, &ot); f_GetInt(G_READ_FB_BIND, &of);
    f_GenTex(1, &b->tex); f_BindTex(G_TEX2D, b->tex); f_Target(G_TEX2D, b->img);
    f_BindTex(G_TEX2D, ot);
    f_GenFb(1, &b->fbo); f_BindFb(G_FB, b->fbo);
    f_FbTex(G_FB, G_COLOR0, G_TEX2D, b->tex, 0);
    unsigned st = f_FbStatus(G_FB);
    f_BindFb(G_FB, of);
    if (st != G_FB_COMPLETE) { fprintf(stderr, "LIBGL: AHB: FBO incompleto 0x%x\n", st); return 0; }

    // Handshake DRI3: el servidor escribe 1 byte y espera el handle por el socket
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return 0;
    b->pix = xcb_generate_id(S.c);
    int32_t fds[1] = { sv[0] };
    xcb_void_cookie_t ck = xcb_dri3_pixmap_from_buffers_checked(S.c, b->pix, win, 1, w, h,
        w * 4, 0, 0, 0, 0, 0, 0, 0, 24, 32, MOD_FLIPPED, fds);
    xcb_flush(S.c);
    struct pollfd p = { sv[1], POLLIN, 0 };
    char c1 = 0; int ok = 0;
    if (poll(&p, 1, 3000) == 1 && read(sv[1], &c1, 1) == 1 && a_send(b->ahb, sv[1]) == 0) ok = 1;
    else fprintf(stderr, "LIBGL: AHB: handshake DRI3 sin respuesta del servidor\n");
    close(sv[1]);
    if (!ok) return 0;
    xcb_generic_error_t *er = xcb_request_check(S.c, ck);
    if (er) { fprintf(stderr, "LIBGL: AHB: DRI3 error %d\n", er->error_code); free(er); b->pix = 0; return 0; }
    return 1;
}

int lorica_ahb_present(Display *dpy, Window win, int w, int h, EGLDisplay ed) {
    if (S.dead) return 0;
    if (!S.init) {
        S.init = 1;
        const char *e = getenv("LIBGL_AHB");
        if ((e && e[0] == '0') || !resolve()) { S.dead = 1; return 0; }
    }
    if (!S.ready || S.w != w || S.h != h || S.win != win) {
        rel_all(ed);
        XFlush(dpy);
        S.c = XGetXCBConnection(dpy);
        if (!S.c || !mk(&S.b[0], win, w, h, ed) || !mk(&S.b[1], win, w, h, ed)) {
            rel_all(ed); S.dead = 1;
            fprintf(stderr, "LIBGL: AHB desactivado, uso copia por CPU\n");
            return 0;
        }
        S.ready = 1; S.w = w; S.h = h; S.win = win; S.first = 1; S.cur = 0;
        fprintf(stderr, "LIBGL: AHB cero-copia activo %dx%d\n", w, h);
    }
    buf_t *b = &S.b[S.cur];
    int rb = 0, db = 0;
    f_GetInt(G_READ_FB_BIND, &rb); f_GetInt(G_DRAW_FB_BIND, &db);
    unsigned char sc = f_IsEn(G_SCISSOR);
    if (sc) f_Dis(G_SCISSOR);
    f_BindFb(G_READ_FB, 0); f_BindFb(G_DRAW_FB, b->fbo);
    f_Blit(0, 0, w, h, 0, 0, w, h, G_COLOR_BIT, G_NEAREST);
    f_BindFb(G_READ_FB, rb); f_BindFb(G_DRAW_FB, db);
    if (sc) f_En(G_SCISSOR);
    f_Finish();
    if (S.first) {
        S.first = 0;
        xcb_void_cookie_t ck = xcb_present_pixmap_checked(S.c, win, b->pix, ++S.serial, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL);
        xcb_generic_error_t *er = xcb_request_check(S.c, ck);
        if (er) { fprintf(stderr, "LIBGL: AHB: Present error %d\n", er->error_code); free(er); rel_all(ed); S.dead = 1; return 0; }
    } else {
        xcb_present_pixmap(S.c, win, b->pix, ++S.serial, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL);
        xcb_flush(S.c);
    }
    S.cur ^= 1;
    return 1;
}
