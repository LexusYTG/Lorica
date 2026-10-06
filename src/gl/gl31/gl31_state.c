/* gl31_state.c - estado global GL 3.1 por hilo (errores, limites, samplers).
 * Enable/Disable/IsEnabled viven en gl31_render.c. */
#include "gl31.h"
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
