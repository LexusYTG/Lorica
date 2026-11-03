/* gl31_state.c - estado global GL 3.1 por hilo (errores, limites, samplers).
 * Enable/Disable/IsEnabled viven en gl31_render.c. */
#include "gl31.h"
#include <stdio.h>
#include <stdlib.h>
#if defined(__GLIBC__)
#include <execinfo.h>
#endif
#include <string.h>

static GL31_TLS gl31_state_t g_state;

/* El estado es por hilo: cada hilo lo inicializa la primera vez que lo usa,
 * no solo el que llamo a gl31_init(). */
gl31_state_t* gl31_state(void)
{
    if (!g_state.inited) {
        memset(&g_state, 0, sizeof(g_state));
        gl31_render_state_defaults(&g_state);
        g_state.inited = 1;
    }
    return &g_state;
}

void gl31_state_init(void)
{
    memset(&g_state, 0, sizeof(g_state));
    g_state.inited = 0;
    (void)gl31_state();
}

void gl31_state_load_limits(void)
{
    gl31_state_t* s = gl31_state();
    if (s->limits_loaded) return;
    s->ubo_alignment = 256;
    s->max_ubo_bindings = 24;
    s->max_tex_units = 16;
    BE(glGetIntegerv)(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &s->ubo_alignment);
    BE(glGetIntegerv)(GL_MAX_UNIFORM_BUFFER_BINDINGS, &s->max_ubo_bindings);
    BE(glGetIntegerv)(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &s->max_tex_units);
    if (s->ubo_alignment < 1) s->ubo_alignment = 1;
    if (s->max_ubo_bindings > GL31_MAX_UBO_BINDINGS) s->max_ubo_bindings = GL31_MAX_UBO_BINDINGS;
    if (s->max_tex_units > GL31_MAX_TEX_UNITS) s->max_tex_units = GL31_MAX_TEX_UNITS;
    s->limits_loaded = 1;
}

void gl31_set_error(GLenum e)
{
    gl31_state_t* s = gl31_state();
#if defined(__GLIBC__)
    /* LORICA_TRACE_ERR=1: vuelca la pila donde GLADIATOR genera un error GL (depuracion) */
    static int trace = -1;
    if (trace < 0) trace = getenv("LORICA_TRACE_ERR") != NULL;
    if (trace) {
        void* bt[12]; int n = backtrace(bt, 12);
        fprintf(stderr, "LORICA: gl31_set_error(0x%x)\n", e);
        backtrace_symbols_fd(bt, n, 2);
    }
#endif
    if (!s->error) s->error = e;
}

/* Pasa a nuestro registro los errores pendientes del backend (GL guarda solo el
 * primero), para que un error generado aqui despues no tape a uno anterior. */
void gl31_err_flush(void)
{
    GLenum e;
    int guard = 0;
    while (guard++ < 8 && (e = BE(glGetError)()) != GL_NO_ERROR)
        gl31_set_error(e);
}

GLenum gl31_glGetError(void)
{
    gl31_state_t* s = gl31_state();
    GLenum e = s->error;
    if (e) { s->error = GL_NO_ERROR; return e; }
    return BE(glGetError)();
}

void gl31_glPrimitiveRestartIndex(GLuint index)
{
    gl31_state()->prim_restart_index = index;
}

void gl31_glGenSamplers(GLsizei n, GLuint* s)
{
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGenSamplers)(n, s);
}

void gl31_glDeleteSamplers(GLsizei n, const GLuint* s)
{
    gl31_state_t* st = gl31_state();
    GLsizei i; int u;
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < n; i++)
        for (u = 0; u < GL31_MAX_TEX_UNITS; u++)
            if (s[i] && st->sampler[u] == s[i]) st->sampler[u] = 0;
    BE(glDeleteSamplers)(n, s);
}

void gl31_glBindSampler(GLuint unit, GLuint sampler)
{
    gl31_state_t* st = gl31_state();
    gl31_state_load_limits();
    if ((GLint)unit >= st->max_tex_units) { gl31_set_error(GL_INVALID_VALUE); return; }
    st->sampler[unit] = sampler;
    BE(glBindSampler)(unit, sampler);
}

void gl31_glSamplerParameteri(GLuint s, GLenum pname, GLint v) { BE(glSamplerParameteri)(s, pname, v); }
void gl31_glSamplerParameterf(GLuint s, GLenum pname, GLfloat v) { BE(glSamplerParameterf)(s, pname, v); }
GLboolean gl31_glIsSampler(GLuint s) { return BE(glIsSampler)(s); }
void gl31_glGetSamplerParameteriv(GLuint s, GLenum pname, GLint* v) { BE(glGetSamplerParameteriv)(s, pname, v); }

/* error pendiente sin consumirlo (uso interno) */
GLenum gl31_err_peek(void) { return gl31_state()->error; }

/* ---- parametros de sampler (vectoriales y enteros sin signo/con signo) ---- */
/* GL_TEXTURE_BORDER_COLOR por sampler: se guarda siempre (para los getters, incluidos los enteros) y se
 * reenvia al backend solo si tiene EXT_texture_border_clamp. Sin ella, GL_CLAMP_TO_BORDER no existe en
 * el backend y las lecturas fuera de la textura usan el borde mas cercano (CLAMP_TO_EDGE). */
typedef struct { GLuint id; GLfloat f[4]; GLint i[4]; int is_int; } sbord_t;
static sbord_t* g_sb; static size_t g_nsb, g_csb;

static sbord_t* sb_get(GLuint id, int create)
{
    size_t k;
    for (k = 0; k < g_nsb; k++) if (g_sb[k].id == id) return &g_sb[k];
    if (!create) return NULL;
    if (g_nsb == g_csb) {
        size_t c = g_csb ? g_csb * 2 : 16;
        sbord_t* p = (sbord_t*)realloc(g_sb, c * sizeof *p);
        if (!p) return NULL;
        g_sb = p; g_csb = c;
    }
    memset(&g_sb[g_nsb], 0, sizeof g_sb[0]);
    g_sb[g_nsb].id = id;
    return &g_sb[g_nsb++];
}

static void sb_set_f(GLuint id, const GLfloat* v)
{
    sbord_t* b = sb_get(id, 1);
    if (!b) return;
    memcpy(b->f, v, sizeof b->f); b->is_int = 0;
}

static void sb_set_i(GLuint id, const GLint* v)
{
    sbord_t* b = sb_get(id, 1);
    if (!b) return;
    memcpy(b->i, v, sizeof b->i); b->is_int = 1;
}

static void sb_get_i(GLuint id, GLint* out)
{
    sbord_t* b = sb_get(id, 0);
    int k;
    if (!b) { memset(out, 0, 4 * sizeof *out); return; }
    if (b->is_int) { memcpy(out, b->i, sizeof b->i); return; }
    for (k = 0; k < 4; k++) {                  /* de float a entero: [-1,1] -> [-INT_MAX, INT_MAX] */
        double x = b->f[k]; x = x < -1.0 ? -1.0 : x > 1.0 ? 1.0 : x;
        out[k] = (GLint)(x * 2147483647.0);
    }
}

void gl31_glSamplerParameteriv(GLuint s, GLenum pname, const GLint* v)
{
    if (!v) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (pname == 0x1004) {                      /* TEXTURE_BORDER_COLOR: entero normalizado a [0,1] como float */
        GLfloat f[4]; int k;
        for (k = 0; k < 4; k++) { f[k] = (GLfloat)((double)v[k] / 2147483647.0); if (f[k] < -1.f) f[k] = -1.f; }
        sb_set_f(s, f);
        if (gl31_caps.border_clamp) BE(glSamplerParameterfv)(s, pname, f);
        return;
    }
    BE(glSamplerParameteriv)(s, pname, v);
}
void gl31_glSamplerParameterfv(GLuint s, GLenum pname, const GLfloat* v)
{
    if (!v) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (pname == 0x1004) {
        sb_set_f(s, v);
        if (gl31_caps.border_clamp) BE(glSamplerParameterfv)(s, pname, v);
        return;
    }
    BE(glSamplerParameterfv)(s, pname, v);
}
void gl31_glGetSamplerParameterfv(GLuint s, GLenum pname, GLfloat* v)
{
    if (!v) return;
    if (pname == 0x1004) {
        sbord_t* b = sb_get(s, 0);
        int k;
        if (!b) { memset(v, 0, 4 * sizeof *v); return; }
        for (k = 0; k < 4; k++) v[k] = b->is_int ? (GLfloat)b->i[k] : b->f[k];
        return;
    }
    BE(glGetSamplerParameterfv)(s, pname, v);
}
void gl31_glSamplerParameterIiv(GLuint s, GLenum pname, const GLint* v)
{
    if (!v) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (pname == 0x1004) {
        sb_set_i(s, v);
        if (gl31_caps.border_clamp && gl31_be.glSamplerParameterIiv) BE(glSamplerParameterIiv)(s, pname, v);
        return;
    }
    BE(glSamplerParameteriv)(s, pname, v);     /* el resto de parametros son escalares: igual que la ruta entera */
}
void gl31_glSamplerParameterIuiv(GLuint s, GLenum pname, const GLuint* v)
{
    if (!v) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (pname == 0x1004) {
        sb_set_i(s, (const GLint*)v);
        if (gl31_caps.border_clamp && gl31_be.glSamplerParameterIuiv) BE(glSamplerParameterIuiv)(s, pname, v);
        return;
    }
    BE(glSamplerParameteriv)(s, pname, (const GLint*)v);
}
void gl31_glGetSamplerParameterIiv(GLuint s, GLenum pname, GLint* v)
{
    if (!v) return;
    if (pname == 0x1004) { sb_get_i(s, v); return; }
    BE(glGetSamplerParameteriv)(s, pname, v);
}
void gl31_glGetSamplerParameterIuiv(GLuint s, GLenum pname, GLuint* v)
{
    if (!v) return;
    if (pname == 0x1004) { sb_get_i(s, (GLint*)v); return; }
    BE(glGetSamplerParameteriv)(s, pname, (GLint*)v);
}

/* GL 4.4: glBindSamplers (emulado con un bucle) */
void gl31_glBindSamplers(GLuint first, GLsizei count, const GLuint* samplers)
{
    GLsizei i;
    if (count < 0) { gl31_set_error(GL_INVALID_OPERATION); return; }
    for (i = 0; i < count; i++) {
        gl31_glBindSampler(first + (GLuint)i, samplers ? samplers[i] : 0);
        if (gl31_state()->error != GL_NO_ERROR) return;
    }
}
