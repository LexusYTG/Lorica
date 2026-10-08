#define _POSIX_C_SOURCE 200112L
/* Pruebas de GL 3.3 sobre un backend simulado: version/GLSL anunciados, timer queries (ARB_timer_query),
 * dual-source blending (glBindFragDataLocationIndexed, layout index), glVertexAttribP* (2_10_10_10),
 * texture swizzle RGBA, queries indexadas y dispatch table. */
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
    M(glTexImage3D), M(glIsEnabled), M(glBeginQuery), M(glGenQueries), M(glVertexAttrib4f), M(glTexParameteri),
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
    gl31_set_max_version(3, 3);
    CHECK(gl31_init(loader) == 0);
    while (gl31_glGetError() != GL_NO_ERROR) {}
}



static void test_versions(void)
{
    GLint v = 0;
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    CHECK(!strncmp((const char*)gl31_glGetString(GL_VERSION), "3.3", 3));
    CHECK(!strcmp((const char*)gl31_glGetString(GL_SHADING_LANGUAGE_VERSION), "3.30"));
    gl31_glGetIntegerv(GL_MAJOR_VERSION, &v); CHECK(v == 3);
    gl31_glGetIntegerv(GL_MINOR_VERSION, &v); CHECK(v == 3);
    gl31_set_max_version(3, 2);
    CHECK(!strcmp((const char*)gl31_glGetString(GL_SHADING_LANGUAGE_VERSION), "1.50"));
    gl31_set_max_version(3, 3);
    reinit(3, 0, NULL, NULL, NULL, NULL, NULL);
    CHECK(!strncmp((const char*)gl31_glGetString(GL_VERSION), "3.1", 3));
    CHECK(!strcmp((const char*)gl31_glGetString(GL_SHADING_LANGUAGE_VERSION), "1.40"));
    reinit(3, 1, "GL_EXT_geometry_shader", NULL, NULL, NULL, NULL);
    CHECK(!strncmp((const char*)gl31_glGetString(GL_VERSION), "3.3", 3));
}

static int has_ext_str(const char* name)
{
    GLint n = 0, i;
    gl31_glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    for (i = 0; i < n; i++) {
        const GLubyte* e = gl31_glGetStringi(GL_EXTENSIONS, (GLuint)i);
        if (e && !strcmp((const char*)e, name)) return 1;
    }
    return 0;
}

static void test_extensions(void)
{
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    CHECK(has_ext_str("GL_ARB_explicit_attrib_location") && has_ext_str("GL_ARB_vertex_type_2_10_10_10_rev"));
    CHECK(has_ext_str("GL_ARB_texture_swizzle") && has_ext_str("GL_ARB_occlusion_query2"));
    CHECK(!has_ext_str("GL_ARB_timer_query") && !has_ext_str("GL_ARB_blend_func_extended"));
    reinit(3, 2, "GL_EXT_disjoint_timer_query", "GL_EXT_blend_func_extended", NULL, NULL, NULL);
    CHECK(has_ext_str("GL_ARB_timer_query") && has_ext_str("GL_ARB_blend_func_extended"));
    /* se cae a 3.1: las ARB de 3.3 no se anuncian */
    reinit(3, 0, NULL, NULL, NULL, NULL, NULL);
    CHECK(!has_ext_str("GL_ARB_explicit_attrib_location"));
}

static void test_timer(void)
{
    GLuint ids[2];
    GLint bits = -1;
    GLuint64 r = 0;
    GLint64 ri = 0;

    /* sin EXT_disjoint_timer_query: resultados 0, sin llamar al backend */
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    gl31_glGenQueries(2, ids);
    gl31_glQueryCounter(ids[0], GL_TIMESTAMP);
    CHECK(c.qcounter == 0 && gl31_glGetError() == GL_NO_ERROR);
    gl31_glGetQueryObjectui64v(ids[0], GL_QUERY_RESULT_AVAILABLE, &r); CHECK(r == 1);
    gl31_glGetQueryObjectui64v(ids[0], GL_QUERY_RESULT, &r); CHECK(r == 0);
    gl31_glGetQueryiv(GL_TIMESTAMP, 0x8864, &bits); CHECK(bits == 0);
    gl31_glBeginQuery(GL_TIME_ELAPSED, ids[1]);
    CHECK(c.beginq == 0);
    gl31_glEndQuery(GL_TIME_ELAPSED);
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    gl31_glQueryCounter(ids[0], GL_TIME_ELAPSED); CHECK(gl31_glGetError() == GL_INVALID_ENUM);

    /* con la extension: todo se reenvia */
    reinit(3, 2, "GL_EXT_disjoint_timer_query", NULL, NULL, NULL, NULL);
    CHECK(gl31_caps.timer);
    gl31_glGenQueries(2, ids);
    gl31_glQueryCounter(ids[0], GL_TIMESTAMP);
    CHECK(c.qcounter == 1);
    gl31_glGetQueryObjectui64v(ids[0], GL_QUERY_RESULT, &r);
    CHECK(c.q64 == 1 && r == 0x100000000ull);
    gl31_glGetQueryObjecti64v(ids[0], GL_QUERY_RESULT, &ri);
    CHECK(ri == 0x100000000ll);
    gl31_glGetQueryiv(GL_TIMESTAMP, 0x8864, &bits); CHECK(bits == 64);
    gl31_glBeginQuery(GL_TIME_ELAPSED, ids[1]);
    CHECK(c.beginq == 1 && c.beginq_target == GL_TIME_ELAPSED);
    gl31_glGetQueryObjectui64v(ids[1], GL_QUERY_RESULT, &r);          /* activa: error */
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glEndQuery(GL_TIME_ELAPSED);

    /* queries indexadas: solo indice 0 */
    gl31_glBeginQueryIndexed(GL_SAMPLES_PASSED, 1, ids[0]);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glBeginQueryIndexed(GL_ANY_SAMPLES_PASSED, 0, ids[0]);
    CHECK(c.beginq == 2 && gl31_glGetError() == GL_NO_ERROR);
    gl31_glEndQueryIndexed(GL_ANY_SAMPLES_PASSED, 0);
}

static GLuint make_prog(const char* fs)
{
    GLuint sh = gl31_glCreateShader(GL_FRAGMENT_SHADER), p = gl31_glCreateProgram();
    gl31_glShaderSource(sh, 1, &fs, NULL);
    gl31_glCompileShader(sh);
    gl31_glAttachShader(p, sh);
    return p;
}

static void test_dual_source(void)
{
    static const char* fs =
        "#version 330 core\nout vec4 c0; out vec4 c1; in vec2 uv;\n"
        "void main(){ c0 = vec4(uv, 0.0, 1.0); c1 = vec4(1.0); }";
    GLuint p;
    GLint idx;

    /* sin EXT_blend_func_extended: index=1 no se puede */
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    p = make_prog(fs);
    gl31_glBindFragDataLocationIndexed(p, 0, 1, "c1");
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glBindFragDataLocationIndexed(p, 0, 0, "c0");           /* indice 0: siempre valido */
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    gl31_glBindFragDataLocationIndexed(p, 0, 2, "c0");
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);

    /* con la extension */
    reinit(3, 2, "GL_EXT_blend_func_extended", NULL, NULL, NULL, NULL);
    CHECK(gl31_caps.dual_src);
    p = make_prog(fs);
    gl31_glBindFragDataLocationIndexed(p, 0, 0, "c0");
    gl31_glBindFragDataLocationIndexed(p, 0, 1, "c1");
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    gl31_glBindFragDataLocationIndexed(p, 1, 1, "c1");           /* MAX_DUAL_SOURCE_DRAW_BUFFERS = 1 */
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glLinkProgram(p);
    CHECK(strstr(c.src, "#extension GL_EXT_blend_func_extended : require"));
    CHECK(strstr(c.src, "layout(location = 0, index = 1) out vec4 c1"));
    CHECK(strstr(c.src, "layout(location = 0) out vec4 c0"));
    idx = gl31_glGetFragDataIndex(p, "c1"); CHECK(idx == 1);
    idx = gl31_glGetFragDataIndex(p, "c0"); CHECK(idx == 0);

    /* layout(index = 1) escrito a mano: se conserva con la extension... */
    {
        char err[256];
        char* o = gl31_glsl_convert_ex("#version 330 core\nlayout(location = 0, index = 0) out vec4 a;"
                                       "layout(location = 0, index = 1) out vec4 b;\nvoid main(){ a = vec4(1.0); b = vec4(0.5); }",
                                       GL_FRAGMENT_SHADER, NULL, 0, NULL, err, sizeof err);
        CHECK(o && strstr(o, "index = 1") && strstr(o, "GL_EXT_blend_func_extended"));
        free(o);
        /* ... y una variable que se llame `index` no activa la extension */
        o = gl31_glsl_convert_ex("#version 330 core\nout vec4 a; uniform int index;\nvoid main(){ a = vec4(float(index)); }",
                                 GL_FRAGMENT_SHADER, NULL, 0, NULL, err, sizeof err);
        CHECK(o && !strstr(o, "GL_EXT_blend_func_extended"));
        free(o);
    }
    /* sin la extension el layout index se descarta */
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    {
        char err[256];
        char* o = gl31_glsl_convert_ex("#version 330 core\nlayout(location = 0, index = 0) out vec4 a;\nvoid main(){ a = vec4(1.0); }",
                                       GL_FRAGMENT_SHADER, NULL, 0, NULL, err, sizeof err);
        CHECK(o && !strstr(o, "index") && !strstr(o, "blend_func_extended"));
        free(o);
    }
}

static void test_attrib_p(void)
{
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    /* UNSIGNED 2_10_10_10: x=1023, y=0, z=511, w=3 */
    gl31_glVertexAttribP4ui(1, GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, 1023u | (511u << 20) | (3u << 30));
    CHECK(c.va4 == 1 && c.va[0] == 1.f && c.va[1] == 0.f && c.va[3] == 1.f);
    CHECK(c.va[2] > 0.499f && c.va[2] < 0.501f);
    /* sin normalizar */
    gl31_glVertexAttribP4ui(1, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, 7u | (9u << 10) | (11u << 20) | (2u << 30));
    CHECK(c.va[0] == 7.f && c.va[1] == 9.f && c.va[2] == 11.f && c.va[3] == 2.f);
    /* con signo: x=-1, y=511, z=-512, w=-2 (normalizado: -1/511, 1, -1 (clamp), -1) */
    {
        GLuint v = (GLuint)(1023u) | (511u << 10) | (512u << 20) | (2u << 30);
        gl31_glVertexAttribP4ui(2, GL_INT_2_10_10_10_REV, GL_FALSE, v);
        CHECK(c.va[0] == -1.f && c.va[1] == 511.f && c.va[2] == -512.f && c.va[3] == -2.f);
        gl31_glVertexAttribP4ui(2, GL_INT_2_10_10_10_REV, GL_TRUE, v);
        CHECK(c.va[1] == 1.f && c.va[2] == -1.f && c.va[3] == -1.f);
    }
    /* P2: z=0, w=1 por defecto */
    gl31_glVertexAttribP2ui(3, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, 5u | (6u << 10) | (7u << 20));
    CHECK(c.va[0] == 5.f && c.va[1] == 6.f && c.va[2] == 0.f && c.va[3] == 1.f);
    /* variante vectorial y errores */
    {
        GLuint v = 4u;
        gl31_glVertexAttribP1uiv(0, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, &v);
        CHECK(c.va[0] == 4.f && c.va[1] == 0.f);
    }
    gl31_glVertexAttribP3ui(0, GL_FLOAT, GL_FALSE, 0);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);
    gl31_glVertexAttribP3uiv(0, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, NULL);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
}

static void test_swizzle(void)
{
    GLint s[4] = { GL_BLUE, GL_GREEN, GL_RED, GL_ONE };
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    gl31_glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, s);
    CHECK(gl31_glGetError() == GL_NO_ERROR && c.tp == 4);
    CHECK(c.tp_pn[0] == GL_TEXTURE_SWIZZLE_R && c.tp_v[0] == GL_BLUE);
    CHECK(c.tp_pn[1] == GL_TEXTURE_SWIZZLE_G && c.tp_v[1] == GL_GREEN);
    CHECK(c.tp_pn[2] == GL_TEXTURE_SWIZZLE_B && c.tp_v[2] == GL_RED);
    CHECK(c.tp_pn[3] == GL_TEXTURE_SWIZZLE_A && c.tp_v[3] == GL_ONE);
    gl31_glTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, s);
    CHECK(c.tp == 8);
}

static void test_glsl33(void)
{
    char err[256];
    char* o = gl31_glsl_convert_ex("#version 330 core\nlayout(location = 0) in vec3 p; layout(location = 1) in vec4 col;\n"
                                   "out vec4 vc;\nvoid main(){ vc = col; gl_Position = vec4(p, 1.0); }",
                                   GL_VERTEX_SHADER, NULL, 0, NULL, err, sizeof err);
    CHECK(o && strstr(o, "#version 3") && strstr(o, "es") && strstr(o, "layout(location = 1) in vec4 col"));
    free(o);
    o = gl31_glsl_convert_ex("#version 330 core\nin vec4 vc; out vec4 f;\n"
                             "void main(){ f = vec4(uintBitsToFloat(floatBitsToUint(vc.x)), vc.yzw); }",
                             GL_FRAGMENT_SHADER, NULL, 0, NULL, err, sizeof err);
    CHECK(o != NULL);
    free(o);
}

static void test_dispatch(void)
{
    static const char* const n[] = {
        "glQueryCounter", "glGetQueryObjecti64v", "glGetQueryObjectui64v", "glBeginQueryIndexed",
        "glEndQueryIndexed", "glGetQueryIndexediv", "glBindFragDataLocationIndexed", "glGetFragDataIndex",
        "glVertexAttribP1ui", "glVertexAttribP2ui", "glVertexAttribP3ui", "glVertexAttribP4ui",
        "glVertexAttribP1uiv", "glVertexAttribP2uiv", "glVertexAttribP3uiv", "glVertexAttribP4uiv",
        "glVertexAttribDivisor", "glGenSamplers", "glSamplerParameteri", "glBindSampler", NULL
    };
    int i;
    for (i = 0; n[i]; i++)
        if (!gl31_get_proc_address(n[i])) { printf("FALLO: %s no esta en la dispatch table\n", n[i]); fails++; }
}

int main(void)
{
    test_versions();
    test_extensions();
    test_timer();
    test_dual_source();
    test_attrib_p();
    test_swizzle();
    test_glsl33();
    test_dispatch();
    gl31_shutdown();
    printf(fails ? "%d FALLOS\n" : "TODO OK\n", fails);
    return fails != 0;
}
