/* Prueba de integracion de Lorica GL31 con un backend GLES simulado (sin GPU).
 *
 * Comprueba lo que un test de linkeo no ve: que gl31_init() carga y detecta
 * capacidades igual con ES 3.0 y 3.2, que la dispatch table resuelve, que el
 * estado es por hilo, y que los modulos completados (texturas, render
 * condicional, consultas de shaders, glPixelStorei...) hacen lo que dicen.
 *
 * Las funciones del backend que la prueba no mira se resuelven a `mock_noop`
 * (devuelve 0). Llamarlas con otra firma es tecnicamente UB, pero es inocuo en
 * x86-64/AArch64 y solo ocurre en este test.
 */
#include "../gl31.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* enums de desktop que el header publico no define (los .c los definen localmente) */
#define DEF(n, v) enum { n = v }
#ifndef GL_PROGRAM_POINT_SIZE
#define GL_PROGRAM_POINT_SIZE 0x8642
#endif
#define GL_QUERY_WAIT 0x8E13
#define GL_TEXTURE_WIDTH 0x1000
#define GL_TEXTURE_HEIGHT 0x1001
#define GL_TEXTURE_RED_SIZE 0x805C
#define GL_TEXTURE_BINDING_1D_ARRAY 0x8C1C
#define GL_PACK_SWAP_BYTES 0x0D00
#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif
#ifndef GL_LINE_SMOOTH
#define GL_LINE_SMOOTH 0x0B20
#endif

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

/* ======================= backend simulado ======================= */
static int  cfg_major = 3, cfg_minor = 0;
static const char* cfg_ext[8];
static int  cfg_next;
static const char* cfg_hide;          /* nombre exacto que el loader no encuentra */
static const char* cfg_hide_prefix;   /* prefijo que el loader no encuentra */
static int  cfg_opt_plain, cfg_opt_ext, cfg_opt_oes;   /* que variantes de opcionales existen */
static GLuint cfg_query_result = 1;

static GLenum be_err;
static struct {
    int enable, disable, enablei, draw_arrays, color_mask, color_maski;
    GLenum last_enable;
    int teximage2d, teximage3d;
    GLint t2_ifmt, t2_w, t2_h;
    GLint t3_w, t3_h, t3_d; GLenum t3_target;
    int pixelstore; GLenum ps_pname; GLint ps_param;
    int drawbuffers; GLenum db0; GLsizei db_n;
    char src[512];
    GLuint next_id;
} cnt;

static long mock_noop(void) { return 0; }

static void mk_glEnable(GLenum c) { cnt.enable++; cnt.last_enable = c; }
static void mk_glDisable(GLenum c) { cnt.disable++; cnt.last_enable = c; }
static GLboolean mk_glIsEnabled(GLenum c) { (void)c; return GL_FALSE; }
static void mk_glEnablei(GLenum c, GLuint i) { (void)c; (void)i; cnt.enablei++; }
static void mk_glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) { (void)r; (void)g; (void)b; (void)a; cnt.color_mask++; }
static void mk_glColorMaski(GLuint i, GLboolean r, GLboolean g, GLboolean b, GLboolean a) { (void)i; (void)r; (void)g; (void)b; (void)a; cnt.color_maski++; }
static void mk_glDrawArrays(GLenum m, GLint f, GLsizei n) { (void)m; (void)f; (void)n; cnt.draw_arrays++; }
static GLenum mk_glGetError(void) { GLenum e = be_err; be_err = 0; return e; }

static void mk_glGetIntegerv(GLenum p, GLint* v)
{
    switch (p) {
        case GL_MAJOR_VERSION: *v = cfg_major; break;
        case GL_MINOR_VERSION: *v = cfg_minor; break;
        case GL_NUM_EXTENSIONS: *v = cfg_next; break;
        case GL_MAX_DRAW_BUFFERS: *v = 4; break;
        case GL_MAX_COLOR_ATTACHMENTS: *v = 4; break;
        case GL_MAX_VERTEX_ATTRIBS: *v = 16; break;
        case GL_MAX_TEXTURE_SIZE: *v = 2048; break;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: *v = 32; break;
        case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT: *v = 256; break;
        case GL_MAX_UNIFORM_BUFFER_BINDINGS: *v = 24; break;
        default: *v = 0;
    }
}
static const GLubyte* mk_glGetStringi(GLenum n, GLuint i)
{
    (void)n;
    return (i < (GLuint)cfg_next) ? (const GLubyte*)cfg_ext[i] : NULL;
}
static void mk_glGetFloatv(GLenum p, GLfloat* v) { (void)p; v[0] = 1; v[1] = 2; v[2] = 3; v[3] = 4; }

static GLboolean mk_glIsQuery(GLuint id) { return id >= 1; }
static void mk_glGetQueryObjectuiv(GLuint id, GLenum pn, GLuint* o)
{
    (void)id;
    *o = (pn == GL_QUERY_RESULT_AVAILABLE) ? 1u : cfg_query_result;
}

static void mk_glTexImage2D(GLenum t, GLint l, GLint ifmt, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty, const void* p)
{ (void)t; (void)l; (void)b; (void)f; (void)ty; (void)p; cnt.teximage2d++; cnt.t2_ifmt = ifmt; cnt.t2_w = w; cnt.t2_h = h; }
static void mk_glTexImage3D(GLenum t, GLint l, GLint ifmt, GLsizei w, GLsizei h, GLsizei d, GLint b, GLenum f, GLenum ty, const void* p)
{ (void)l; (void)ifmt; (void)b; (void)f; (void)ty; (void)p; cnt.teximage3d++; cnt.t3_target = t; cnt.t3_w = w; cnt.t3_h = h; cnt.t3_d = d; }
static void mk_glPixelStorei(GLenum p, GLint v) { cnt.pixelstore++; cnt.ps_pname = p; cnt.ps_param = v; }
static void mk_glDrawBuffers(GLsizei n, const GLenum* b) { cnt.drawbuffers++; cnt.db_n = n; cnt.db0 = b[0]; }
static void mk_glGetTexLevelParameteriv(GLenum t, GLint l, GLenum p, GLint* o) { (void)t; (void)l; (void)p; *o = 99; }

static GLuint mk_glCreateShader(GLenum t) { (void)t; return ++cnt.next_id; }
static GLuint mk_glCreateProgram(void) { return ++cnt.next_id; }
static void mk_glShaderSource(GLuint s, GLsizei n, const GLchar* const* str, const GLint* l)
{ (void)s; (void)n; (void)l; strncpy(cnt.src, str[0], sizeof cnt.src - 1); }
static void mk_glGetShaderiv(GLuint s, GLenum pn, GLint* o) { (void)s; *o = (pn == GL_COMPILE_STATUS) ? 1 : 0; }
static void mk_glGetProgramiv(GLuint p, GLenum pn, GLint* o) { (void)p; *o = (pn == GL_LINK_STATUS) ? 1 : 0; }

#define M(n) { #n, (void*)mk_##n }
static const struct { const char* n; void* f; } k_mock[] = {
    M(glEnable), M(glDisable), M(glIsEnabled), M(glColorMask), M(glDrawArrays), M(glGetError),
    M(glGetIntegerv), M(glGetStringi), M(glGetFloatv), M(glIsQuery), M(glGetQueryObjectuiv),
    M(glTexImage2D), M(glTexImage3D), M(glPixelStorei), M(glDrawBuffers),
    M(glCreateShader), M(glCreateProgram), M(glShaderSource), M(glGetShaderiv), M(glGetProgramiv),
};
static const char* const k_opt[] = {
    "glEnablei", "glDisablei", "glIsEnabledi", "glColorMaski", "glTexBuffer", "glTexBufferRange",
    "glTexParameterIiv", "glTexParameterIuiv", "glGetTexParameterIiv", "glGetTexParameterIuiv",
    "glSamplerParameterIiv", "glSamplerParameterIuiv", "glGetTexLevelParameteriv", NULL
};

static void* loader(const char* name)
{
    size_t i;
    if (cfg_hide && !strcmp(cfg_hide, name)) return NULL;
    if (cfg_hide_prefix && !strncmp(cfg_hide_prefix, name, strlen(cfg_hide_prefix))) return NULL;
    for (i = 0; i < sizeof k_mock / sizeof k_mock[0]; i++)
        if (!strcmp(k_mock[i].n, name)) return k_mock[i].f;
    for (i = 0; k_opt[i]; i++) {
        size_t n = strlen(k_opt[i]);
        if (strncmp(name, k_opt[i], n) != 0) continue;
        {
            const char* sfx = name + n;
            int have = !*sfx ? cfg_opt_plain : !strcmp(sfx, "EXT") ? cfg_opt_ext : !strcmp(sfx, "OES") ? cfg_opt_oes : 0;
            if (!have) return NULL;
            if (!strcmp(k_opt[i], "glEnablei"))  return (void*)mk_glEnablei;
            if (!strcmp(k_opt[i], "glColorMaski")) return (void*)mk_glColorMaski;
            if (!strcmp(k_opt[i], "glGetTexLevelParameteriv")) return (void*)mk_glGetTexLevelParameteriv;
            return (void*)mock_noop;
        }
    }
    return (void*)mock_noop;
}

static int reinit(int maj, int min, int ext_a, int ext_b, const char* ea, const char* eb)
{
    (void)ext_a; (void)ext_b;
    cfg_major = maj; cfg_minor = min;
    cfg_next = 0;
    if (ea) cfg_ext[cfg_next++] = ea;
    if (eb) cfg_ext[cfg_next++] = eb;
    memset(&cnt, 0, sizeof cnt);
    be_err = 0;
    return gl31_init(loader);
}

/* ======================= pruebas ======================= */
static void test_init_and_caps(void)
{
    /* falta una funcion obligatoria -> falla */
    cfg_hide = "glDrawBuffers";
    CHECK(reinit(3, 0, 0, 0, NULL, NULL) == -1);
    cfg_hide = NULL;
    CHECK(gl31_init(NULL) == -1);

    /* ES 3.0 pelado: sin opcionales */
    cfg_opt_plain = cfg_opt_ext = cfg_opt_oes = 1;   /* aunque el loader las ofrezca, sin caps no se piden */
    CHECK(reinit(3, 0, 0, 0, NULL, NULL) == 0);
    CHECK(gl31_caps.es_major == 3 && gl31_caps.es_minor == 0);
    CHECK(!gl31_caps.draw_buf_indexed && !gl31_caps.border_clamp && !gl31_caps.level_query && !gl31_caps.tex_buffer);
    CHECK(gl31_get_optional("glEnablei") == NULL);
    CHECK(gl31_get_optional("glNoExiste") == NULL);
    CHECK(gl31_get_optional(NULL) == NULL);

    /* ES 3.2: todo viene de serie */
    cfg_opt_plain = 1; cfg_opt_ext = cfg_opt_oes = 0;
    CHECK(reinit(3, 2, 0, 0, NULL, NULL) == 0);
    CHECK(gl31_caps.draw_buf_indexed && gl31_caps.border_clamp && gl31_caps.level_query && gl31_caps.tex_buffer);
    CHECK(gl31_get_optional("glEnablei") == (void*)mk_glEnablei);
    CHECK(gl31_get_optional("glTexParameterIiv") != NULL);

    /* ES 3.2 pero al backend le falta una de las cuatro de indexado: se degrada */
    cfg_hide_prefix = "glColorMaski";
    CHECK(reinit(3, 2, 0, 0, NULL, NULL) == 0);
    CHECK(!gl31_caps.draw_buf_indexed && gl31_get_optional("glEnablei") == NULL);
    cfg_hide_prefix = NULL;

    /* ES 3.0 + extensiones, funciones con sufijo EXT / OES */
    cfg_opt_plain = 0; cfg_opt_ext = 1; cfg_opt_oes = 1;
    CHECK(reinit(3, 0, 0, 0, "GL_EXT_draw_buffers_indexed", "GL_OES_texture_border_clamp") == 0);
    CHECK(gl31_caps.draw_buf_indexed && gl31_caps.border_clamp && !gl31_caps.level_query);
    CHECK(gl31_get_optional("glEnablei") == (void*)mk_glEnablei);   /* cargada como glEnableiEXT */
    CHECK(gl31_get_optional("glTexParameterIuiv") != NULL);

    /* ES 3.1: solo level_query */
    cfg_opt_plain = 1; cfg_opt_ext = cfg_opt_oes = 0;
    CHECK(reinit(3, 1, 0, 0, NULL, NULL) == 0);
    CHECK(gl31_caps.level_query && !gl31_caps.draw_buf_indexed);
}

static void test_dispatch(void)
{
    static const char* const must[] = {
        "glEnable", "glDrawBuffer", "glBeginConditionalRender", "glTexImage2D", "glPixelStorei",
        "glGetShaderSource", "glGetAttachedShaders", "glIsShader", "glFlushMappedBufferRange",
        "glGetBufferPointerv", "glGenerateMipmap", "glGetBooleani_v", "glTexStorage1D", "glUniform1f",
        "glVertexAttrib4Nub", "glGetDoublev", NULL
    };
    int i;
    for (i = 0; must[i]; i++) CHECK(gl31_get_proc_address(must[i]) != NULL);
    CHECK(gl31_get_proc_address("glEnable") == (void*)gl31_glEnable);
    CHECK(gl31_get_proc_address("glDrawBuffer") == (void*)gl31_glDrawBuffer);
    CHECK(gl31_get_proc_address("glNoExiste") == NULL);
    CHECK(gl31_get_proc_address("gl31_glEnable") == NULL);
    CHECK(gl31_get_proc_address(NULL) == NULL);
}

static void* thread_state(void* arg)
{
    int* ok = (int*)arg;
    GLboolean m[4] = {0, 0, 0, 0};
    gl31_glGetBooleani_v(GL_COLOR_WRITEMASK, 2, m);
    *ok = gl31_glIsEnabled(GL_MULTISAMPLE) == GL_TRUE && m[0] && m[1] && m[2] && m[3];
    return NULL;
}

static void test_state_and_caps(void)
{
    pthread_t th; int ok = 0;
    cfg_opt_plain = 1;
    CHECK(reinit(3, 0, 0, 0, NULL, NULL) == 0);

    /* defaults de GL: MULTISAMPLE activo, LINE_SMOOTH no */
    CHECK(gl31_glIsEnabled(GL_MULTISAMPLE) == GL_TRUE);
    CHECK(gl31_glIsEnabled(GL_LINE_SMOOTH) == GL_FALSE);

    /* caps de desktop: se quedan en el estado propio, no llegan al backend */
    gl31_glEnable(GL_LINE_SMOOTH);
    CHECK(cnt.enable == 0 && gl31_glIsEnabled(GL_LINE_SMOOTH) == GL_TRUE);
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    gl31_glDisable(GL_LINE_SMOOTH);
    CHECK(gl31_glIsEnabled(GL_LINE_SMOOTH) == GL_FALSE && cnt.disable == 0);
    { GLint v = -1; gl31_glEnable(GL_PROGRAM_POINT_SIZE); gl31_glGetIntegerv(GL_PROGRAM_POINT_SIZE, &v); CHECK(v == 1); }

    /* las de ES si llegan */
    gl31_glEnable(GL_BLEND);
    CHECK(cnt.enable == 1 && cnt.last_enable == GL_BLEND);

    /* primitive restart */
    gl31_glEnable(GL_PRIMITIVE_RESTART);
    CHECK(gl31_glIsEnabled(GL_PRIMITIVE_RESTART) == GL_TRUE && cnt.enable == 1);
    gl31_glPrimitiveRestartIndex(0xFFFF);
    { GLint v = 0; gl31_glGetIntegerv(GL_PRIMITIVE_RESTART_INDEX, &v); CHECK(v == 0xFFFF); }
    gl31_glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);     /* no existe en GL desktop */
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);

    /* el estado es por hilo y cada hilo arranca con los defaults */
    pthread_create(&th, NULL, thread_state, &ok);
    pthread_join(th, NULL);
    CHECK(ok);

    /* indexado sin soporte: con >1 draw buffers es error, no un no-op silencioso */
    gl31_glEnablei(GL_BLEND, 1);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION && cnt.enablei == 0);
    gl31_glColorMaski(1, GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION && cnt.color_maski == 0);

    /* con ES 3.2 se reenvia y queda reflejado en glGetBooleani_v */
    CHECK(reinit(3, 2, 0, 0, NULL, NULL) == 0);
    gl31_glEnablei(GL_BLEND, 1);
    CHECK(cnt.enablei == 1 && gl31_glGetError() == GL_NO_ERROR);
    gl31_glColorMaski(1, GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
    {
        GLboolean m[4];
        gl31_glGetBooleani_v(GL_COLOR_WRITEMASK, 1, m);
        CHECK(cnt.color_maski == 1 && m[0] == GL_FALSE && m[1] == GL_TRUE && m[2] == GL_FALSE && m[3] == GL_TRUE);
        gl31_glGetBooleani_v(GL_COLOR_WRITEMASK, 0, m);
        CHECK(m[0] && m[1] && m[2] && m[3]);              /* el 0 no cambio */
    }
}

static void test_conditional_render(void)
{
    CHECK(reinit(3, 0, 0, 0, NULL, NULL) == 0);
    gl31_state()->program = 1;
    gl31_glBindVertexArray(1);

    gl31_glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK(cnt.draw_arrays == 1);

    cfg_query_result = 0;
    gl31_glBeginConditionalRender(5, GL_QUERY_WAIT);
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    gl31_glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK(cnt.draw_arrays == 1);                          /* omitido */
    gl31_glBeginConditionalRender(5, GL_QUERY_WAIT);      /* anidado */
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glEndConditionalRender();
    gl31_glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK(cnt.draw_arrays == 2);                          /* ya dibuja */
    gl31_glEndConditionalRender();
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);     /* sin Begin */

    cfg_query_result = 7;
    gl31_glBeginConditionalRender(5, GL_QUERY_WAIT);
    gl31_glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK(cnt.draw_arrays == 3);                          /* resultado > 0: dibuja */
    gl31_glEndConditionalRender();

    gl31_glBeginConditionalRender(5, 0x1234);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);
    gl31_glBeginConditionalRender(0, GL_QUERY_WAIT);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    cfg_query_result = 1;
}

static void test_shaders(void)
{
    static const char* src = "#version 140\nin vec4 p;\nvoid main() { gl_Position = p; }\n";
    char buf[256]; GLsizei len = -1; GLuint att[4]; GLsizei n = -1;
    GLuint sh, pr;
    CHECK(reinit(3, 0, 0, 0, NULL, NULL) == 0);

    sh = gl31_glCreateShader(GL_VERTEX_SHADER);
    pr = gl31_glCreateProgram();
    CHECK(sh && pr && sh != pr);
    gl31_glShaderSource(sh, 1, &src, NULL);
    gl31_glCompileShader(sh);
    CHECK(strstr(cnt.src, "#version 300 es") != NULL);            /* el backend recibio GLSL ES */

    gl31_glGetShaderSource(sh, sizeof buf, &len, buf);            /* la app recibe SU fuente */
    CHECK(len == (GLsizei)strlen(src) && strcmp(buf, src) == 0);
    gl31_glGetShaderSource(pr, sizeof buf, &len, buf);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glGetShaderSource(9999, sizeof buf, &len, buf);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);

    CHECK(gl31_glIsShader(sh) == GL_TRUE && gl31_glIsShader(pr) == GL_FALSE && gl31_glIsShader(0) == GL_FALSE);
    CHECK(gl31_glIsProgram(pr) == GL_TRUE && gl31_glIsProgram(sh) == GL_FALSE);

    gl31_glAttachShader(pr, sh);
    gl31_glGetAttachedShaders(pr, 4, &n, att);
    CHECK(n == 1 && att[0] == sh);
    gl31_glGetAttachedShaders(sh, 4, &n, att);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glGetAttachedShaders(pr, -1, &n, att);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);

    gl31_glValidateProgram(sh);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glValidateProgram(9999);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glValidateProgram(pr);
    CHECK(gl31_glGetError() == GL_NO_ERROR);
}

static GLint level_param(GLenum target, GLint level, GLenum pname)
{
    GLint v = -12345;
    gl31_glGetTexLevelParameteriv(target, level, pname, &v);
    return v;
}

static void test_textures(void)
{
    GLint v = 0;
    CHECK(reinit(3, 0, 0, 0, NULL, NULL) == 0);

    gl31_glBindTexture(GL_TEXTURE_2D, 1);
    gl31_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK(gl31_glGetError() == GL_NO_ERROR && cnt.teximage2d == 1 && cnt.t2_w == 16 && cnt.t2_h == 8);
    CHECK(level_param(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH) == 16);
    CHECK(level_param(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT) == 8);
    CHECK(level_param(GL_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE) == 8);   /* sin ES 3.1: tabla propia */

    /* mipmaps: la cadena queda anotada */
    gl31_glGenerateMipmap(GL_TEXTURE_2D);
    CHECK(level_param(GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH) == 8 && level_param(GL_TEXTURE_2D, 1, GL_TEXTURE_HEIGHT) == 4);
    CHECK(level_param(GL_TEXTURE_2D, 3, GL_TEXTURE_WIDTH) == 2 && level_param(GL_TEXTURE_2D, 3, GL_TEXTURE_HEIGHT) == 1);
    CHECK(level_param(GL_TEXTURE_2D, 4, GL_TEXTURE_WIDTH) == 1 && level_param(GL_TEXTURE_2D, 4, GL_TEXTURE_HEIGHT) == 1);
    CHECK(level_param(GL_TEXTURE_2D, 5, GL_TEXTURE_WIDTH) == 0);

    /* formatos de desktop que ES exige con tamano */
    gl31_glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 4, 4, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    CHECK(cnt.t2_ifmt == (GLint)GL_DEPTH_COMPONENT24);
    gl31_glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 4, 4, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);
    CHECK(cnt.t2_ifmt == (GLint)GL_DEPTH_COMPONENT16);
    gl31_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_FLOAT, NULL);
    CHECK(cnt.t2_ifmt == (GLint)GL_RGBA32F);
    gl31_glTexImage2D(GL_TEXTURE_2D, 0, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);   /* "4 componentes" */
    CHECK(cnt.t2_ifmt == (GLint)GL_RGBA);

    /* errores: no llegan al backend */
    cnt.teximage2d = 0;
    gl31_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glTexImage2D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);
    gl31_glTexImage2D(GL_TEXTURE_2D, -1, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, -4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    CHECK(cnt.teximage2d == 0);

    /* 1D_ARRAY -> 2D_ARRAY de alto 1 con las capas en depth */
    gl31_glBindTexture(GL_TEXTURE_1D_ARRAY, 2);
    gl31_glTexImage2D(GL_TEXTURE_1D_ARRAY, 0, GL_RGBA8, 32, 5, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK(cnt.teximage3d == 1 && cnt.t3_target == GL_TEXTURE_2D_ARRAY && cnt.t3_w == 32 && cnt.t3_h == 1 && cnt.t3_d == 5);
    CHECK(level_param(GL_TEXTURE_1D_ARRAY, 0, GL_TEXTURE_WIDTH) == 32 && level_param(GL_TEXTURE_1D_ARRAY, 0, GL_TEXTURE_HEIGHT) == 5);

    /* TEXTURE_BINDING_* se sirve desde los bindings logicos */
    gl31_glGetIntegerv(GL_TEXTURE_BINDING_1D_ARRAY, &v);
    CHECK(v == 2);
    gl31_glGetIntegerv(GL_TEXTURE_BINDING_2D, &v);
    CHECK(v == 1);

    /* rectangulo: solo nivel 0; no tiene mipmaps */
    gl31_glBindTexture(GL_TEXTURE_RECTANGLE, 3);
    gl31_glTexImage2D(GL_TEXTURE_RECTANGLE, 1, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glGenerateMipmap(GL_TEXTURE_RECTANGLE);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);

    /* proxy */
    gl31_glTexImage2D(GL_PROXY_TEXTURE_2D, 0, GL_RGBA, 1024, 1024, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK(level_param(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_WIDTH) == 1024);
    gl31_glTexImage2D(GL_PROXY_TEXTURE_2D, 0, GL_RGBA, 99999, 99999, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK(level_param(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_WIDTH) == 0);

    /* desktop GL_TEXTURE_BUFFER sin soporte del backend */
    gl31_glBindTexture(GL_TEXTURE_BUFFER, 4);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);

    /* con ES 3.1+ las consultas de nivel van al backend */
    cfg_opt_plain = 1;
    CHECK(reinit(3, 1, 0, 0, NULL, NULL) == 0);
    gl31_glBindTexture(GL_TEXTURE_2D, 1);
    gl31_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK(level_param(GL_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE) == 99);
    CHECK(level_param(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH) == 16);        /* esto sigue saliendo de la tabla */
}

static void test_pixelstore_and_misc(void)
{
    GLdouble d[4] = {0, 0, 0, 0};
    CHECK(reinit(3, 0, 0, 0, NULL, NULL) == 0);

    gl31_glPixelStorei(GL_UNPACK_ALIGNMENT, 3);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE && cnt.pixelstore == 0);
    gl31_glPixelStorei(GL_PACK_ROW_LENGTH, -1);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glPixelStorei(0x1234, 1);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);
    gl31_glPixelStorei(GL_PACK_ALIGNMENT, 1);
    CHECK(cnt.pixelstore == 1 && cnt.ps_pname == GL_PACK_ALIGNMENT && cnt.ps_param == 1);
    CHECK(gl31_state()->pack_alignment == 1);
    gl31_glPixelStorei(GL_PACK_SWAP_BYTES, 0);                 /* sin efecto, sin error */
    CHECK(gl31_glGetError() == GL_NO_ERROR && cnt.pixelstore == 1);
    gl31_glPixelStoref(GL_UNPACK_ROW_LENGTH, 7.6f);
    CHECK(cnt.ps_pname == GL_UNPACK_ROW_LENGTH && cnt.ps_param == 8);

    /* glDrawBuffer(GL_BACK) del framebuffer por defecto -> glDrawBuffers(1, {BACK}) */
    gl31_glDrawBuffer(GL_BACK);
    CHECK(cnt.drawbuffers == 1 && cnt.db_n == 1 && cnt.db0 == GL_BACK);
    gl31_glDrawBuffer(GL_FRONT);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);

    /* glGetDoublev copia todos los componentes */
    gl31_glGetDoublev(GL_VIEWPORT, d);
    CHECK(d[0] == 1 && d[1] == 2 && d[2] == 3 && d[3] == 4);

    /* strings */
    CHECK(strncmp((const char*)gl31_glGetString(GL_VERSION), "3.1", 3) == 0);
    { GLint maj = 0; gl31_glGetIntegerv(GL_MAJOR_VERSION, &maj); CHECK(maj == 3); }
}

static void test_error_order_xfb(void)
{
    CHECK(reinit(3, 0, 0, 0, NULL, NULL) == 0);
    gl31_state()->program = 5;
    be_err = GL_INVALID_ENUM;                       /* error previo de la app, aun en el backend */
    gl31_glBeginTransformFeedback(GL_POINTS);
    CHECK(gl31_state()->xfb_active == 1);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);    /* no se perdio */
    gl31_glBeginTransformFeedback(GL_POINTS);       /* anidado */
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glEndTransformFeedback();
    CHECK(gl31_state()->xfb_active == 0);
    gl31_glEndTransformFeedback();
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
}

static void test_shutdown(void)
{
    CHECK(reinit(3, 0, 0, 0, NULL, NULL) == 0);
    gl31_shutdown();
    CHECK(gl31_be.glEnable == NULL);
    gl31_shutdown();                                /* idempotente */
}

int main(void)
{
    test_init_and_caps();
    test_dispatch();
    test_state_and_caps();
    test_conditional_render();
    test_shaders();
    test_textures();
    test_pixelstore_and_misc();
    test_error_order_xfb();
    test_shutdown();
    printf(fails ? "%d FALLOS\n" : "TODO OK\n", fails);
    return fails != 0;
}
