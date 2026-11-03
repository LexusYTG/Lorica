/* gl31_xfb.c - transform feedback (GL 3.0 core): objeto implícito, sin pausa/reanudación.
 * El estado "activo" se recuerda aquí porque GL lo usa para rechazar UseProgram,
 * Begin anidado y cambios de programa mientras se captura. */
#include "gl31.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS 0x8C8B
#endif

/* Por cada objeto de TF se lleva la cuenta de vertices capturados desde su ultimo Begin, que es lo que
 * glDrawTransformFeedback necesita (ES 3.0 no lo expone). Sin geometry/tessellation la cuenta sale de los
 * parametros de cada draw; con ellas (la salida depende del shader) se usa una query interna de
 * GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, siempre que la app no tenga otra activa en ese target. */
typedef struct {
    GLuint id; GLenum mode; uint64_t verts;
    int exact;               /* 1: cuenta aritmetica; 0: query interna */
    GLuint q; int qactive, qok;
} xfbo_t;
static xfbo_t* g_xo; static size_t g_nxo, g_cxo;

static xfbo_t* xo_find(GLuint id)
{
    size_t i;
    for (i = 0; i < g_nxo; i++) if (g_xo[i].id == id) return &g_xo[i];
    return NULL;
}

static xfbo_t* xo_get(GLuint id)
{
    xfbo_t* x = xo_find(id);
    if (x) return x;
    if (g_nxo == g_cxo) {
        size_t nc = g_cxo ? g_cxo * 2 : 16;
        xfbo_t* n = (xfbo_t*)realloc(g_xo, nc * sizeof *n);
        if (!n) return NULL;
        g_xo = n; g_cxo = nc;
    }
    x = &g_xo[g_nxo++];
    memset(x, 0, sizeof *x);
    x->id = id;
    return x;
}

void gl31_xfb_forget(GLuint id)
{
    xfbo_t* x = xo_find(id);
    if (!x) return;
    if (x->q) BE(glDeleteQueries)(1, &x->q);
    *x = g_xo[--g_nxo];
}

static uint64_t prim_count(GLenum mode, uint64_t n)
{
    switch (mode) {
        case GL_POINTS:         return n;
        case GL_LINES:          return n / 2;
        case GL_LINE_STRIP:     return n >= 2 ? n - 1 : 0;
        case GL_LINE_LOOP:      return n >= 2 ? n : 0;
        case GL_TRIANGLES:      return n / 3;
        case GL_TRIANGLE_STRIP: case GL_TRIANGLE_FAN: return n >= 3 ? n - 2 : 0;
        case GL_LINES_ADJACENCY:          return n / 4;
        case GL_LINE_STRIP_ADJACENCY:     return n >= 4 ? n - 3 : 0;
        case GL_TRIANGLES_ADJACENCY:      return n / 6;
        case GL_TRIANGLE_STRIP_ADJACENCY: return n >= 6 ? (n - 4) / 2 : 0;
    }
    return 0;
}

static unsigned vpp(GLenum xfb_mode) { return xfb_mode == GL_POINTS ? 1u : xfb_mode == GL_LINES ? 2u : 3u; }

/* lo llama gl31_draw.c tras cada draw (con los parametros originales) */
void gl31_xfb_account(GLenum mode, GLsizei count, GLsizei inst)
{
    gl31_state_t* s = gl31_state();
    uint64_t prims;
    if (count <= 0 || inst <= 0) return;
    prims = prim_count(mode, (uint64_t)count) * (uint64_t)inst;
    gl31_query_account_prims(prims);                   /* GL_PRIMITIVES_GENERATED */
    if (s->xfb_active && !s->xfb_paused) {
        xfbo_t* x = xo_find(s->xfb_bound);
        if (x && x->exact) x->verts += prims * vpp(x->mode);
    }
}

void gl31_glBeginTransformFeedback(GLenum mode)
{
    gl31_state_t* s = gl31_state();
    xfbo_t* x;
    if (mode != GL_POINTS && mode != GL_LINES && mode != GL_TRIANGLES) {
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    if (s->xfb_active || !s->program) {
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    gl31_err_flush();
    BE(glBeginTransformFeedback)(mode);
    s->xfb_active = 1;
    s->xfb_paused = 0;
    x = xo_get(s->xfb_bound);
    if (!x) return;
    x->mode = mode; x->verts = 0; x->qok = 0;
    x->exact = !gl31_program_staged(s->program);
    if (!x->exact && !gl31_query_target_active(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN)) {
        if (!x->q) BE(glGenQueries)(1, &x->q);
        BE(glBeginQuery)(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, x->q);
        x->qactive = 1; x->qok = 1;
    }
}

void gl31_glEndTransformFeedback(void)
{
    gl31_state_t* s = gl31_state();
    xfbo_t* x;
    if (!s->xfb_active) {
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    gl31_err_flush();
    x = xo_find(s->xfb_bound);
    if (x && x->qactive) { BE(glEndQuery)(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN); x->qactive = 0; }
    BE(glEndTransformFeedback)();
    s->xfb_active = 0;
    s->xfb_paused = 0;
}

/* glDrawTransformFeedback* (GL 4.0): dibuja los vertices que capturo el objeto en su ultima captura */
void gl31_glDrawTransformFeedbackInstanced(GLenum mode, GLuint id, GLsizei inst)
{
    gl31_state_t* s = gl31_state();
    xfbo_t* x;
    uint64_t verts = 0;
    if (inst < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (id && !BE(glIsTransformFeedback)(id)) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (s->xfb_active && !s->xfb_paused && id == s->xfb_bound) { gl31_set_error(GL_INVALID_OPERATION); return; }
    x = xo_find(id);
    if (x) {
        if (x->exact) verts = x->verts;
        else if (x->qok && !x->qactive) {
            GLuint r = 0;
            BE(glGetQueryObjectuiv)(x->q, GL_QUERY_RESULT, &r);    /* espera a la GPU: unico dato que sale de ella */
            verts = (uint64_t)r * vpp(x->mode);
        } else {
            gl31_stub_warn("glDrawTransformFeedback con geometry/tessellation y una query TF_PRIMITIVES_WRITTEN activa");
            gl31_set_error(GL_INVALID_OPERATION);
            return;
        }
    }
    if (verts > 0x7FFFFFFFu) verts = 0x7FFFFFFFu;
    if (verts == 0) return;
    gl31_glDrawArraysInstanced(mode, 0, (GLsizei)verts, inst);
}

void gl31_glDrawTransformFeedback(GLenum mode, GLuint id) { gl31_glDrawTransformFeedbackInstanced(mode, id, 1); }

void gl31_glDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream)
{
    if (stream != 0) { gl31_set_error(GL_INVALID_VALUE); return; }   /* GL_MAX_VERTEX_STREAMS == 1 */
    gl31_glDrawTransformFeedbackInstanced(mode, id, 1);
}

void gl31_glTransformFeedbackVaryings(GLuint program, GLsizei count, 
                                      const GLchar* const* varyings, GLenum bufferMode)
{
    if (!program || count < 0) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    if (count > 0 && !varyings) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    BE(glTransformFeedbackVaryings)(program, count, varyings, bufferMode);
}

void gl31_glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize,
                                        GLsizei* length, GLsizei* size, GLenum* type,
                                        GLchar* name)
{
    if (bufSize < 0) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    BE(glGetTransformFeedbackVarying)(program, index, bufSize, length, size, type, name);
}
