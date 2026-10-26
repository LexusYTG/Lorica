// Presentacion cero-copia: AHardwareBuffer + DRI3 (modifier 1256) + Present
// AHardwareBuffer solo existe en Android. En Linux/otros se compilan stubs para que glx.c enlace sin cambios.
#ifndef __ANDROID__
#include <X11/Xlib.h>
#include <EGL/egl.h>
void lorica_ahb_context_destroyed(void) { }
int lorica_ahb_present(Display *dpy, Window win, int w, int h, EGLDisplay ed) {
    (void)dpy; (void)win; (void)w; (void)h; (void)ed;
    return 0;
}
#else
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
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
#define G_CLEAR_VALUE 0x0C22
#define G_COLOR_MASK 0x0C23
#define E_NATIVE_BUF 0x3140
#define E_PRESERVED 0x30D2
#define G_VERT 0x8B31
#define G_FRAG 0x8B30
#define G_COMPILE_OK 0x8B81
#define G_LINK_OK 0x8B82
#define G_TRIANGLES 4
#define G_RGBA 0x1908
#define G_RGBA8 0x8058
#define G_UBYTE 0x1401
#define G_TEX_MIN 0x2801
#define G_TEX_MAG 0x2800
#define G_TEX_WRAP_S 0x2802
#define G_TEX_WRAP_T 0x2803
#define G_CLAMP 0x812F
#define G_VIEWPORT 0x0BA2
#define G_CUR_PROGRAM 0x8B8D
#define G_VAO_BIND 0x85B5
#define G_ACTIVE_TEX 0x84E0
#define G_TEXTURE0 0x84C0
#define G_BLEND 0x0BE2
#define G_DEPTH_TEST 0x0B71
#define G_CULL_FACE 0x0B44
#define G_STENCIL_TEST 0x0B90
#define G_DITHER 0x0BD0
#define G_RASTER_DISCARD 0x8C89
#define MOD_FLIPPED 1256ULL

typedef struct { AHardwareBuffer *ahb; void *img; unsigned tex, fbo; xcb_pixmap_t pix; } buf_t;
static struct { int init, dead, ready, first, cur, w, h, hooks; Display *dpy; Window win; xcb_connection_t *c; buf_t b[2]; uint32_t serial; void *ctx; unsigned gen; } S;

// Se incrementa cada vez que glx.c destruye un contexto GL: las texturas/FBO de ese contexto dejan de existir.
static unsigned g_gen;
void lorica_ahb_context_destroyed(void) { g_gen++; }

static void *(*gpa)(const char*);
static void *(*f_CurCtx)(void);
static void (*f_GenTex)(int, unsigned*);
static void (*f_DelTex)(int, const unsigned*);
static void (*f_BindTex)(unsigned, unsigned);
static void (*f_GetInt)(unsigned, int*);
static void (*f_GetFloat)(unsigned, float*);
static void (*f_GetBool)(unsigned, unsigned char*);
static void (*f_ClearColor)(float, float, float, float);
static void (*f_Clear)(unsigned);
static void (*f_ColorMask)(unsigned char, unsigned char, unsigned char, unsigned char);
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
// Camino de swizzle (shader): se resuelven aparte; si faltan, se usa el blit (colores R/B invertidos).
static unsigned (*f_CreateShader)(unsigned);
static void (*f_ShaderSource)(unsigned, int, const char *const*, const int*);
static void (*f_CompileShader)(unsigned);
static void (*f_GetShaderiv)(unsigned, unsigned, int*);
static void (*f_GetShaderLog)(unsigned, int, int*, char*);
static void (*f_DeleteShader)(unsigned);
static unsigned (*f_CreateProgram)(void);
static void (*f_AttachShader)(unsigned, unsigned);
static void (*f_LinkProgram)(unsigned);
static void (*f_GetProgramiv)(unsigned, unsigned, int*);
static void (*f_DeleteProgram)(unsigned);
static void (*f_UseProgram)(unsigned);
static int (*f_GetUniformLoc)(unsigned, const char*);
static void (*f_Uniform1i)(int, int);
static void (*f_GenVA)(int, unsigned*);
static void (*f_BindVA)(unsigned);
static void (*f_DelVA)(int, const unsigned*);
static void (*f_TexImage)(unsigned, int, int, int, int, int, unsigned, unsigned, const void*);
static void (*f_TexParam)(unsigned, unsigned, int);
static void (*f_Viewport)(int, int, int, int);
static void (*f_Draw)(unsigned, int, int);
static int (*a_alloc)(const AHardwareBuffer_Desc*, AHardwareBuffer**);
static void (*a_release)(AHardwareBuffer*);
static int (*a_send)(const AHardwareBuffer*, int);
static void (*a_describe)(const AHardwareBuffer*, AHardwareBuffer_Desc*);

static int resolve(void) {
    const char *e = getenv("LIBGL_EGL");
    void *h = dlopen(e ? e : "libEGL.so", RTLD_NOW);
    void *n = dlopen("libnativewindow.so", RTLD_NOW);
    if (!h || !n) return 0;
    *(void**)&gpa = dlsym(h, "eglGetProcAddress");
    if (!gpa) return 0;
    *(void**)&f_CurCtx = dlsym(h, "eglGetCurrentContext");
    if (!f_CurCtx) return 0;
#define R(v, s) *(void**)&v = gpa(s); if (!v) return 0;
#define A(v, s) *(void**)&v = dlsym(n, s); if (!v) return 0;
    R(f_GenTex, "glGenTextures") R(f_DelTex, "glDeleteTextures") R(f_BindTex, "glBindTexture")
    R(f_GetInt, "glGetIntegerv") R(f_GetFloat, "glGetFloatv") R(f_GetBool, "glGetBooleanv")
    R(f_ClearColor, "glClearColor") R(f_Clear, "glClear") R(f_ColorMask, "glColorMask")
    R(f_GenFb, "glGenFramebuffers") R(f_DelFb, "glDeleteFramebuffers")
    R(f_BindFb, "glBindFramebuffer") R(f_FbTex, "glFramebufferTexture2D")
    R(f_FbStatus, "glCheckFramebufferStatus") R(f_Blit, "glBlitFramebuffer") R(f_Finish, "glFinish")
    R(f_IsEn, "glIsEnabled") R(f_En, "glEnable") R(f_Dis, "glDisable")
    R(f_Target, "glEGLImageTargetTexture2DOES") R(f_CreateImg, "eglCreateImageKHR")
    R(f_DestroyImg, "eglDestroyImageKHR") R(f_NativeBuf, "eglGetNativeClientBufferANDROID")
    A(a_alloc, "AHardwareBuffer_allocate") A(a_release, "AHardwareBuffer_release")
    A(a_send, "AHardwareBuffer_sendHandleToUnixSocket") A(a_describe, "AHardwareBuffer_describe")
    return 1;
}

static int resolve_swz(void) {
    R(f_CreateShader, "glCreateShader") R(f_ShaderSource, "glShaderSource") R(f_CompileShader, "glCompileShader")
    R(f_GetShaderiv, "glGetShaderiv") R(f_GetShaderLog, "glGetShaderInfoLog") R(f_DeleteShader, "glDeleteShader")
    R(f_CreateProgram, "glCreateProgram") R(f_AttachShader, "glAttachShader") R(f_LinkProgram, "glLinkProgram")
    R(f_GetProgramiv, "glGetProgramiv") R(f_DeleteProgram, "glDeleteProgram") R(f_UseProgram, "glUseProgram")
    R(f_GetUniformLoc, "glGetUniformLocation") R(f_Uniform1i, "glUniform1i")
    R(f_GenVA, "glGenVertexArrays") R(f_BindVA, "glBindVertexArray") R(f_DelVA, "glDeleteVertexArrays")
    R(f_TexImage, "glTexImage2D") R(f_TexParam, "glTexParameteri") R(f_Viewport, "glViewport") R(f_Draw, "glDrawArrays")
    return 1;
}

/* El AHB es R8G8B8A8 (bytes R,G,B,A) y el servidor lo lee como B,G,R,X: sin intercambiar canales el rojo y el azul
 * salen cambiados. glBlitFramebuffer no puede reordenar canales, asi que se hace en dos pasos GPU->GPU:
 * (1) blit del framebuffer 0 a una textura propia, (2) un triangulo a pantalla completa hacia el FBO del AHB que
 * lee la textura con .bgra y voltea la Y (GL es bottom-up, X espera top-down). Sin copias por CPU. */
static const char *VS_SRC =
    "#version 300 es\n"
    "void main() {\n"
    "  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
    "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";
static const char *FS_SRC =
    "#version 300 es\n"
    "precision highp float;\nprecision highp int;\nprecision highp sampler2D;\n"
    "uniform sampler2D t;\nout vec4 o;\n"
    "void main() {\n"
    "  ivec2 s = textureSize(t, 0);\n"
    "  vec4 c = texelFetch(t, ivec2(int(gl_FragCoord.x), s.y - 1 - int(gl_FragCoord.y)), 0);\n"
    "  o = vec4(c.b, c.g, c.r, 1.0);\n"
    "}\n";

// Objetos GL del camino de swizzle: pertenecen al contexto S.ctx/S.gen (se olvidan si ese contexto muere).
static struct { unsigned prog, vao, tex, fbo; int w, h, loc; } Z;
static int noswz;

static unsigned sh(unsigned type, const char *src) {
    unsigned id = f_CreateShader(type); int ok = 0;
    f_ShaderSource(id, 1, &src, NULL); f_CompileShader(id); f_GetShaderiv(id, G_COMPILE_OK, &ok);
    if (!ok) {
        char log[512] = {0}; int n = 0; f_GetShaderLog(id, sizeof(log) - 1, &n, log);
        fprintf(stderr, "LIBGL: AHB: shader de swizzle no compila: %s\n", log);
        f_DeleteShader(id); return 0;
    }
    return id;
}

static void swz_free(void) {
    if (Z.fbo) f_DelFb(1, &Z.fbo);
    if (Z.tex) f_DelTex(1, &Z.tex);
    if (Z.vao) f_DelVA(1, &Z.vao);
    if (Z.prog) f_DeleteProgram(Z.prog);
    memset(&Z, 0, sizeof(Z));
}

// Crea (o redimensiona) programa, VAO, textura y FBO. No deja ningun binding cambiado.
static int swz_build(int w, int h) {
    if (!Z.prog) {
        unsigned vs = sh(G_VERT, VS_SRC), fs = vs ? sh(G_FRAG, FS_SRC) : 0;
        if (vs && fs) {
            int ok = 0;
            Z.prog = f_CreateProgram(); f_AttachShader(Z.prog, vs); f_AttachShader(Z.prog, fs);
            f_LinkProgram(Z.prog); f_GetProgramiv(Z.prog, G_LINK_OK, &ok);
            if (!ok) { fprintf(stderr, "LIBGL: AHB: programa de swizzle no enlaza\n"); f_DeleteProgram(Z.prog); Z.prog = 0; }
        }
        if (vs) f_DeleteShader(vs);
        if (fs) f_DeleteShader(fs);
        if (!Z.prog) return 0;
        Z.loc = f_GetUniformLoc(Z.prog, "t");
        f_GenVA(1, &Z.vao);
    }
    int ot = 0, orb = 0, odb = 0;
    f_GetInt(G_TEX2D_BIND, &ot); f_GetInt(G_READ_FB_BIND, &orb); f_GetInt(G_DRAW_FB_BIND, &odb);
    if (!Z.tex) f_GenTex(1, &Z.tex);
    f_BindTex(G_TEX2D, Z.tex);
    f_TexImage(G_TEX2D, 0, G_RGBA8, w, h, 0, G_RGBA, G_UBYTE, NULL);
    f_TexParam(G_TEX2D, G_TEX_MIN, G_NEAREST); f_TexParam(G_TEX2D, G_TEX_MAG, G_NEAREST);
    f_TexParam(G_TEX2D, G_TEX_WRAP_S, G_CLAMP); f_TexParam(G_TEX2D, G_TEX_WRAP_T, G_CLAMP);
    f_BindTex(G_TEX2D, ot);
    if (!Z.fbo) f_GenFb(1, &Z.fbo);
    f_BindFb(G_FB, Z.fbo); f_FbTex(G_FB, G_COLOR0, G_TEX2D, Z.tex, 0);
    unsigned st = f_FbStatus(G_FB);
    f_BindFb(G_READ_FB, orb); f_BindFb(G_DRAW_FB, odb);
    if (st != G_FB_COMPLETE) { fprintf(stderr, "LIBGL: AHB: FBO de swizzle incompleto 0x%x\n", st); return 0; }
    Z.w = w; Z.h = h;
    return 1;
}

static unsigned char cap_off(unsigned c) { if (f_IsEn(c)) { f_Dis(c); return 1; } return 0; }

// Framebuffer 0 -> buffer AHB con R/B intercambiados y Y volteada. Devuelve 0 (sin tocar nada) si no se puede.
static int swz_draw(buf_t *b, int w, int h) {
    if (!Z.prog || Z.w != w || Z.h != h) {
        if (!swz_build(w, h)) {
            swz_free(); noswz = 1;
            fprintf(stderr, "LIBGL: AHB: sin swizzle por shader, uso blit (rojo y azul invertidos)\n");
            return 0;
        }
    }
    int vp[4] = {0, 0, w, h}, prog = 0, vao = 0, tb = 0, au = G_TEXTURE0, rb = 0, db = 0;
    unsigned char cm[4] = {1, 1, 1, 1};
    f_GetInt(G_VIEWPORT, vp); f_GetInt(G_CUR_PROGRAM, &prog); f_GetInt(G_VAO_BIND, &vao);
    f_GetInt(G_TEX2D_BIND, &tb); f_GetInt(G_ACTIVE_TEX, &au);
    f_GetInt(G_READ_FB_BIND, &rb); f_GetInt(G_DRAW_FB_BIND, &db); f_GetBool(G_COLOR_MASK, cm);
    unsigned char sc = cap_off(G_SCISSOR), bl = cap_off(G_BLEND), dt = cap_off(G_DEPTH_TEST), cf = cap_off(G_CULL_FACE);
    unsigned char st = cap_off(G_STENCIL_TEST), di = cap_off(G_DITHER), rd = cap_off(G_RASTER_DISCARD);

    // (1) framebuffer 0 -> textura propia, sin voltear (mascara de color completa por si el driver la aplica al blit)
    f_ColorMask(1, 1, 1, 1);
    f_BindFb(G_READ_FB, 0); f_BindFb(G_DRAW_FB, Z.fbo);
    f_Blit(0, 0, w, h, 0, 0, w, h, G_COLOR_BIT, G_NEAREST);
    // (2) triangulo a pantalla completa hacia el FBO del AHB
    f_BindFb(G_FB, b->fbo);
    f_Viewport(0, 0, w, h); f_ColorMask(1, 1, 1, 1);
    f_UseProgram(Z.prog); f_BindVA(Z.vao);
    f_BindTex(G_TEX2D, Z.tex); f_Uniform1i(Z.loc, au - G_TEXTURE0);
    f_Draw(G_TRIANGLES, 0, 3);

    f_BindTex(G_TEX2D, tb); f_BindVA(vao); f_UseProgram(prog);
    f_Viewport(vp[0], vp[1], vp[2], vp[3]); f_ColorMask(cm[0], cm[1], cm[2], cm[3]);
    f_BindFb(G_READ_FB, rb); f_BindFb(G_DRAW_FB, db);
    if (sc) f_En(G_SCISSOR);
    if (bl) f_En(G_BLEND);
    if (dt) f_En(G_DEPTH_TEST);
    if (cf) f_En(G_CULL_FACE);
    if (st) f_En(G_STENCIL_TEST);
    if (di) f_En(G_DITHER);
    if (rd) f_En(G_RASTER_DISCARD);
    return 1;
}

// Camino anterior: blit directo con Y invertida (GL es bottom-up, X espera top-down). No cambia R/B.
static void blit_flip(buf_t *b, int w, int h) {
    int rb = 0, db = 0;
    f_GetInt(G_READ_FB_BIND, &rb); f_GetInt(G_DRAW_FB_BIND, &db);
    unsigned char sc = f_IsEn(G_SCISSOR);
    if (sc) f_Dis(G_SCISSOR);
    f_BindFb(G_READ_FB, 0); f_BindFb(G_DRAW_FB, b->fbo);
    f_Blit(0, 0, w, h, 0, h, w, 0, G_COLOR_BIT, G_NEAREST);
    f_BindFb(G_READ_FB, rb); f_BindFb(G_DRAW_FB, db);
    if (sc) f_En(G_SCISSOR);
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

// Textura + FBO sobre el EGLImage del buffer, en el contexto GL actual.
static int mk_gl(buf_t *b) {
    int ot = 0, orb = 0, odb = 0;
    f_GetInt(G_TEX2D_BIND, &ot); f_GetInt(G_READ_FB_BIND, &orb); f_GetInt(G_DRAW_FB_BIND, &odb);
    f_GenTex(1, &b->tex); f_BindTex(G_TEX2D, b->tex); f_Target(G_TEX2D, b->img);
    f_BindTex(G_TEX2D, ot);
    f_GenFb(1, &b->fbo); f_BindFb(G_FB, b->fbo);
    f_FbTex(G_FB, G_COLOR0, G_TEX2D, b->tex, 0);
    unsigned st = f_FbStatus(G_FB);
    f_BindFb(G_READ_FB, orb); f_BindFb(G_DRAW_FB, odb);
    if (st != G_FB_COMPLETE) { fprintf(stderr, "LIBGL: AHB: FBO incompleto 0x%x\n", st); return 0; }
    return 1;
}

// Un AHardwareBuffer nuevo trae memoria sin inicializar (restos de otras apps): se limpia a negro
// guardando y restaurando el estado que toca (scissor, mascara de color, color de limpieza, FBO).
static void clear_buf(buf_t *b) {
    float cc[4] = {0, 0, 0, 0}; unsigned char cm[4] = {1, 1, 1, 1}; int orb = 0, odb = 0;
    unsigned char sc = f_IsEn(G_SCISSOR);
    f_GetInt(G_READ_FB_BIND, &orb); f_GetInt(G_DRAW_FB_BIND, &odb);
    f_GetFloat(G_CLEAR_VALUE, cc); f_GetBool(G_COLOR_MASK, cm);
    if (sc) f_Dis(G_SCISSOR);
    f_BindFb(G_DRAW_FB, b->fbo);
    f_ColorMask(1, 1, 1, 1); f_ClearColor(0, 0, 0, 1); f_Clear(G_COLOR_BIT);
    f_ColorMask(cm[0], cm[1], cm[2], cm[3]); f_ClearColor(cc[0], cc[1], cc[2], cc[3]);
    f_BindFb(G_READ_FB, orb); f_BindFb(G_DRAW_FB, odb);
    if (sc) f_En(G_SCISSOR);
}

static int mk(buf_t *b, Window win, int w, int h, EGLDisplay ed) {
    AHardwareBuffer_Desc d; memset(&d, 0, sizeof(d));
    d.width = w; d.height = h; d.layers = 1;
    d.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    d.usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    if (a_alloc(&d, &b->ahb) != 0 || !b->ahb) { fprintf(stderr, "LIBGL: AHB: allocate fallo\n"); return 0; }
    // Ancho de fila real del buffer (en pixeles): puede ser mayor que el ancho pedido.
    AHardwareBuffer_Desc dd; memset(&dd, 0, sizeof(dd)); a_describe(b->ahb, &dd);
    uint32_t stride = dd.stride ? dd.stride * 4 : (uint32_t)w * 4;
    if (stride != (uint32_t)w * 4) fprintf(stderr, "LIBGL: AHB: stride real %u bytes (ancho %d)\n", stride, w);
    EGLint at[] = { E_PRESERVED, EGL_TRUE, EGL_NONE };
    b->img = f_CreateImg(ed, EGL_NO_CONTEXT, E_NATIVE_BUF, f_NativeBuf(b->ahb), at);
    if (!b->img) { fprintf(stderr, "LIBGL: AHB: eglCreateImage fallo\n"); return 0; }
    if (!mk_gl(b)) return 0;
    clear_buf(b);

    // Handshake DRI3: el servidor escribe 1 byte y espera el handle por el socket
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return 0;
    b->pix = xcb_generate_id(S.c);
    int32_t fds[1] = { sv[0] };
    xcb_void_cookie_t ck = xcb_dri3_pixmap_from_buffers_checked(S.c, b->pix, win, 1, w, h,
        stride, 0, 0, 0, 0, 0, 0, 0, 24, 32, MOD_FLIPPED, fds);
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

/* Liberacion de recursos del servidor. free_x() solo toca X (pixmaps DRI3): no usa GL ni EGL, asi que es segura
 * desde atexit, desde el cierre del display y desde el manejador de senales (via bandera, ver present). */
static volatile sig_atomic_t g_sig;

static void free_x(void) {
    int i;
    if (!S.c) return;
    for (i = 0; i < 2; i++)
        if (S.b[i].pix) { xcb_free_pixmap(S.c, S.b[i].pix); S.b[i].pix = 0; }
    xcb_flush(S.c);
}

// XCloseDisplay llama esto con la conexion todavia viva: se liberan los pixmaps y se olvida la conexion.
static int on_close(Display *d, XExtCodes *codes) {
    (void)codes;
    if (d != S.dpy) return 0;
    free_x();
    S.ready = 0; S.c = NULL; S.dpy = NULL;
    return 0;
}

// Si la app sale sin cerrar el display (S.c sigue valido porque on_close no corrio), se liberan igual.
static void on_atexit(void) { free_x(); }

// Solo marca la bandera: xcb no es seguro dentro de un manejador. La libera present() en el proximo frame.
static void on_sig(int sig) {
    unsigned left;
    g_sig = sig;
    signal(sig, SIG_DFL);      // un segundo Ctrl+C mata sin esperar
    left = alarm(0);
    alarm(left ? left : 1);    // si la app no presenta mas frames, SIGALRM la termina en 1 s
}

static void hooks_once(void) {
    const char *e = getenv("LIBGL_AHB_SIGNALS");
    int i, sigs[2] = { SIGINT, SIGTERM };
    if (S.hooks) return;
    S.hooks = 1;
    atexit(on_atexit);
    if (e && e[0] == '0') return;
    for (i = 0; i < 2; i++) {          // solo si la app no tiene manejador propio
        struct sigaction old, nw;
        if (sigaction(sigs[i], NULL, &old) != 0 || old.sa_handler != SIG_DFL || (old.sa_flags & SA_SIGINFO)) continue;
        memset(&nw, 0, sizeof(nw)); nw.sa_handler = on_sig; sigemptyset(&nw.sa_mask); nw.sa_flags = SA_RESTART;
        sigaction(sigs[i], &nw, NULL);
    }
}

// Prototipo de libX11 (esta en Xlibint.h; se declara aca para no depender de ese header).
extern int (*XESetCloseDisplay(Display *dpy, int extension, int (*proc)(Display*, XExtCodes*)))(Display*, XExtCodes*);

int lorica_ahb_present(Display *dpy, Window win, int w, int h, EGLDisplay ed) {
    if (g_sig) {                       // Ctrl+C / SIGTERM: liberar en el servidor y terminar como corresponde
        int sg = g_sig; g_sig = 0;
        free_x();
        signal(sg, SIG_DFL); raise(sg);
        return 0;
    }
    if (S.dead) return 0;
    if (!S.init) {
        S.init = 1;
        const char *e = getenv("LIBGL_AHB"), *z = getenv("LIBGL_AHB_SWIZZLE");
        if ((e && e[0] == '0') || !resolve()) { S.dead = 1; return 0; }
        if ((z && z[0] == '0') || !resolve_swz()) noswz = 1;
    }
    // Si el contexto GL cambio (la app destruyo y recreo el suyo), la textura y el FBO guardados
    // pertenecian al viejo. Se olvidan sin borrarlos (borrar esos nombres desde el contexto nuevo
    // podria tocar objetos ajenos) y se rehacen desde el EGLImage, que no depende del contexto.
    void *cc = f_CurCtx();
    if (!cc) return 0;
    if (cc != S.ctx || g_gen != S.gen) {
        int i;
        memset(&Z, 0, sizeof(Z));
        for (i = 0; i < 2; i++) { S.b[i].tex = 0; S.b[i].fbo = 0; }
        if (S.ready) {
            for (i = 0; i < 2; i++) {
                if (!mk_gl(&S.b[i])) {
                    rel_all(ed); S.dead = 1;
                    fprintf(stderr, "LIBGL: AHB desactivado (contexto nuevo), uso copia por CPU\n");
                    return 0;
                }
            }
        }
    }
    S.ctx = cc; S.gen = g_gen;
    if (!S.ready || S.w != w || S.h != h || S.win != win) {
        rel_all(ed);
        XFlush(dpy);
        S.c = XGetXCBConnection(dpy);
        if (S.c && S.dpy != dpy) {
            XExtCodes *ec = XAddExtension(dpy);   // solo local: no consulta al servidor
            if (ec) XESetCloseDisplay(dpy, ec->extension, on_close);
            S.dpy = dpy;
        }
        hooks_once();
        if (!S.c || !mk(&S.b[0], win, w, h, ed) || !mk(&S.b[1], win, w, h, ed)) {
            rel_all(ed); S.dead = 1;
            fprintf(stderr, "LIBGL: AHB desactivado, uso copia por CPU\n");
            return 0;
        }
        S.ready = 1; S.w = w; S.h = h; S.win = win; S.first = 1; S.cur = 0;
        fprintf(stderr, "LIBGL: AHB cero-copia activo %dx%d (%s)\n", w, h, noswz ? "blit" : "swizzle BGRA");
    }
    buf_t *b = &S.b[S.cur];
    if (noswz || !swz_draw(b, w, h)) blit_flip(b, w, h);
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

#endif /* __ANDROID__ */
