#define _POSIX_C_SOURCE 200112L
/* Pruebas de GL 4.0-4.3 sobre un backend simulado: version anunciada y tope, conversor GLSL
 * (compute, tessellation, images, double, gl_PerVertex), compute/imagenes/barreras, draws indirectos,
 * vertex attrib binding + base vertex/instance emulados, pipelines, texture views, copy image,
 * cube map arrays, debug, viewport arrays, clear buffer, blend indexado, dispatch table. */
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

#define M(n) { #n, (void*)mk_##n }
static const struct { const char* n; void* f; } k_mock[] = {
    M(glGetIntegerv), M(glGetStringi), M(glGetError), M(glCreateShader), M(glCreateProgram), M(glShaderSource),
    M(glGetShaderiv), M(glGetProgramiv), M(glGenTextures), M(glGenBuffers), M(glGenVertexArrays),
    M(glDispatchCompute), M(glMemoryBarrier), M(glDrawArraysIndirect), M(glDrawElementsIndirect),
    M(glBindVertexBuffer), M(glDrawElementsInstanced), M(glDrawArraysInstanced), M(glPatchParameteri),
    M(glTextureView), M(glCopyImageSubData), M(glBlendFunc), M(glBlendFunci), M(glMinSampleShading),
    M(glProgramUniform1f), M(glProgramParameteri), M(glGetBufferParameteriv), M(glBufferSubData),
    M(glTexImage3D), M(glIsEnabled),
};
static void* loader(const char* name)
{
    size_t i;
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

static char* conv(const char* src, GLenum t, char* err)
{
    return gl31_glsl_convert_ex(src, t, NULL, 0, NULL, err, 256);
}

static void test_versions(void)
{
    GLint v = 0;
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    CHECK(gl31_caps.es31 && gl31_caps.tess && gl31_caps.cube_array && gl31_caps.texture_view);
    CHECK(!strncmp((const char*)gl31_glGetString(GL_VERSION), "3.3", 3));        /* tope por defecto */
    gl31_set_max_version(4, 3);
    CHECK(!strncmp((const char*)gl31_glGetString(GL_VERSION), "4.3", 3));
    CHECK(!strcmp((const char*)gl31_glGetString(GL_SHADING_LANGUAGE_VERSION), "4.30"));
    gl31_glGetIntegerv(GL_MAJOR_VERSION, &v); CHECK(v == 4);
    gl31_glGetIntegerv(GL_MINOR_VERSION, &v); CHECK(v == 3);
    gl31_set_max_version(4, 1);
    CHECK(!strncmp((const char*)gl31_glGetString(GL_VERSION), "4.1", 3));
    CHECK(!strcmp((const char*)gl31_glGetString(GL_SHADING_LANGUAGE_VERSION), "4.10"));
    gl31_set_max_version(9, 9);                                                   /* nunca mas que lo soportado */
    CHECK(!strncmp((const char*)gl31_glGetString(GL_VERSION), "4.3", 3));

    /* ES 3.1 sin extensiones: no hay geometry -> 3.1 aunque el tope sea 4.3 */
    reinit(3, 1, NULL, NULL, NULL, NULL, NULL);
    gl31_set_max_version(4, 3);
    CHECK(!strncmp((const char*)gl31_glGetString(GL_VERSION), "3.1", 3));
    /* ES 3.1 con las extensiones de 4.0-4.2 pero sin texture view / copy image */
    reinit(3, 1, "GL_EXT_geometry_shader", "GL_EXT_tessellation_shader", "GL_EXT_texture_cube_map_array",
           "GL_OES_sample_shading", "GL_EXT_draw_buffers_indexed");
    gl31_set_max_version(4, 3);
    CHECK(!strncmp((const char*)gl31_glGetString(GL_VERSION), "4.2", 3));
    /* la variable de entorno fija el tope en gl31_init */
    setenv("LORICA_GL_MAX_VERSION", "4.0", 1);
    gl31_shutdown();
    CHECK(gl31_init(loader) == 0);
    CHECK(!strncmp((const char*)gl31_glGetString(GL_VERSION), "4.0", 3));
    unsetenv("LORICA_GL_MAX_VERSION");
    /* extensiones ARB anunciadas */
    {
        GLint n = 0, i, found = 0;
        gl31_glGetIntegerv(GL_NUM_EXTENSIONS, &n);
        for (i = 0; i < n; i++) {
            const char* e = (const char*)gl31_glGetStringi(GL_EXTENSIONS, (GLuint)i);
            if (e && !strcmp(e, "GL_ARB_compute_shader")) found = 1;
        }
        CHECK(found);
    }
}

static void test_glsl(void)
{
    char err[256], *o;
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    o = conv("#version 430\nlayout(local_size_x = 8, local_size_y = 8) in;\n"
             "layout(rgba8, binding = 0) uniform writeonly image2D img;\n"
             "layout(std430, binding = 1) buffer Data { float v[]; };\n"
             "layout(binding = 2) uniform atomic_uint cnt;\n"
             "void main(){ imageStore(img, ivec2(gl_GlobalInvocationID.xy), vec4(v[0])); atomicCounterIncrement(cnt); }",
             GL_COMPUTE_SHADER, err);
    CHECK(o != NULL);
    if (o) {
        CHECK(strstr(o, "#version 320 es"));
        CHECK(strstr(o, "local_size_x = 8"));
        CHECK(strstr(o, "binding = 0") && strstr(o, "binding = 1") && strstr(o, "binding = 2"));
        CHECK(strstr(o, "precision highp image2D;"));
        free(o);
    }
    /* double -> float, literales lf, precise, gl_PerVertex, location entre etapas */
    o = conv("#version 410\n#extension GL_ARB_gpu_shader_fp64 : enable\n"
             "layout(location = 0) in dvec3 p; layout(location = 1) in vec2 uv;\n"
             "layout(location = 0) out vec2 vuv;\n"
             "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };\n"
             "precise float k;\n"
             "void main(){ double d = 1.5lf; vuv = uv * float(d); gl_Position = vec4(vec3(p), 1.0); }",
             GL_VERTEX_SHADER, err);
    CHECK(o != NULL);
    if (o) {
        CHECK(!strstr(o, "double") && !strstr(o, "dvec3") && strstr(o, "float d = 1.5  ;"));
        CHECK(!strstr(o, "gl_PerVertex"));   /* precise es nativo en ES 3.2: se conserva */
        CHECK(strstr(o, "layout(location = 0) out vec2 vuv"));      /* ES 3.1+ lo admite entre etapas */
        free(o);
    }
    /* tessellation */
    o = conv("#version 400\nlayout(vertices = 3) out;\nvoid main(){ gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;"
             " gl_TessLevelOuter[0] = 2.0; }", GL_TESS_CONTROL_SHADER, err);
    CHECK(o && strstr(o, "layout(vertices = 3) out") && strstr(o, "gl_TessLevelOuter"));
    free(o);
    o = conv("#version 400\nlayout(triangles, equal_spacing, ccw) in;\nvoid main(){ gl_Position = gl_in[0].gl_Position; }", GL_TESS_EVALUATION_SHADER, err);
    CHECK(o != NULL);
    free(o);
    o = conv("#version 400\nlayout(vertices = 3) out;\nvoid main(){}", GL_TESS_CONTROL_SHADER, err);
    CHECK(o && !strstr(o, "GL_EXT_tessellation_shader"));        /* ES 3.2: core */
    free(o);
    /* cube map array + rechazos */
    o = conv("#version 400\nuniform samplerCubeArray s; out vec4 c; void main(){ c = texture(s, vec4(0.)); }", GL_FRAGMENT_SHADER, err);
    CHECK(o && strstr(o, "samplerCubeArray s"));
    free(o);
    CHECK(conv("#version 430\nuniform image1D i; void main(){}", GL_FRAGMENT_SHADER, err) == NULL);
    CHECK(conv("#version 400\nsubroutine void f(); void main(){}", GL_FRAGMENT_SHADER, err) == NULL);
    CHECK(conv("#version 420\nout vec4 c; void main(){ float l = textureQueryLod(sampler2D(0), vec2(0.)).x; }", GL_FRAGMENT_SHADER, err) == NULL);
    CHECK(conv("#version 330\nlayout(local_size_x=1) in; void main(){}", GL_COMPUTE_SHADER, err) == NULL);   /* version < 430 */

    /* ES 3.1 con tessellation por extension: se agrega el #extension */
    reinit(3, 1, "GL_EXT_tessellation_shader", NULL, NULL, NULL, NULL);
    CHECK(gl31_caps.tess && gl31_caps.glsl_es == 310);
    o = conv("#version 400\nlayout(vertices = 3) out;\nvoid main(){}", GL_TESS_CONTROL_SHADER, err);
    CHECK(o && strstr(o, "#version 310 es") && strstr(o, "GL_EXT_tessellation_shader : require"));
    free(o);
    o = conv("#version 420\nlayout(binding = 3) uniform sampler2D t; out vec4 c; void main(){ c = texture(t, vec2(0.)); }", GL_FRAGMENT_SHADER, err);
    CHECK(o && strstr(o, "layout(binding = 3)"));
    free(o);

    /* ES 3.0: sin compute, sin images, binding se elimina */
    reinit(3, 0, NULL, NULL, NULL, NULL, NULL);
    CHECK(conv("#version 430\nlayout(local_size_x=1) in; void main(){}", GL_COMPUTE_SHADER, err) == NULL);
    CHECK(conv("#version 420\nlayout(rgba8) uniform image2D i; void main(){}", GL_FRAGMENT_SHADER, err) == NULL);
    o = conv("#version 420\nlayout(binding = 3) uniform sampler2D t; out vec4 c; void main(){ c = texture(t, vec2(0.)); }", GL_FRAGMENT_SHADER, err);
    CHECK(o && !strstr(o, "binding"));
    free(o);
}

static GLuint make_program(void)
{
    const char* fs = "#version 130\nout vec4 c; void main(){ c = vec4(1); }";
    GLuint sh = gl31_glCreateShader(GL_FRAGMENT_SHADER), pr = gl31_glCreateProgram();
    gl31_glShaderSource(sh, 1, &fs, NULL);
    gl31_glCompileShader(sh);
    gl31_glAttachShader(pr, sh);
    gl31_glLinkProgram(pr);
    gl31_glUseProgram(pr);
    return pr;
}

static void test_compute_and_images(void)
{
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    gl31_glDispatchCompute(1, 1, 1);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION && c.dispatch == 0);        /* sin programa */
    make_program();
    gl31_glDispatchCompute(4, 2, 1);
    CHECK(gl31_glGetError() == GL_NO_ERROR && c.dispatch == 1);
    gl31_glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    CHECK(c.barrier == 1 && c.barrier_bits == GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    gl31_glMemoryBarrier(0x4000);                   /* CLIENT_MAPPED_BUFFER: sin equivalente -> barrera completa */
    CHECK(c.barrier == 2 && c.barrier_bits == 0x3FEF);
    gl31_glMemoryBarrier(GL_ALL_BARRIER_BITS);
    CHECK(c.barrier_bits == 0x3FEF);
    gl31_glBindImageTexture(8, 1, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glBindImageTexture(0, 1, 0, GL_FALSE, 0, 0x1234, GL_RGBA8);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);
    gl31_glBindImageTexture(0, 1, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    gl31_glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    gl31_glShaderStorageBlockBinding(1, 0, 0);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);

    reinit(3, 0, NULL, NULL, NULL, NULL, NULL);
    gl31_glDispatchCompute(1, 1, 1);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glBindImageTexture(0, 1, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);
}

static void test_draws(void)
{
    GLuint vao = 0, buf = 0;
    const void* idx = 0;
    /* ---- indirectos ---- */
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    make_program();
    gl31_glGenVertexArrays(1, &vao); gl31_glBindVertexArray(vao);
    gl31_glDrawArraysIndirect(GL_TRIANGLES, (const void*)32);
    CHECK(c.ind_arrays == 1 && c.ind_off[0] == 32);
    c.ind_arrays = 0;
    gl31_glMultiDrawArraysIndirect(GL_TRIANGLES, (const void*)8, 3, 0);
    CHECK(c.ind_arrays == 3 && c.ind_off[0] == 8 && c.ind_off[1] == 24 && c.ind_off[2] == 40);
    gl31_glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, (const void*)0, 4, 32);
    CHECK(c.ind_elems == 4);
    gl31_glMultiDrawArraysIndirect(GL_TRIANGLES, (const void*)0, 2, 3);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);                               /* stride no multiplo de 4 */
    gl31_glDrawTransformFeedback(GL_POINTS, 1);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    /* tessellation */
    gl31_glPatchParameteri(GL_PATCH_VERTICES, 3);
    CHECK(c.patch == 1);
    gl31_glPatchParameteri(GL_PATCH_VERTICES, 0);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glDrawArrays(GL_PATCHES, 0, 3);
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    gl31_glMinSampleShading(0.5f);
    CHECK(c.minss == 1);

    /* ---- vertex attrib binding + base vertex / base instance emulados (ES 3.1 sin EXT_base_*) ---- */
    reinit(3, 1, NULL, NULL, NULL, NULL, NULL);
    CHECK(gl31_caps.es31 && !gl31_caps.base_vertex && !gl31_caps.base_instance);
    make_program();
    gl31_glGenVertexArrays(1, &vao); gl31_glBindVertexArray(vao);
    gl31_glGenBuffers(1, &buf);
    gl31_glBindBuffer(GL_ARRAY_BUFFER, buf);
    gl31_glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    gl31_glVertexAttribBinding(0, 2);                                           /* atributo 0 usa el binding 2 */
    gl31_glBindVertexBuffer(2, buf, 16, 12);
    gl31_glEnableVertexAttribArray(0);
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    c.bvb = 0;
    gl31_glDrawElementsBaseVertex(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, idx, 10);
    CHECK(c.bvb == 2 && c.bvb_off[0] == 16 + 10 * 12 && c.bvb_off[1] == 16 && c.bvb_stride[0] == 12);
    /* atributo instanciado: baseinstance desplaza por baseinstance/divisor * stride */
    gl31_glVertexBindingDivisor(2, 2);
    c.bvb = 0;
    gl31_glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 3, 4, 6);
    CHECK(c.draw_arrays_inst == 1 && c.bvb == 2 && c.bvb_off[0] == 16 + 3 * 12 && c.bvb_off[1] == 16);
    gl31_glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 3, 4, 5);          /* 5 no es multiplo de 2 */
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    /* VertexAttribPointer sigue funcionando y tambien entra al modelo */
    gl31_glVertexBindingDivisor(2, 0);
    gl31_glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (const void*)8);
    gl31_glEnableVertexAttribArray(1);
    c.bvb = 0;
    gl31_glDrawElementsBaseVertex(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, idx, 2);
    CHECK(c.bvb == 4);                                                          /* 2 bindings x (aplicar + restaurar) */
    /* ES 3.0: ninguna de las funciones de binding existe */
    reinit(3, 0, NULL, NULL, NULL, NULL, NULL);
    make_program();
    gl31_glGenVertexArrays(1, &vao); gl31_glBindVertexArray(vao);
    gl31_glBindVertexBuffer(0, 1, 0, 4);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glDrawArrays(GL_PATCHES, 0, 3);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);
}

static void test_pipelines(void)
{
    const char* vs = "#version 410\nvoid main(){ gl_Position = vec4(0); }";
    GLuint pr;
    GLuint vao = 0;
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    pr = gl31_glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &vs);
    CHECK(pr != 0 && c.progparam == 1);
    {
        GLint ok = 0;
        gl31_glGetProgramiv(pr, GL_LINK_STATUS, &ok);
        CHECK(ok == GL_TRUE);
    }
    gl31_glProgramUniform1f(pr, 3, 1.0f);
    CHECK(c.pu1f == 1);
    gl31_glProgramUniform1f(pr, -1, 1.0f);
    CHECK(c.pu1f == 1 && gl31_glGetError() == GL_NO_ERROR);
    gl31_glProgramUniform1f(9999, 0, 1.0f);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glGenVertexArrays(1, &vao); gl31_glBindVertexArray(vao);
    gl31_glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);                           /* ni programa ni pipeline */
    gl31_glBindProgramPipeline(7);
    gl31_glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    gl31_glProgramParameteri(pr, 0x1234, 1);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);
    reinit(3, 0, NULL, NULL, NULL, NULL, NULL);
    CHECK(gl31_glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &vs) == 0);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
}

static void test_textures(void)
{
    GLuint a = 0, b = 0, cube = 0;
    GLint v = 0;
    reinit(3, 2, NULL, NULL, NULL, NULL, NULL);
    gl31_glGenTextures(1, &a);
    gl31_glBindTexture(GL_TEXTURE_2D, a);
    gl31_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    b = 900;
    gl31_glTextureView(b, GL_TEXTURE_2D, a, GL_RGBA8, 0, 1, 0, 1);
    CHECK(gl31_glGetError() == GL_NO_ERROR && c.tview == 1);
    gl31_glBindTexture(GL_TEXTURE_2D, b);
    gl31_glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &v);
    CHECK(v == 64);
    gl31_glTextureView(b, GL_TEXTURE_2D, a, GL_RGBA8, 0, 1, 0, 1);              /* ya especificada */
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glTextureView(a, GL_TEXTURE_2D, a, GL_RGBA8, 0, 1, 0, 1);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);

    /* copy image: 2D normal y 1D array (la capa va de Y a Z) */
    gl31_glCopyImageSubData(a, GL_TEXTURE_2D, 0, 1, 2, 0, b, GL_TEXTURE_2D, 0, 3, 4, 0, 8, 8, 1);
    CHECK(c.copyimg == 1 && c.ci_args[1] == GL_TEXTURE_2D && c.ci_args[4] == 2 && c.ci_args[12] == 8 && c.ci_args[14] == 1);
    gl31_glCopyImageSubData(a, GL_TEXTURE_1D_ARRAY, 0, 1, 5, 0, b, GL_TEXTURE_1D_ARRAY, 0, 3, 7, 0, 16, 2, 1);
    CHECK(c.ci_args[1] == GL_TEXTURE_2D_ARRAY && c.ci_args[4] == 0 && c.ci_args[5] == 5 &&
          c.ci_args[10] == 0 && c.ci_args[11] == 7 && c.ci_args[13] == 1 && c.ci_args[14] == 2);
    gl31_glCopyImageSubData(a, GL_TEXTURE_BUFFER, 0, 0, 0, 0, b, GL_TEXTURE_2D, 0, 0, 0, 0, 1, 1, 1);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);

    /* cube map array */
    gl31_glGenTextures(1, &cube);
    gl31_glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, cube);
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    gl31_glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_RGBA8, 16, 16, 12, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK(gl31_glGetError() == GL_NO_ERROR && c.tex3d == 1 && c.tex3d_target == GL_TEXTURE_CUBE_MAP_ARRAY);
    gl31_glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_RGBA8, 16, 16, 5, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, &v);
    CHECK((GLuint)v == cube);

    reinit(3, 0, NULL, NULL, NULL, NULL, NULL);
    gl31_glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 1);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);
    gl31_glTextureView(5, GL_TEXTURE_2D, 4, GL_RGBA8, 0, 1, 0, 1);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glCopyImageSubData(1, GL_TEXTURE_2D, 0, 0, 0, 0, 2, GL_TEXTURE_2D, 0, 0, 0, 0, 1, 1, 1);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
}

static void dbg_cb(GLenum s, GLenum t, GLuint id, GLenum sev, GLsizei len, const GLchar* m, const void* u)
{ (void)s; (void)t; (void)id; (void)sev; (void)len; (void)m; (void)u; c.debug_cb_calls++; }

static void test_misc(void)
{
    GLint v = 0;
    GLfloat vp[4];
    /* debug sin backend KHR_debug (ES 3.0): la callback recibe los mensajes insertados */
    reinit(3, 0, NULL, NULL, NULL, NULL, NULL);
    gl31_glDebugMessageCallback(dbg_cb, NULL);
    gl31_glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 1, GL_DEBUG_SEVERITY_NOTIFICATION, -1, "hola");
    CHECK(c.debug_cb_calls == 0);                                               /* GL_DEBUG_OUTPUT apagado */
    gl31_glEnable(GL_DEBUG_OUTPUT);
    CHECK(gl31_glIsEnabled(GL_DEBUG_OUTPUT) && gl31_glGetError() == GL_NO_ERROR);
    gl31_glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 1, GL_DEBUG_SEVERITY_NOTIFICATION, -1, "hola");
    CHECK(c.debug_cb_calls == 1);
    gl31_glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "g");
    gl31_glPopDebugGroup();
    CHECK(gl31_glGetError() == GL_NO_ERROR);

    /* viewport arrays: solo el indice 0 */
    gl31_glGetIntegerv(0x825B, &v);
    CHECK(v == 1);
    gl31_glViewportIndexedf(0, 0, 0, 100, 50);
    CHECK(gl31_glGetError() == GL_NO_ERROR);
    gl31_glViewportIndexedf(1, 0, 0, 100, 50);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glGetFloati_v(GL_VIEWPORT, 3, vp);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);

    /* blend indexado sin soporte: el buffer 0 usa el estado global */
    gl31_glBlendFunci(0, GL_ONE, GL_ZERO);
    CHECK(c.blendfunc == 1 && c.blendfunci == 0);
    gl31_glBlendFunci(2, GL_ONE, GL_ZERO);
    CHECK(c.blendfunc == 1 && c.blendfunci == 0);
    gl31_glBlendFunci(9, GL_ONE, GL_ZERO);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);
    gl31_glMinSampleShading(0.5f);                                              /* sin soporte: se ignora */
    CHECK(c.minss == 0 && gl31_glGetError() == GL_NO_ERROR);
    gl31_glPatchParameteri(GL_PATCH_VERTICES, 3);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);

    /* glClearBufferData: cero y patron R32UI */
    gl31_glBindBuffer(GL_ARRAY_BUFFER, 0);
    gl31_glClearBufferData(GL_ARRAY_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
    CHECK(c.buffsub == 1 && c.buffsub_len == 16 && c.buffsub_first == 0);
    {
        GLuint pat = 0x01020304u;
        gl31_glClearBufferData(GL_ARRAY_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &pat);
        CHECK(c.buffsub == 2 && c.buffsub_first == 0x04);                       /* little endian */
    }
    gl31_glClearBufferData(GL_ARRAY_BUFFER, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, "abcd");
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glClearBufferSubData(GL_ARRAY_BUFFER, GL_R32UI, 2, 4, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
    CHECK(gl31_glGetError() == GL_INVALID_VALUE);                               /* offset no multiplo de 4 */

    /* invalidate y consultas */
    gl31_glInvalidateTexImage(1, 0);
    gl31_glInvalidateBufferData(1);
    gl31_glInvalidateFramebuffer(0x1234, 0, NULL);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);
    gl31_glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_INTERNALFORMAT_SUPPORTED, 1, &v);
    CHECK(v == GL_TRUE);
    gl31_glGetActiveAtomicCounterBufferiv(1, 0, 0, &v);
    CHECK(gl31_glGetError() == GL_INVALID_OPERATION);
    gl31_glPatchParameterfv(0x1234, vp);
    CHECK(gl31_glGetError() == GL_INVALID_ENUM);
}

static void test_dispatch(void)
{
    static const char* const n[] = {
        "glDispatchCompute", "glDispatchComputeIndirect", "glMemoryBarrier", "glMemoryBarrierByRegion",
        "glBindImageTexture", "glBindImageTextures", "glFramebufferParameteri", "glGetFramebufferParameteriv",
        "glGetProgramInterfaceiv", "glGetProgramResourceIndex", "glGetProgramResourceName", "glGetProgramResourceiv",
        "glGetProgramResourceLocation", "glShaderStorageBlockBinding", "glCreateShaderProgramv", "glUseProgramStages",
        "glBindProgramPipeline", "glGenProgramPipelines", "glDeleteProgramPipelines", "glIsProgramPipeline",
        "glValidateProgramPipeline", "glGetProgramPipelineiv", "glGetProgramPipelineInfoLog", "glActiveShaderProgram",
        "glProgramUniform1f", "glProgramUniform4uiv", "glProgramUniformMatrix4x3fv", "glProgramParameteri",
        "glGetProgramBinary", "glProgramBinary", "glDrawArraysIndirect", "glDrawElementsIndirect",
        "glMultiDrawArraysIndirect", "glMultiDrawElementsIndirect", "glDrawTransformFeedback",
        "glDrawArraysInstancedBaseInstance", "glDrawElementsInstancedBaseInstance",
        "glDrawElementsInstancedBaseVertexBaseInstance", "glPatchParameteri", "glPatchParameterfv",
        "glMinSampleShading", "glBlendEquationi", "glBlendEquationSeparatei", "glBlendFunci", "glBlendFuncSeparatei",
        "glVertexAttribFormat", "glVertexAttribIFormat", "glVertexAttribBinding", "glBindVertexBuffer",
        "glVertexBindingDivisor", "glBindVertexBuffers", "glTextureView", "glCopyImageSubData",
        "glTexStorage2DMultisample", "glTexStorage3DMultisample", "glTexBufferRange", "glBindTextures",
        "glBindSamplers", "glBindBuffersBase", "glBindBuffersRange", "glDebugMessageCallback", "glDebugMessageControl",
        "glDebugMessageInsert", "glPushDebugGroup", "glPopDebugGroup", "glObjectLabel", "glGetObjectLabel",
        "glViewportIndexedf", "glViewportArrayv", "glScissorIndexed", "glScissorArrayv", "glDepthRangeIndexed",
        "glGetFloati_v", "glGetDoublei_v", "glDepthRangef", "glClearDepthf", "glGetShaderPrecisionFormat",
        "glGenTransformFeedbacks", "glBindTransformFeedback", "glPauseTransformFeedback",
        "glInvalidateFramebuffer", "glInvalidateSubFramebuffer", "glInvalidateTexImage", "glInvalidateBufferData",
        "glClearBufferData", "glClearBufferSubData", "glGetInternalformativ", NULL
    };
    int i;
    for (i = 0; n[i]; i++)
        if (!gl31_get_proc_address(n[i])) { printf("FALLO: %s no esta en la dispatch table\n", n[i]); fails++; }
    /* lo que ES no puede dar no se publica: las apps lo detectan por NULL */
    CHECK(gl31_get_proc_address("glUniform1d") == NULL);
    CHECK(gl31_get_proc_address("glClipControl") == NULL);
    CHECK(gl31_get_proc_address("glUniformSubroutinesuiv") == NULL);
}

int main(void)
{
    test_versions();
    test_glsl();
    test_compute_and_images();
    test_draws();
    test_pipelines();
    test_textures();
    test_misc();
    test_dispatch();
    gl31_shutdown();
    printf(fails ? "%d FALLOS\n" : "TODO OK\n", fails);
    return fails != 0;
}
