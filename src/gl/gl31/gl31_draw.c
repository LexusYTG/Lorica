/* gl31_draw.c - draw calls del perfil core (sin FPE, sin punteros cliente) */
#include "gl31.h"

#ifndef GL_ELEMENT_ARRAY_BUFFER_BINDING
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#endif

static int mode_ok(GLenum m)
{
    switch (m) {
        case GL_POINTS: case GL_LINES: case GL_LINE_STRIP: case GL_LINE_LOOP:
        case GL_TRIANGLES: case GL_TRIANGLE_STRIP: case GL_TRIANGLE_FAN:
            return 1;
    }
    return 0; /* GL_QUADS / GL_POLYGON no existen en core */
}

static int pre_draw(GLenum mode, GLsizei count)
{
    gl31_state_t* s = gl31_state();
    if (!mode_ok(mode)) { gl31_set_error(GL_INVALID_ENUM); return 0; }
    if (count < 0)      { gl31_set_error(GL_INVALID_VALUE); return 0; }
    if (!s->vao || !s->program) { gl31_set_error(GL_INVALID_OPERATION); return 0; }
    if (gl31_cond_skip()) return 0;       /* dentro de glBeginConditionalRender y query en 0 */
    gl31_tex_sync_for_draw();             /* elige 1D/2D/RECT segun el sampler del programa */
    return 1;
}

static int pre_draw_elements(GLenum mode, GLsizei count, GLenum type)
{
    if (!pre_draw(mode, count)) return 0;
    if (type != GL_UNSIGNED_BYTE && type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT) {
        gl31_set_error(GL_INVALID_ENUM);
        return 0;
    }
#ifndef GL31_SKIP_ELEMENT_CHECK
    {   /* core: los indices salen siempre de un buffer */
        GLint eb = 0;
        BE(glGetIntegerv)(GL_ELEMENT_ARRAY_BUFFER_BINDING, &eb);
        if (!eb) { gl31_set_error(GL_INVALID_OPERATION); return 0; }
    }
#endif
    return 1;
}

/* GLES solo tiene "restart" con indice fijo (0xFF / 0xFFFF / 0xFFFFFFFF segun el tipo).
 * Si el indice de la app coincide se activa; si es otro, no se puede emular
 * sin reescribir el buffer de indices: se avisa y se dibuja sin restart. */
static void apply_restart(GLenum type)
{
    gl31_state_t* s = gl31_state();
    GLuint fixed = (type == GL_UNSIGNED_BYTE)  ? 0xFFu :
                   (type == GL_UNSIGNED_SHORT) ? 0xFFFFu : 0xFFFFFFFFu;
    GLboolean want = s->prim_restart;
    if (want && s->prim_restart_index != fixed) {
        gl31_stub_warn("glPrimitiveRestartIndex(indice distinto del fijo)");
        want = GL_FALSE;
    }
    if (want != s->restart_fixed_applied) {
        if (want) BE(glEnable)(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        else      BE(glDisable)(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        s->restart_fixed_applied = want;
    }
}

void gl31_glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    if (first < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw(mode, count)) return;
    BE(glDrawArrays)(mode, first, count);
}

void gl31_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    if (!pre_draw_elements(mode, count, type)) return;
    apply_restart(type);
    BE(glDrawElements)(mode, count, type, indices);
}

void gl31_glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei inst)
{
    if (first < 0 || inst < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw(mode, count)) return;
    BE(glDrawArraysInstanced)(mode, first, count, inst);
}

void gl31_glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei inst)
{
    if (inst < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw_elements(mode, count, type)) return;
    apply_restart(type);
    BE(glDrawElementsInstanced)(mode, count, type, indices, inst);
}

void gl31_glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void* indices)
{
    if (end < start) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw_elements(mode, count, type)) return;
    apply_restart(type);
    BE(glDrawRangeElements)(mode, start, end, count, type, indices);
}

/* GLES 3.0 no tiene MultiDraw*: se emula con un bucle. */
void gl31_glMultiDrawArrays(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount)
{
    GLsizei i;
    if (drawcount < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw(mode, 0)) return;
    if (drawcount && (!first || !count)) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < drawcount; i++) {
        if (first[i] < 0 || count[i] < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
        if (count[i] > 0) BE(glDrawArrays)(mode, first[i], count[i]);
    }
}

void gl31_glMultiDrawElements(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices, GLsizei drawcount)
{
    GLsizei i;
    if (drawcount < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw_elements(mode, 0, type)) return;
    if (drawcount && (!count || !indices)) { gl31_set_error(GL_INVALID_VALUE); return; }
    apply_restart(type);
    for (i = 0; i < drawcount; i++) {
        if (count[i] < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
        if (count[i] > 0) BE(glDrawElements)(mode, count[i], type, indices[i]);
    }
}
