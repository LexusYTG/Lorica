/* gl31_query.c - queries de oclusion/primitivas, render condicional y
 * glGetString / glGet*v de GL 3.1 */
#include "gl31.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

#ifndef GL_QUERY_COUNTER_BITS
#define GL_QUERY_COUNTER_BITS 0x8864
#endif
#ifndef GL_QUERY_WAIT
#define GL_QUERY_WAIT                 0x8E13
#define GL_QUERY_NO_WAIT              0x8E14
#define GL_QUERY_BY_REGION_WAIT       0x8E15
#define GL_QUERY_BY_REGION_NO_WAIT    0x8E16
#endif
#ifndef GL_POINT_SIZE
#define GL_POINT_SIZE 0x0B11
#endif
#ifndef GL_POINT_FADE_THRESHOLD_SIZE
#define GL_POINT_FADE_THRESHOLD_SIZE 0x8128
#endif
#ifndef GL_POINT_SPRITE_COORD_ORIGIN
#define GL_POINT_SPRITE_COORD_ORIGIN 0x8CA0
#endif

/* ---------- queries ---------- */
enum { Q_SAMPLES, Q_ANY, Q_ANY_CONS, Q_PRIMS_GEN, Q_TF_WRITTEN, Q_TIME, Q_COUNT };

static GL31_TLS GLuint g_active[Q_COUNT];

#define MAX_FAKE 64
static GLuint g_fake[MAX_FAKE];   /* ids usados con targets no soportados */
static int    g_nfake;

/* slot: indice interno; be: target del backend; ok: 0 si no existe en GLES */
static int classify(GLenum target, int* slot, GLenum* be, int* ok)
{
    *ok = 1;
    switch (target) {
        case GL_SAMPLES_PASSED:                  *slot = Q_SAMPLES;   *be = GL_ANY_SAMPLES_PASSED; return 1;
        case GL_ANY_SAMPLES_PASSED:              *slot = Q_ANY;       *be = target; return 1;
        case GL_ANY_SAMPLES_PASSED_CONSERVATIVE: *slot = Q_ANY_CONS;  *be = target; return 1;
        case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
                                                 *slot = Q_TF_WRITTEN; *be = target; return 1;
        case GL_PRIMITIVES_GENERATED:            *slot = Q_PRIMS_GEN; *be = 0; *ok = 0; return 1;
        case GL_TIME_ELAPSED:                    *slot = Q_TIME;      *be = 0; *ok = 0; return 1;
    }
    return 0;
}

static int is_fake(GLuint id)
{
    int i;
    for (i = 0; i < g_nfake; i++) if (g_fake[i] == id) return 1;
    return 0;
}

static void fake_add(GLuint id)
{
    if (!is_fake(id) && g_nfake < MAX_FAKE) g_fake[g_nfake++] = id;
}

static void fake_del(GLuint id)
{
    int i;
    for (i = 0; i < g_nfake; i++)
        if (g_fake[i] == id) { g_fake[i] = g_fake[--g_nfake]; return; }
}

static int is_active(GLuint id)
{
    int i;
    for (i = 0; i < Q_COUNT; i++) if (g_active[i] == id) return 1;
    return 0;
}

void gl31_glGenQueries(GLsizei n, GLuint* ids)
{
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGenQueries)(n, ids);
}

void gl31_glDeleteQueries(GLsizei n, const GLuint* ids)
{
    GLsizei i; int k;
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < n; i++) {
        if (!ids[i]) continue;
        fake_del(ids[i]);
        for (k = 0; k < Q_COUNT; k++) if (g_active[k] == ids[i]) g_active[k] = 0;
    }
    BE(glDeleteQueries)(n, ids);
}

GLboolean gl31_glIsQuery(GLuint id) { return id ? BE(glIsQuery)(id) : GL_FALSE; }

void gl31_glBeginQuery(GLenum target, GLuint id)
{
    int slot, ok; GLenum be;
    if (!classify(target, &slot, &be, &ok)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (!id || g_active[slot])               { gl31_set_error(GL_INVALID_OPERATION); return; }
    if (!ok) {
        gl31_stub_warn(target == GL_TIME_ELAPSED ? "GL_TIME_ELAPSED query" : "GL_PRIMITIVES_GENERATED query");
        fake_add(id);
    } else {
        BE(glBeginQuery)(be, id);
    }
    g_active[slot] = id;
}

void gl31_glEndQuery(GLenum target)
{
    int slot, ok; GLenum be;
    if (!classify(target, &slot, &be, &ok)) { gl31_set_error(GL_INVALID_ENUM); return; }
    if (!g_active[slot])                     { gl31_set_error(GL_INVALID_OPERATION); return; }
    if (ok) BE(glEndQuery)(be);
    g_active[slot] = 0;
}

void gl31_glGetQueryiv(GLenum target, GLenum pname, GLint* params)
{
    int slot, ok; GLenum be;
    if (!params) return;
    if (!classify(target, &slot, &be, &ok)) { gl31_set_error(GL_INVALID_ENUM); return; }
    switch (pname) {
        case GL_CURRENT_QUERY:      *params = (GLint)g_active[slot]; break;
        case GL_QUERY_COUNTER_BITS: *params = ok ? 32 : 0; break;
        default: gl31_set_error(GL_INVALID_ENUM);
    }
}

void gl31_glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params)
{
    if (!params) return;
    if (!id || is_active(id)) { gl31_set_error(GL_INVALID_OPERATION); return; }
    if (pname != GL_QUERY_RESULT && pname != GL_QUERY_RESULT_AVAILABLE) {
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    if (is_fake(id)) { *params = (pname == GL_QUERY_RESULT_AVAILABLE) ? 1u : 0u; return; }
    BE(glGetQueryObjectuiv)(id, pname, params);
}

void gl31_glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params)
{
    GLuint v = 0;
    if (!params) return;
    gl31_glGetQueryObjectuiv(id, pname, &v);
    *params = (v > (GLuint)INT_MAX) ? INT_MAX : (GLint)v;
}

/* ---------- version / extensiones que ve la app ----------
 * GL 3.2 exige geometry shaders: solo se anuncia si el backend los tiene
 * (ES 3.2 o EXT/OES_geometry_shader). Si no, se anuncia 3.1. */
int gl31_advertised_minor(void) { return gl31_caps.geometry ? 2 : 1; }

typedef struct { const char* name; int (*ok)(void); } ext_t;
static int x_always(void)  { return 1; }
static int x_texbuf(void)  { return gl31_caps.tex_buffer; }
static int x_ms(void)      { return gl31_caps.multisample_tex; }
static int x_prov(void)    { return gl31_caps.provoking_vertex; }
static int x_clamp(void)   { return gl31_caps.depth_clamp; }
static int x_aniso(void)   { return gl31_caps.aniso; }
static int x_clipd(void)   { return gl31_caps.clip_distance != 0; }

static const ext_t k_ext[] = {
    { "GL_ARB_vertex_array_object",       x_always },
    { "GL_ARB_uniform_buffer_object",     x_always },
    { "GL_ARB_copy_buffer",               x_always },
    { "GL_ARB_sampler_objects",           x_always },
    { "GL_ARB_map_buffer_range",          x_always },
    { "GL_ARB_texture_storage",           x_always },
    { "GL_ARB_draw_instanced",            x_always },
    { "GL_ARB_instanced_arrays",          x_always },
    { "GL_ARB_framebuffer_object",        x_always },
    { "GL_ARB_sync",                      x_always },
    { "GL_ARB_draw_elements_base_vertex", x_always },   /* nativo o emulado */
    { "GL_ARB_seamless_cube_map",         x_always },   /* ES siempre es seamless */
    { "GL_ARB_texture_rectangle",         x_always },   /* emulado sobre 2D */
    { "GL_ARB_texture_buffer_object",     x_texbuf },
    { "GL_ARB_texture_multisample",       x_ms },
    { "GL_ARB_provoking_vertex",          x_prov },
    { "GL_ARB_depth_clamp",               x_clamp },
    { "GL_EXT_texture_filter_anisotropic", x_aniso },
    { "GL_EXT_clip_cull_distance",        x_clipd },
};
#define N_EXT_ALL ((int)(sizeof(k_ext) / sizeof(k_ext[0])))

static int ext_count(void)
{
    int i, n = 0;
    for (i = 0; i < N_EXT_ALL; i++) if (k_ext[i].ok()) n++;
    return n;
}

static const char* ext_at(int index)
{
    int i, n = 0;
    for (i = 0; i < N_EXT_ALL; i++)
        if (k_ext[i].ok()) { if (n++ == index) return k_ext[i].name; }
    return NULL;
}

const GLubyte* gl31_glGetString(GLenum name)
{
    static GL31_TLS char renderer[192];
    static GL31_TLS char version[64];
    static GL31_TLS char exts[1536];
    const GLubyte* be;
    int i, n, w;

    switch (name) {
        case GL_VENDOR:
            return BE(glGetString)(GL_VENDOR);
        case GL_RENDERER:
            be = BE(glGetString)(GL_RENDERER);
            snprintf(renderer, sizeof renderer, "%s (Lorica GL31)", be ? (const char*)be : "unknown");
            return (const GLubyte*)renderer;
        case GL_VERSION:
            snprintf(version, sizeof version, "3.%d Lorica (GLES backend)", gl31_advertised_minor());
            return (const GLubyte*)version;
        case GL_SHADING_LANGUAGE_VERSION:
            return (const GLubyte*)(gl31_advertised_minor() >= 2 ? "1.50" : "1.40");
        case GL_EXTENSIONS: {
            size_t len = 0;
            exts[0] = 0;
            for (i = 0, n = ext_count(); i < n; i++) {
                w = snprintf(exts + len, sizeof exts - len, "%s%s", i ? " " : "", ext_at(i));
                if (w < 0 || (size_t)w >= sizeof exts - len) break;
                len += (size_t)w;
            }
            return (const GLubyte*)exts;
        }
        default:
            gl31_set_error(GL_INVALID_ENUM);
            return NULL;
    }
}

const GLubyte* gl31_glGetStringi(GLenum name, GLuint index)
{
    const char* e;
    if (name != GL_EXTENSIONS) { gl31_set_error(GL_INVALID_ENUM); return NULL; }
    if ((GLint)index < 0 || (GLint)index >= ext_count()) { gl31_set_error(GL_INVALID_VALUE); return NULL; }
    e = ext_at((int)index);
    return (const GLubyte*)e;
}

/* Valores que Lorica sirve desde su propio estado. 1 = servido en *out. */
static int local_integer(GLenum pname, GLint* out)
{
    gl31_state_t* s;
    GLboolean b;
    switch (pname) {
        case GL_MAJOR_VERSION:           *out = 3; return 1;
        case GL_MINOR_VERSION:           *out = gl31_advertised_minor(); return 1;
        case GL_NUM_EXTENSIONS:          *out = ext_count(); return 1;
        case GL_CONTEXT_PROFILE_MASK:    *out = GL_CONTEXT_CORE_PROFILE_BIT; return 1;
        case GL_CONTEXT_FLAGS:           *out = 0; return 1;
        case GL_MAX_TEXTURE_BUFFER_SIZE:
            if (gl31_caps.tex_buffer) return 0;      /* lo sabe el backend */
            *out = 0; return 1;
        case GL_MAX_RECTANGLE_TEXTURE_SIZE:
            BE(glGetIntegerv)(GL_MAX_TEXTURE_SIZE, out);
            return 1;
        default: break;
    }
    s = gl31_state();
    switch (pname) {
        case GL_PRIMITIVE_RESTART:         *out = s->prim_restart; return 1;
        case GL_PRIMITIVE_RESTART_INDEX:   *out = (GLint)s->prim_restart_index; return 1;
        case GL_POINT_SPRITE_COORD_ORIGIN: *out = (GLint)s->point_sprite_origin; return 1;
        case GL_PROVOKING_VERTEX:          *out = (GLint)s->provoking_vertex; return 1;
        case GL_DRAW_FRAMEBUFFER_BINDING:  *out = (GLint)s->draw_fbo; return 1;
        case GL_READ_FRAMEBUFFER_BINDING:  *out = (GLint)s->read_fbo; return 1;
        default: break;
    }
    if (gl31_tex_get_binding(pname, out)) return 1;          /* TEXTURE_BINDING_1D/RECT/... */
    if (gl31_soft_cap_get(pname, &b)) { *out = b; return 1; }
    return 0;
}

void gl31_glGetIntegerv(GLenum pname, GLint* data)
{
    if (!data) return;
    if (local_integer(pname, data)) return;
    BE(glGetIntegerv)(pname, data);
}

void gl31_glGetInteger64v(GLenum pname, GLint64* data)
{
    GLint v = 0;
    if (!data) return;
    if (local_integer(pname, &v)) { *data = (GLint64)v; return; }
    BE(glGetInteger64v)(pname, data);
}

void gl31_glGetInteger64i_v(GLenum target, GLuint index, GLint64* data)
{
    gl31_state_t* s = gl31_state();
    if (!data) return;
    switch (target) {
        case GL_UNIFORM_BUFFER_BINDING:
        case GL_UNIFORM_BUFFER_START:
        case GL_UNIFORM_BUFFER_SIZE:
            gl31_state_load_limits();
            if ((GLint)index >= s->max_ubo_bindings) { gl31_set_error(GL_INVALID_VALUE); return; }
            if (target == GL_UNIFORM_BUFFER_BINDING)    *data = (GLint64)s->ubo[index].buffer;
            else if (target == GL_UNIFORM_BUFFER_START) *data = (GLint64)s->ubo[index].offset;
            else                                        *data = (GLint64)s->ubo[index].size;
            return;
        default:
            BE(glGetInteger64i_v)(target, index, data);
    }
}

void gl31_glGetBooleanv(GLenum pname, GLboolean* data)
{
    GLboolean b;
    if (!data) return;
    if (pname == GL_PRIMITIVE_RESTART) { *data = gl31_state()->prim_restart; return; }
    if (gl31_soft_cap_get(pname, &b))  { *data = b; return; }
    BE(glGetBooleanv)(pname, data);
}

void gl31_glGetFloatv(GLenum pname, GLfloat* data)
{
    if (!data) return;
    switch (pname) {
        case GL_POINT_SIZE:                *data = gl31_state()->point_size; return;
        case GL_POINT_FADE_THRESHOLD_SIZE: *data = gl31_state()->point_fade_threshold; return;
        default:                           BE(glGetFloatv)(pname, data);
    }
}

/* GLES no tiene glGetDoublev: se lee como float y se ensancha. Hay que copiar
 * tantos valores como devuelve el parametro, no solo uno. */
static int float_count(GLenum pname)
{
    switch (pname) {
        case GL_VIEWPORT: case GL_SCISSOR_BOX: case GL_COLOR_CLEAR_VALUE: case GL_BLEND_COLOR:
            return 4;
        case GL_DEPTH_RANGE: case GL_ALIASED_LINE_WIDTH_RANGE:
            return 2;
        default:
            return 1;
    }
}

void gl31_glGetDoublev(GLenum pname, GLdouble* data)
{
    GLfloat f[4] = {0.f, 0.f, 0.f, 0.f};
    int k, n;
    if (!data) return;
    gl31_glGetFloatv(pname, f);
    n = float_count(pname);
    for (k = 0; k < n; k++) data[k] = (GLdouble)f[k];
}

/* ---------- render condicional (GL 3.0) ----------
 * GLES no lo tiene. Se emula evaluando el resultado de la query al empezar
 * (la query ya debe haber terminado, como exige GL) y omitiendo despues los
 * draw/clear mientras dure el bloque. Las queries de targets que no existen en
 * GLES (TIME_ELAPSED, PRIMITIVES_GENERATED) no se pueden evaluar: no omiten nada. */
static GL31_TLS int g_cond_active;
static GL31_TLS int g_cond_skip;

int gl31_cond_skip(void) { return g_cond_skip; }

void gl31_glBeginConditionalRender(GLuint id, GLenum mode)
{
    GLuint avail = 1, result = 1;
    switch (mode) {
        case GL_QUERY_WAIT: case GL_QUERY_NO_WAIT:
        case GL_QUERY_BY_REGION_WAIT: case GL_QUERY_BY_REGION_NO_WAIT:
            break;
        default: gl31_set_error(GL_INVALID_ENUM); return;
    }
    if (g_cond_active) { gl31_set_error(GL_INVALID_OPERATION); return; }
    if (!id || !BE(glIsQuery)(id)) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (is_active(id)) { gl31_set_error(GL_INVALID_OPERATION); return; }

    g_cond_skip = 0;
    if (!is_fake(id)) {
        int no_wait = (mode == GL_QUERY_NO_WAIT || mode == GL_QUERY_BY_REGION_NO_WAIT);
        if (no_wait) BE(glGetQueryObjectuiv)(id, GL_QUERY_RESULT_AVAILABLE, &avail);
        if (avail) {           /* NO_WAIT con resultado pendiente: GL permite dibujar */
            BE(glGetQueryObjectuiv)(id, GL_QUERY_RESULT, &result);
            g_cond_skip = (result == 0);
        }
    }
    g_cond_active = 1;
}

void gl31_glEndConditionalRender(void)
{
    if (!g_cond_active) { gl31_set_error(GL_INVALID_OPERATION); return; }
    g_cond_active = 0;
    g_cond_skip = 0;
}
