/* Pruebas de las funciones de GL 3.0 / 3.1 / 3.2 anadidas: conversor GLSL (samplers 1D/Rect,
 * geometry, buffer, ClipDistance, frag-data), sync, base vertex, texturas 1D / multisample / BGRA,
 * BindFragDataLocation y registro en la dispatch table. Backend simulado, sin GPU. */
#include "../gl31.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

static int cfg_major = 3, cfg_minor = 0, cfg_next;
static const char* cfg_ext[8];
static struct {
    int draw_elements, vap, tex2d, ms2d, fence;
    GLint tex_w, tex_h; GLenum tex_fmt; unsigned char tex_px[16]; int tex_has_px;
    char src[4096];
    GLuint next_id; GLuint vao_next;
    GLsizeiptr last_vap_off[4]; int nvap;
} c;

static long mock_noop(void) { return 0; }
static void mk_glGetIntegerv(GLenum p, GLint* v)
{
    switch (p) {
        case GL_MAJOR_VERSION: *v = cfg_major; break;
        case GL_MINOR_VERSION: *v = cfg_minor; break;
        case GL_NUM_EXTENSIONS: *v = cfg_next; break;
        case GL_MAX_DRAW_BUFFERS: *v = 4; break;
        case GL_MAX_VERTEX_ATTRIBS: *v = 16; break;
        case GL_MAX_TEXTURE_SIZE: *v = 2048; break;
        case GL_MAX_SAMPLES: *v = 4; break;
        case GL_ELEMENT_ARRAY_BUFFER_BINDING: *v = 1; break;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: *v = 32; break;
        case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT: *v = 256; break;
        case GL_MAX_UNIFORM_BUFFER_BINDINGS: *v = 24; break;
        default: *v = 0;
    }
}
static const GLubyte* mk_glGetStringi(GLenum n, GLuint i) { (void)n; return i < (GLuint)cfg_next ? (const GLubyte*)cfg_ext[i] : NULL; }
static GLenum mk_glGetError(void) { return 0; }
static void mk_glTexImage2D(GLenum t, GLint l, GLint ifmt, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty, const void* p)
{
    (void)t; (void)l; (void)ifmt; (void)b; (void)ty;
    c.tex2d++; c.tex_w = w; c.tex_h = h; c.tex_fmt = f;
    if (p) { memcpy(c.tex_px, p, (size_t)w * (size_t)h * 4u > 16 ? 16 : (size_t)w * (size_t)h * 4u); c.tex_has_px = 1; }
}
static void mk_glTexStorage2DMultisample(GLenum t, GLsizei s, GLenum f, GLsizei w, GLsizei h, GLboolean fx)
{ (void)t; (void)s; (void)f; (void)w; (void)h; (void)fx; c.ms2d++; }
static GLuint mk_glCreateShader(GLenum t) { (void)t; return ++c.next_id; }
static GLuint mk_glCreateProgram(void) { return ++c.next_id; }
static void mk_glShaderSource(GLuint s, GLsizei n, const GLchar* const* str, const GLint* l)
{ (void)s; (void)n; (void)l; strncpy(c.src, str[0], sizeof c.src - 1); }
static void mk_glGetShaderiv(GLuint s, GLenum pn, GLint* o) { (void)s; *o = (pn == GL_COMPILE_STATUS) ? 1 : 0; }
static void mk_glGetProgramiv(GLuint p, GLenum pn, GLint* o) { (void)p; *o = (pn == GL_LINK_STATUS) ? 1 : 0; }
static GLsync mk_glFenceSync(GLenum cd, GLbitfield f) { (void)cd; (void)f; c.fence++; return (GLsync)(uintptr_t)0x1234; }
static GLboolean mk_glIsSync(GLsync s) { return s == (GLsync)(uintptr_t)0x1234; }
static void mk_glDrawElements(GLenum m, GLsizei n, GLenum t, const void* i) { (void)m; (void)n; (void)t; (void)i; c.draw_elements++; }
static void mk_glVertexAttribPointer(GLuint i, GLint s, GLenum t, GLboolean n, GLsizei st, const void* p)
{
    (void)i; (void)s; (void)t; (void)n; (void)st;
    c.vap++;
    if (c.nvap < 4) c.last_vap_off[c.nvap++] = (GLsizeiptr)(uintptr_t)p;
}
static void mk_glGenTextures(GLsizei n, GLuint* a) { GLsizei i; for (i = 0; i < n; i++) a[i] = ++c.next_id; }
static void mk_glGenBuffers(GLsizei n, GLuint* a) { GLsizei i; for (i = 0; i < n; i++) a[i] = ++c.next_id; }
static void mk_glGenVertexArrays(GLsizei n, GLuint* a) { GLsizei i; for (i = 0; i < n; i++) a[i] = ++c.vao_next; }

#define M(n) { #n, (void*)mk_##n }
static const struct { const char* n; void* f; } k_mock[] = {
    M(glGetIntegerv), M(glGetStringi), M(glGetError), M(glTexImage2D), M(glTexStorage2DMultisample),
    M(glCreateShader), M(glCreateProgram), M(glShaderSource), M(glGetShaderiv), M(glGetProgramiv),
    M(glFenceSync), M(glIsSync), M(glDrawElements), M(glVertexAttribPointer), M(glGenVertexArrays), M(glGenTextures), M(glGenBuffers),
};
static void* loader(const char* name)
{
    size_t i;
    for (i = 0; i < sizeof k_mock / sizeof k_mock[0]; i++)
        if (!strcmp(k_mock[i].n, name)) return k_mock[i].f;
    return (void*)mock_noop;
}

static void reinit(int maj, int min, const char* e0, const char* e1)
{
    gl31_shutdown();
    cfg_major = maj; cfg_minor = min; cfg_next = 0;
    if (e0) cfg_ext[cfg_next++] = e0;
    if (e1) cfg_ext[cfg_next++] = e1;
    memset(&c, 0, sizeof c);
    CHECK(gl31_init(loader) == 0);
    while (gl31_glGetError() != GL_NO_ERROR) {}
}

static char* conv(const char* src, GLenum t, gl31_sampler_info_t* si, const gl31_fragbind_t* fb, int nfb, char* err)
{
    return gl31_glsl_convert_ex(src, t, fb, nfb, si, err, 256);
}

static void test_glsl(void)
{
    char err[256], *o;
    gl31_sampler_info_t si;

    /* ---- ES 3.0: version y rechazos ---- */
    reinit(3, 0, NULL, NULL);
    o = conv("#version 150\nuniform sampler1D t; in float u; out vec4 c; void main(){ c = texture(t,u); }",
             GL_FRAGMENT_SHADER, &si, NULL, 0, err);
    CHECK(o != NULL);
    if (o) {
        CHECK(strstr(o, "#version 300 es"));
        CHECK(!strstr(o, "sampler1D"));
        CHECK(strstr(o, "uniform sampler2D t"));
        CHECK(strstr(o, "lorica_texture_1D_f(t,u)"));
        CHECK(strstr(o, "vec2(x,0.5)"));
        CHECK(si.n == 1 && si.s[0].type == GL_SAMPLER_1D && !strcmp(si.s[0].name, "t"));
        free(o);
    }
    o = conv("#version 150\nuniform sampler2DRect r; out vec4 c; void main(){ c = texture(r, vec2(3.0,4.0)); vec2 s = vec2(textureSize(r)); }",
             GL_FRAGMENT_SHADER, &si, NULL, 0, err);
    CHECK(o && strstr(o, "lorica_texture_RECT_f(r,") && strstr(o, "lorica_textureSize_RECT_f(r)") && strstr(o, "p/vec2(textureSize(s,0))"));
    CHECK(si.n == 1 && si.s[0].type == GL_SAMPLER_2D_RECT);
    free(o);
    o = conv("#version 150\nuniform sampler1DArray a; uniform isampler1D b, c2; out vec4 c; void main(){ c = texture(a, vec2(0.5,2.0)); ivec4 q = texelFetch(b, 1, 0); }",
             GL_FRAGMENT_SHADER, &si, NULL, 0, err);
    CHECK(o && strstr(o, "sampler2DArray a") && strstr(o, "isampler2D b") && strstr(o, "lorica_texture_1DA_f(a,") &&
          strstr(o, "lorica_texelFetch_1D_i(b,"));
    CHECK(si.n == 3);
    free(o);
    /* una funcion con parametro sampler1D no debe tragarse `float x` como nombre de sampler */
    o = conv("#version 150\nuniform sampler1D t; out vec4 c; vec4 f(sampler1D s, float x){ return texture(s,x); } void main(){ c = f(t, 0.5); }",
             GL_FRAGMENT_SHADER, &si, NULL, 0, err);
    CHECK(o && si.n == 2);
    free(o);
    /* mismo nombre de funcion sobre un sampler2D normal queda intacto */
    o = conv("#version 150\nuniform sampler2D t2; uniform sampler1D t1; out vec4 c; void main(){ c = texture(t2,vec2(0.))+texture(t1,0.5); }",
             GL_FRAGMENT_SHADER, NULL, NULL, 0, err);
    CHECK(o && strstr(o, "texture(t2,vec2(0.))") && strstr(o, "lorica_texture_1D_f(t1,"));
    free(o);
    o = conv("#version 150\nuniform sampler1D t; out vec4 c; void main(){ c = textureProj(t, vec2(1.0)); }",
             GL_FRAGMENT_SHADER, NULL, NULL, 0, err);
    CHECK(o == NULL && strstr(err, "textureProj"));
    o = conv("#version 150\nuniform sampler1DShadow t; out vec4 c; void main(){ c = vec4(0); }", GL_FRAGMENT_SHADER, NULL, NULL, 0, err);
    CHECK(o == NULL);
    o = conv("#version 150\nuniform samplerBuffer t; out vec4 c; void main(){ c = texelFetch(t,0); }", GL_FRAGMENT_SHADER, NULL, NULL, 0, err);
    CHECK(o == NULL && strstr(err, "samplerBuffer"));
    o = conv("#version 150\nuniform sampler2DMS t; out vec4 c; void main(){ c = texelFetch(t,ivec2(0),0); }", GL_FRAGMENT_SHADER, NULL, NULL, 0, err);
    CHECK(o == NULL);
    o = conv("#version 150\nlayout(triangles) in; layout(triangle_strip, max_vertices=3) out; void main(){}", GL_GEOMETRY_SHADER, NULL, NULL, 0, err);
    CHECK(o == NULL);
    o = conv("#version 150\nvoid main(){ gl_ClipDistance[0] = 1.0; gl_Position = vec4(0); }", GL_VERTEX_SHADER, NULL, NULL, 0, err);
    CHECK(o == NULL && strstr(err, "gl_ClipDistance"));
    CHECK(gl31_glCreateShader(GL_GEOMETRY_SHADER) == 0 && gl31_glGetError() == GL_INVALID_ENUM);

    /* ---- ES 3.1 + extensiones ---- */
    reinit(3, 1, "GL_EXT_geometry_shader", "GL_EXT_texture_buffer");
    CHECK(gl31_caps.glsl_es == 310 && gl31_caps.geometry && gl31_caps.tex_buffer && gl31_caps.multisample_tex);
    o = conv("#version 150\nuniform samplerBuffer t; out vec4 c; void main(){ c = texelFetch(t,0); }", GL_FRAGMENT_SHADER, NULL, NULL, 0, err);
    CHECK(o && strstr(o, "#version 310 es") && strstr(o, "GL_EXT_texture_buffer") && strstr(o, "samplerBuffer t"));
    free(o);
    o = conv("#version 150\nuniform sampler2DMS t; out vec4 c; void main(){ c = texelFetch(t,ivec2(0),0); }", GL_FRAGMENT_SHADER, NULL, NULL, 0, err);
    CHECK(o && strstr(o, "sampler2DMS t"));
    free(o);
    o = conv("#version 150\nlayout(triangles) in; layout(triangle_strip, max_vertices=3) out;\nvoid main(){ for(int i=0;i<3;i++){ gl_Position = gl_in[i].gl_Position; EmitVertex(); } EndPrimitive(); }",
             GL_GEOMETRY_SHADER, NULL, NULL, 0, err);
    CHECK(o && strstr(o, "GL_EXT_geometry_shader : require") && strstr(o, "layout(triangles) in") &&
          strstr(o, "max_vertices=3") && strstr(o, "gl_in[i]"));
    free(o);
    o = conv("#version 150\nvoid main(){ gl_ClipDistance[0] = 1.0; }", GL_VERTEX_SHADER, NULL, NULL, 0, err);
    CHECK(o == NULL);   /* sin EXT_clip_cull_distance */
    CHECK(gl31_glCreateShader(GL_GEOMETRY_SHADER) != 0);

    /* ---- ES 3.2 + clip distance: sin #extension de geometry ---- */
    reinit(3, 2, "GL_EXT_clip_cull_distance", NULL);
    CHECK(gl31_caps.glsl_es == 320);
    o = conv("#version 150\nlayout(points) in; layout(points, max_vertices=1) out; void main(){}", GL_GEOMETRY_SHADER, NULL, NULL, 0, err);
    CHECK(o && strstr(o, "#version 320 es") && !strstr(o, "GL_EXT_geometry_shader"));
    free(o);
    o = conv("#version 150\nvoid main(){ gl_ClipDistance[0] = 1.0; gl_Position = vec4(0); }", GL_VERTEX_SHADER, NULL, NULL, 0, err);
    CHECK(o && strstr(o, "GL_EXT_clip_cull_distance : require"));
    free(o);

    /* ---- frag data ---- */
    {
        gl31_fragbind_t fb[2] = { { "color1", 1, 0 }, { "color0", 0, 0 } };
        o = conv("#version 130\nout vec4 color0; out vec4 color1; layout(location=3) out vec4 other; void main(){ color0 = vec4(0); color1 = vec4(1); other = vec4(2); }",
                 GL_FRAGMENT_SHADER, NULL, fb, 2, err);
        CHECK(o && strstr(o, "layout(location = 1) out vec4 color1") && strstr(o, "layout(location = 0) out vec4 color0") &&
              strstr(o, "layout(location=3) out vec4 other"));
        free(o);
    }
}

static void test_api(void)
{
    GLuint vao = 0, buf = 0, tex = 0, sh, pr;
    GLsync s;
    GLint v = 0;
    const void* idx = 0;

    /* ---- version anunciada ---- */
    reinit(3, 0, NULL, NULL);
    CHECK(strncmp((const char*)gl31_glGetString(GL_VERSION), "3.1", 3) == 0);
    reinit(3, 2, NULL, NULL);
    CHECK(strncmp((const char*)gl31_glGetString(GL_VERSION), "3.3", 3) == 0);
    gl31_glGetIntegerv(GL_MAJOR_VERSION, &v); CHECK(v == 3);
    gl31_glGetIntegerv(GL_MINOR_VERSION, &v); CHECK(v == 3);

    /* ---- sync ---- */
    CHECK(gl31_glFenceSync(0x1234, 0) == NULL && gl31_glGetError() == GL_INVALID_ENUM);
    CHECK(gl31_glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 1) == NULL && gl31_glGetError() == GL_INVALID_VALUE);
    s = gl31_glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    CHECK(s != NULL && c.fence == 1 && gl31_glIsSync(s) && !gl31_glIsSync(NULL));
    CHECK(gl31_glClientWaitSync(s, 0x80, 0) == GL_WAIT_FAILED && gl31_glGetError() == GL_INVALID_VALUE);

    /* ---- base vertex emulado (backend sin EXT_draw_elements_base_vertex) ---- */
    reinit(3, 0, NULL, NULL);
    CHECK(!gl31_caps.base_vertex);
    {
        const char* fs = "#version 130\nout vec4 c; void main(){ c = vec4(1); }";
        GLuint fsh = gl31_glCreateShader(GL_FRAGMENT_SHADER), prg = gl31_glCreateProgram();
        gl31_glShaderSource(fsh, 1, &fs, NULL);
        gl31_glCompileShader(fsh);
        gl31_glAttachShader(prg, fsh);
        gl31_glLinkProgram(prg);
        gl31_glUseProgram(prg);
        CHECK(gl31_glGetError() == GL_NO_ERROR);
    }
    gl31_glGenVertexArrays(1, &vao);
    gl31_glBindVertexArray(vao);
    gl31_glGenBuffers(1, &buf);
    gl31_glBindBuffer(GL_ARRAY_BUFFER, buf);
    gl31_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void*)16);
    gl31_glEnableVertexAttribArray(0);
    c.vap = 0; c.nvap = 0;
    gl31_glDrawElementsBaseVertex(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, idx, 10);
    CHECK(c.draw_elements == 1);
    CHECK(c.vap == 2);                               /* desplazado y restaurado */
    CHECK(c.last_vap_off[0] == 16 + 10 * 12);        /* 3 floats = 12 bytes por vertice */
    CHECK(c.last_vap_off[1] == 16);
    c.draw_elements = 0;
    gl31_glDrawElementsBaseVertex(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, idx, 0);
    CHECK(c.draw_elements == 1 && c.vap == 2);       /* base 0: sin tocar atributos */
    gl31_glDrawElementsBaseVertex(GL_LINES_ADJACENCY, 4, GL_UNSIGNED_SHORT, idx, 0);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);     /* adyacencia sin geometry shaders */

    /* ---- provoking vertex ---- */
    gl31_glProvokingVertex(0x1234);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);
    gl31_glProvokingVertex(GL_LAST_VERTEX_CONVENTION);
    gl31_glGetIntegerv(GL_PROVOKING_VERTEX, &v);
    CHECK(v == (GLint)GL_LAST_VERTEX_CONVENTION);

    /* ---- texturas 1D ---- */
    gl31_glGenTextures(1, &tex);
    gl31_glBindTexture(GL_TEXTURE_1D, tex);
    gl31_glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK(c.tex2d == 1 && c.tex_w == 64 && c.tex_h == 1);
    gl31_glGetTexLevelParameteriv(GL_TEXTURE_1D, 0, GL_TEXTURE_WIDTH, &v);
    CHECK(v == 64);
    gl31_glGetTexLevelParameteriv(GL_TEXTURE_1D, 0, GL_TEXTURE_HEIGHT, &v);
    CHECK(v == 1);
    gl31_glTexImage1D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);
    gl31_glTexImage1D(GL_PROXY_TEXTURE_1D, 0, GL_RGBA8, 99999, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    gl31_glGetTexLevelParameteriv(GL_PROXY_TEXTURE_1D, 0, GL_TEXTURE_WIDTH, &v);
    CHECK(v == 0);

    /* ---- BGRA se sube como RGBA con R y B intercambiados ---- */
    {
        const unsigned char bgra[8] = { 10, 20, 30, 40, 50, 60, 70, 80 };   /* B G R A */
        GLuint t2 = 0;
        gl31_glGenTextures(1, &t2);
        gl31_glBindTexture(GL_TEXTURE_2D, t2);
        c.tex_has_px = 0;
        gl31_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_BGRA, GL_UNSIGNED_BYTE, bgra);
        CHECK(c.tex_has_px && c.tex_fmt == GL_RGBA);
        CHECK(c.tex_px[0] == 30 && c.tex_px[1] == 20 && c.tex_px[2] == 10 && c.tex_px[3] == 40);
        CHECK(c.tex_px[4] == 70 && c.tex_px[5] == 60 && c.tex_px[6] == 50 && c.tex_px[7] == 80);
    }

    /* ---- multisample: ES 3.0 no, ES 3.1 si ---- */
    gl31_glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);
    reinit(3, 1, NULL, NULL);
    CHECK(gl31_caps.multisample_tex && !gl31_caps.ms_array);
    gl31_glGenTextures(1, &tex);
    gl31_glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex);
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    gl31_glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, 32, 32, GL_TRUE);
    CHECK(c.ms2d == 1 && gl31_glGetError() == GL_NO_ERROR);
    gl31_glTexImage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 4, GL_RGBA8, 32, 32, 2, GL_TRUE);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glSampleMaski(1, 0);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_TEXTURE_WIDTH, &v);
    CHECK(v == 32);

    /* ---- texture buffer sin soporte ---- */
    gl31_glTexBuffer(GL_TEXTURE_BUFFER, GL_R8, 1);
    CHECK(gl31_glGetError() != GL_NO_ERROR);

    /* ---- glBindFragDataLocation: reconvierte el fragment shader al linkear ---- */
    reinit(3, 0, NULL, NULL);
    {
        const char* src = "#version 130\nout vec4 a; out vec4 b; void main(){ a = vec4(0); b = vec4(1); }";
        sh = gl31_glCreateShader(GL_FRAGMENT_SHADER);
        pr = gl31_glCreateProgram();
        gl31_glShaderSource(sh, 1, &src, NULL);
        gl31_glCompileShader(sh);
        CHECK(!strstr(c.src, "location = 1"));
        gl31_glAttachShader(pr, sh);
        gl31_glBindFragDataLocation(pr, 1, "b");
        CHECK(gl31_glGetError() == GL_NO_ERROR);
        gl31_glBindFragDataLocation(pr, 9, "a");
        CHECK(gl31_glGetError() == GL_INVALID_VALUE);
        gl31_glBindFragDataLocation(pr, 0, "gl_x");
        CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
        gl31_glLinkProgram(pr);
        CHECK(strstr(c.src, "layout(location = 1) out vec4 b"));
        CHECK(!strstr(c.src, "layout(location = 0) out vec4 a"));
    }
}

static void test_dispatch(void)
{
    static const char* const n[] = {
        "glFenceSync", "glIsSync", "glDeleteSync", "glClientWaitSync", "glWaitSync", "glGetSynciv",
        "glGetInteger64v", "glGetInteger64i_v", "glGetBufferParameteri64v",
        "glDrawElementsBaseVertex", "glDrawRangeElementsBaseVertex", "glDrawElementsInstancedBaseVertex",
        "glMultiDrawElementsBaseVertex", "glProvokingVertex", "glFramebufferTexture",
        "glTexImage1D", "glTexSubImage1D", "glCopyTexImage1D", "glCopyTexSubImage1D",
        "glCompressedTexImage1D", "glCompressedTexSubImage1D", "glTexImage2DMultisample",
        "glTexImage3DMultisample", "glGetMultisamplefv", "glSampleMaski", "glTexBuffer",
        "glGetTexImage", "glGetCompressedTexImage", "glBindFragDataLocation",
        "glSamplerParameteriv", "glSamplerParameterfv", "glSamplerParameterIiv", "glSamplerParameterIuiv",
        "glGetSamplerParameterfv", "glGetSamplerParameterIiv", "glGetSamplerParameterIuiv", NULL
    };
    int i;
    for (i = 0; n[i]; i++)
        if (!gl31_get_proc_address(n[i])) { printf("FALLO: %s no esta en la dispatch table\n", n[i]); fails++; }
}

int main(void)
{
    test_glsl();
    test_api();
    test_dispatch();
    gl31_shutdown();
    printf(fails ? "%d FALLOS\n" : "TODO OK\n", fails);
    return fails != 0;
}
