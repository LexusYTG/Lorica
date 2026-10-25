#define _POSIX_C_SOURCE 200112L
/* Pruebas de ARB_direct_state_access (gl31_dsa.c): enlazar / llamar / restaurar sobre un backend simulado. */
#include "../gl31.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

static int cfg_major = 3, cfg_minor = 0, cfg_next;
static const char* cfg_ext[16];
static struct {
    int dispatch, barrier; GLbitfield barrier_bits;
    int ind_arrays, ind_elems; GLintptr ind_off[8];
    int bvb; GLintptr bvb_off[8]; GLsizei bvb_stride[8];
    int draw_arrays_inst, draw_elems_inst;
    int patch, tview, copyimg, blendfunc, blendfunci, minss;
    GLuint ci_args[15];
    int pu1f, progparam, buffsub; size_t buffsub_len; unsigned char buffsub_first;
    int tex3d; GLenum tex3d_target;
    char src[4096];
    GLuint next_id;
    int debug_cb_calls;
    int qcounter, q64, beginq; GLenum beginq_target;
    int va4; GLfloat va[4];
    int tp, swz[4]; GLenum tp_pn[8]; GLint tp_v[8];
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
        case GL_MAX_RENDERBUFFER_SIZE: *v = 4096; break;
        case GL_MAX_IMAGE_UNITS: *v = 8; break;
        case GL_MAX_VERTEX_ATTRIB_BINDINGS: *v = 16; break;
        case GL_ELEMENT_ARRAY_BUFFER_BINDING: *v = 1; break;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: *v = 32; break;
        case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT: *v = 256; break;
        case GL_MAX_UNIFORM_BUFFER_BINDINGS: *v = 24; break;
        default: *v = 0;
    }
}
static const GLubyte* mk_glGetStringi(GLenum n, GLuint i) { (void)n; return i < (GLuint)cfg_next ? (const GLubyte*)cfg_ext[i] : NULL; }
static GLenum mk_glGetError(void) { return 0; }
static GLuint mk_glCreateShader(GLenum t) { (void)t; return ++c.next_id; }
static GLuint mk_glCreateProgram(void) { return ++c.next_id; }
static void mk_glShaderSource(GLuint s, GLsizei n, const GLchar* const* str, const GLint* l)
{ (void)s; (void)n; (void)l; strncpy(c.src, str[0], sizeof c.src - 1); }
static void mk_glGetShaderiv(GLuint s, GLenum pn, GLint* o) { (void)s; *o = (pn == GL_COMPILE_STATUS) ? 1 : 0; }
static void mk_glGetProgramiv(GLuint p, GLenum pn, GLint* o) { (void)p; *o = (pn == GL_LINK_STATUS) ? 1 : 0; }
static void mk_glGenTextures(GLsizei n, GLuint* a) { GLsizei i; for (i = 0; i < n; i++) a[i] = ++c.next_id; }
static void mk_glGenBuffers(GLsizei n, GLuint* a) { GLsizei i; for (i = 0; i < n; i++) a[i] = ++c.next_id; }
static void mk_glGenVertexArrays(GLsizei n, GLuint* a) { GLsizei i; for (i = 0; i < n; i++) a[i] = ++c.next_id; }
static void mk_glDispatchCompute(GLuint x, GLuint y, GLuint z) { (void)x; (void)y; (void)z; c.dispatch++; }
static void mk_glMemoryBarrier(GLbitfield b) { c.barrier++; c.barrier_bits = b; }
static void mk_glDrawArraysIndirect(GLenum m, const void* i) { (void)m; if (c.ind_arrays < 8) c.ind_off[c.ind_arrays] = (GLintptr)(uintptr_t)i; c.ind_arrays++; }
static void mk_glDrawElementsIndirect(GLenum m, GLenum t, const void* i) { (void)m; (void)t; (void)i; c.ind_elems++; }
static void mk_glBindVertexBuffer(GLuint b, GLuint buf, GLintptr off, GLsizei st)
{ (void)b; (void)buf; if (c.bvb < 8) { c.bvb_off[c.bvb] = off; c.bvb_stride[c.bvb] = st; } c.bvb++; }
static void mk_glDrawElementsInstanced(GLenum m, GLsizei n, GLenum t, const void* i, GLsizei k) { (void)m; (void)n; (void)t; (void)i; (void)k; c.draw_elems_inst++; }
static void mk_glDrawArraysInstanced(GLenum m, GLint f, GLsizei n, GLsizei k) { (void)m; (void)f; (void)n; (void)k; c.draw_arrays_inst++; }
static void mk_glPatchParameteri(GLenum p, GLint v) { (void)p; (void)v; c.patch++; }
static void mk_glTextureView(GLuint a, GLenum b, GLuint d, GLenum e, GLuint f, GLuint g, GLuint h, GLuint i)
{ (void)a; (void)b; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i; c.tview++; }
static void mk_glCopyImageSubData(GLuint a, GLenum b, GLint cc, GLint d, GLint e, GLint f, GLuint g, GLenum h, GLint i,
                                  GLint j, GLint k, GLint l, GLsizei m, GLsizei n, GLsizei o)
{
    GLuint v[15] = { a, b, (GLuint)cc, (GLuint)d, (GLuint)e, (GLuint)f, g, h, (GLuint)i, (GLuint)j, (GLuint)k, (GLuint)l, (GLuint)m, (GLuint)n, (GLuint)o };
    memcpy(c.ci_args, v, sizeof v);
    c.copyimg++;
}
static void mk_glBlendFunc(GLenum a, GLenum b) { (void)a; (void)b; c.blendfunc++; }
static void mk_glBlendFunci(GLuint i, GLenum a, GLenum b) { (void)i; (void)a; (void)b; c.blendfunci++; }
static void mk_glMinSampleShading(GLfloat v) { (void)v; c.minss++; }
static void mk_glProgramUniform1f(GLuint p, GLint l, GLfloat v) { (void)p; (void)l; (void)v; c.pu1f++; }
static void mk_glProgramParameteri(GLuint p, GLenum n, GLint v) { (void)p; (void)n; (void)v; c.progparam++; }
static void mk_glGetBufferParameteriv(GLenum t, GLenum p, GLint* o) { (void)t; (void)p; *o = 16; }
static void mk_glBufferSubData(GLenum t, GLintptr o, GLsizeiptr s, const void* d)
{ (void)t; (void)o; c.buffsub++; c.buffsub_len = (size_t)s; c.buffsub_first = d ? ((const unsigned char*)d)[0] : 0xAA; }
static void mk_glTexImage3D(GLenum t, GLint l, GLint f, GLsizei w, GLsizei h, GLsizei d, GLint b, GLenum fm, GLenum ty, const void* p)
{ (void)l; (void)f; (void)w; (void)h; (void)d; (void)b; (void)fm; (void)ty; (void)p; c.tex3d++; c.tex3d_target = t; }
static GLboolean mk_glIsEnabled(GLenum e) { (void)e; return GL_FALSE; }
static void mk_glQueryCounter(GLuint id, GLenum t) { (void)id; (void)t; c.qcounter++; }
static void mk_glGetQueryObjectui64v(GLuint id, GLenum pn, GLuint64* o) { (void)id; (void)pn; *o = 0x100000000ull; c.q64++; }
static struct { int subimg2d; GLenum subimg_t[8]; const void* subimg_p[8]; int bufdata; GLenum bufdata_t; int fbtex2d; GLenum fbtex_t; GLuint fbtex_id;
                int bindtex_calls; int blit; int drawbufs; GLuint boundfbo_draw; int ebo; } d;
static void mk_glTexSubImage2D(GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const void* p)
{ (void)l; (void)x; (void)y; (void)w; (void)h; (void)f; (void)ty; if (d.subimg2d < 8) { d.subimg_t[d.subimg2d] = t; d.subimg_p[d.subimg2d] = p; } d.subimg2d++; }
static void mk_glBufferData(GLenum t, GLsizeiptr s, const void* p, GLenum u) { (void)s; (void)p; (void)u; d.bufdata++; d.bufdata_t = t; }
static void mk_glFramebufferTexture2D(GLenum t, GLenum a, GLenum tt, GLuint id, GLint l) { (void)t; (void)a; (void)l; d.fbtex2d++; d.fbtex_t = tt; d.fbtex_id = id; }
static void mk_glBlitFramebuffer(GLint a, GLint b, GLint cc, GLint dd, GLint e, GLint f, GLint g, GLint h, GLbitfield m, GLenum fl)
{ (void)a; (void)b; (void)cc; (void)dd; (void)e; (void)f; (void)g; (void)h; (void)m; (void)fl; d.blit++; }
static void mk_glDrawBuffers(GLsizei n, const GLenum* b) { (void)n; (void)b; d.drawbufs++; }
static void mk_glGenFramebuffers(GLsizei n, GLuint* a) { GLsizei i; for (i = 0; i < n; i++) a[i] = ++c.next_id; }
static void mk_glGenRenderbuffers(GLsizei n, GLuint* a) { GLsizei i; for (i = 0; i < n; i++) a[i] = ++c.next_id; }
static void mk_glGenQueries(GLsizei n, GLuint* a) { GLsizei i; for (i = 0; i < n; i++) a[i] = ++c.next_id; }
static void mk_glBeginQuery(GLenum t, GLuint id) { (void)id; c.beginq++; c.beginq_target = t; }
static void mk_glVertexAttrib4f(GLuint i, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{ (void)i; c.va[0] = x; c.va[1] = y; c.va[2] = z; c.va[3] = w; c.va4++; }
static void mk_glTexParameteri(GLenum t, GLenum pn, GLint v)
{ (void)t; if (c.tp < 8) { c.tp_pn[c.tp] = pn; c.tp_v[c.tp] = v; } c.tp++; }

#define M(n) { #n, (void*)mk_##n }
static const struct { const char* n; void* f; } k_mock[] = {
    M(glGetIntegerv), M(glGetStringi), M(glGetError), M(glCreateShader), M(glCreateProgram), M(glShaderSource),
    M(glGetShaderiv), M(glGetProgramiv), M(glGenTextures), M(glGenBuffers), M(glGenVertexArrays),
    M(glDispatchCompute), M(glMemoryBarrier), M(glDrawArraysIndirect), M(glDrawElementsIndirect),
    M(glBindVertexBuffer), M(glDrawElementsInstanced), M(glDrawArraysInstanced), M(glPatchParameteri),
    M(glTextureView), M(glCopyImageSubData), M(glBlendFunc), M(glBlendFunci), M(glMinSampleShading),
    M(glProgramUniform1f), M(glProgramParameteri), M(glGetBufferParameteriv), M(glBufferSubData),
    M(glTexImage3D), M(glIsEnabled), M(glBeginQuery), M(glGenQueries), M(glTexSubImage2D), M(glBufferData), M(glFramebufferTexture2D),
    M(glBlitFramebuffer), M(glDrawBuffers), M(glGenFramebuffers), M(glGenRenderbuffers), M(glVertexAttrib4f), M(glTexParameteri),
};
static void* loader(const char* name)
{
    size_t i;
    if (!strcmp(name, "glQueryCounterEXT") || !strcmp(name, "glQueryCounter")) return (void*)mk_glQueryCounter;
    if (!strcmp(name, "glGetQueryObjectui64vEXT") || !strcmp(name, "glGetQueryObjectui64v")) return (void*)mk_glGetQueryObjectui64v;
    for (i = 0; i < sizeof k_mock / sizeof k_mock[0]; i++)
        if (!strcmp(k_mock[i].n, name)) return k_mock[i].f;
    return (void*)mock_noop;
}

static void reinit(int maj, int min, const char* e0, const char* e1, const char* e2, const char* e3, const char* e4)
{
    const char* all[5];
    int i;
    gl31_shutdown();
    all[0] = e0; all[1] = e1; all[2] = e2; all[3] = e3; all[4] = e4;
    cfg_major = maj; cfg_minor = min; cfg_next = 0;
    for (i = 0; i < 5; i++) if (all[i]) cfg_ext[cfg_next++] = all[i];
    memset(&c, 0, sizeof c);
    memset(&d, 0, sizeof d);
    gl31_set_max_version(3, 3);
    CHECK(gl31_init(loader) == 0);
    while (gl31_glGetError() != GL_NO_ERROR) {}
}




static GLint geti(GLenum p) { GLint v = -1; gl31_glGetIntegerv(p, &v); return v; }

static void test_textures(void)
{
    GLuint t[2], other = 0;
    unsigned char px[64];
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);

    gl31_glGenTextures(1, &other);
    gl31_glBindTexture(GL_TEXTURE_2D, other);
    gl31_glCreateTextures(GL_TEXTURE_2D, 2, t);
    CHECK(t[0] && t[1] && t[0] != t[1]);
    CHECK(geti(GL_TEXTURE_BINDING_2D) == (GLint)other);                  /* binding del usuario intacto */
    CHECK(gl31_tex_target_of(t[0]) == GL_TEXTURE_2D);
    gl31_glCreateTextures(0x1234, 1, t);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);

    /* parametros y subida sobre la textura nombrada */
    gl31_glTextureParameteri(t[0], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    CHECK(c.tp == 1 && c.tp_pn[0] == GL_TEXTURE_MIN_FILTER && c.tp_v[0] == GL_LINEAR);
    gl31_glTextureSubImage2D(t[0], 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK(d.subimg2d == 1 && d.subimg_t[0] == GL_TEXTURE_2D);
    CHECK(geti(GL_TEXTURE_BINDING_2D) == (GLint)other);

    /* textura sin tipo (solo Gen): invalida */
    {
        GLuint g = 0;
        gl31_glGenTextures(1, &g);
        gl31_glTextureParameteri(g, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
        gl31_glTextureParameteri(0, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    }

    /* cube map: TextureSubImage3D reparte en caras (z = cara inicial) */
    {
        GLuint cm;
        size_t step = 2 * 2 * 4;       /* RGBA8 2x2 */
        gl31_glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cm);
        memset(&d, 0, sizeof d);
        gl31_glTextureSubImage3D(cm, 0, 0, 0, 1, 2, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, px);
        CHECK(d.subimg2d == 2);
        CHECK(d.subimg_t[0] == GL_TEXTURE_CUBE_MAP_NEGATIVE_X && d.subimg_t[1] == GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
        CHECK((const unsigned char*)d.subimg_p[1] - (const unsigned char*)d.subimg_p[0] == (ptrdiff_t)step);
        gl31_glTextureSubImage3D(cm, 0, 0, 0, 5, 2, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, px);   /* 5+2 > 6 */
        CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    }

    /* BindTextureUnit: enlaza en esa unidad y deja la unidad activa como estaba */
    gl31_glActiveTexture(GL_TEXTURE0 + 1);
    gl31_glBindTextureUnit(3, t[1]);
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    CHECK(gl31_tex_active_unit() == 1);
    gl31_glActiveTexture(GL_TEXTURE0 + 3);
    CHECK(geti(GL_TEXTURE_BINDING_2D) == (GLint)t[1]);
    gl31_glBindTextureUnit(3, 0);
    CHECK(geti(GL_TEXTURE_BINDING_2D) == 0);
    gl31_glActiveTexture(GL_TEXTURE0);
}

static void test_buffers(void)
{
    GLuint b[2], user = 0;
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    gl31_glGenBuffers(1, &user);
    gl31_glBindBuffer(GL_COPY_WRITE_BUFFER, user);
    gl31_glCreateBuffers(2, b);
    CHECK(b[0] && b[1] && gl31_state()->copy_write_buffer == user);
    gl31_glNamedBufferData(b[0], 64, NULL, GL_STATIC_DRAW);
    CHECK(d.bufdata == 1 && gl31_state()->copy_write_buffer == user);
    gl31_glNamedBufferData(0, 64, NULL, GL_STATIC_DRAW);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glCopyNamedBufferSubData(b[0], b[1], 0, 0, 16);
    CHECK(gl31_state()->copy_write_buffer == user && gl31_state()->copy_read_buffer == 0);
    gl31_glNamedBufferSubData(b[1], 0, 4, "abcd");
    CHECK(c.buffsub == 1 && gl31_state()->copy_write_buffer == user);
}

static void test_framebuffers(void)
{
    GLuint f[2], tex, rb[1];
    GLenum bufs[1] = { GL_COLOR_ATTACHMENT0 };
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    gl31_glCreateFramebuffers(2, f);
    CHECK(f[0] && f[1] && gl31_state()->draw_fbo == 0 && gl31_state()->read_fbo == 0);
    gl31_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, f[1]);          /* binding del usuario */
    gl31_glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    gl31_glNamedFramebufferTexture(f[0], GL_COLOR_ATTACHMENT0, tex, 0);
    CHECK(d.fbtex2d == 1 && d.fbtex_t == GL_TEXTURE_2D && d.fbtex_id == tex);
    CHECK(gl31_state()->draw_fbo == f[1] && gl31_state()->read_fbo == 0);
    gl31_glNamedFramebufferDrawBuffers(f[0], 1, bufs);
    CHECK(d.drawbufs == 1 && gl31_state()->draw_fbo == f[1]);
    gl31_glBlitNamedFramebuffer(f[0], f[1], 0, 0, 4, 4, 0, 0, 4, 4, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    CHECK(d.blit == 1 && gl31_state()->draw_fbo == f[1] && gl31_state()->read_fbo == 0);
    gl31_glCreateRenderbuffers(1, rb);
    gl31_glNamedRenderbufferStorage(rb[0], GL_RGBA8, 4, 4);
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    CHECK(gl31_state()->renderbuffer == 0);
    gl31_glNamedRenderbufferStorage(0, GL_RGBA8, 4, 4);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
}

static void test_vaos(void)
{
    GLuint v[2], user = 0, ebo = 0;
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    gl31_glGenVertexArrays(1, &user);
    gl31_glBindVertexArray(user);
    gl31_glCreateVertexArrays(2, v);
    CHECK(v[0] && v[1] && gl31_state()->vao == user);
    gl31_glGenBuffers(1, &ebo);
    gl31_glVertexArrayElementBuffer(v[0], ebo);
    CHECK(gl31_state()->vao == user);
    gl31_glVertexArrayVertexBuffer(v[0], 0, ebo, 16, 32);
    CHECK(gl31_state()->vao == user && c.bvb >= 1);
    gl31_glVertexArrayAttribFormat(v[0], 0, 3, GL_FLOAT, GL_FALSE, 0);
    gl31_glVertexArrayAttribBinding(v[0], 0, 0);
    gl31_glEnableVertexArrayAttrib(v[0], 0);
    CHECK(gl31_glGetError() == GL_NO_ERROR && gl31_state()->vao == user);
    gl31_glEnableVertexArrayAttrib(0, 0);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
}

static void test_dispatch(void)
{
    static const char* const n[] = {
        "glCreateTextures", "glBindTextureUnit", "glTextureParameteri", "glTextureStorage2D", "glTextureSubImage3D",
        "glGenerateTextureMipmap", "glGetTextureImage", "glCreateBuffers", "glNamedBufferData", "glNamedBufferStorage",
        "glMapNamedBufferRange", "glUnmapNamedBuffer", "glCopyNamedBufferSubData", "glCreateFramebuffers",
        "glNamedFramebufferTexture", "glBlitNamedFramebuffer", "glClearNamedFramebufferfv", "glCreateRenderbuffers",
        "glCreateVertexArrays", "glVertexArrayElementBuffer", "glVertexArrayVertexBuffer", "glEnableVertexArrayAttrib",
        "glCreateSamplers", "glCreateQueries", "glCreateTransformFeedbacks", "glTransformFeedbackBufferBase",
        "glGetGraphicsResetStatus", NULL
    };
    int i;
    for (i = 0; n[i]; i++)
        if (!gl31_get_proc_address(n[i])) { printf("FALLO: %s no esta en la dispatch table\n", n[i]); fails++; }
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    CHECK(gl31_glGetGraphicsResetStatus() == GL_NO_ERROR);
}

int main(void)
{
    test_textures();
    test_buffers();
    test_framebuffers();
    test_vaos();
    test_dispatch();
    gl31_shutdown();
    printf(fails ? "%d FALLOS\n" : "TODO OK\n", fails);
    return fails != 0;
}
