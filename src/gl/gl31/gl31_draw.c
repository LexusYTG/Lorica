/* gl31_draw.c - draw calls del perfil core (sin FPE, sin punteros cliente),
 * variantes con base vertex (GL 3.2) y provoking vertex. */
#include "gl31.h"
#include <stdint.h>

#ifndef GL_ELEMENT_ARRAY_BUFFER_BINDING
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#endif

static int mode_ok(GLenum m)
{
    switch (m) {
        case GL_POINTS: case GL_LINES: case GL_LINE_STRIP: case GL_LINE_LOOP:
        case GL_TRIANGLES: case GL_TRIANGLE_STRIP: case GL_TRIANGLE_FAN:
            return 1;
        /* GL 3.2: primitivas con adyacencia, solo tienen sentido con geometry shaders */
        case GL_LINES_ADJACENCY: case GL_LINE_STRIP_ADJACENCY:
        case GL_TRIANGLES_ADJACENCY: case GL_TRIANGLE_STRIP_ADJACENCY:
            return gl31_caps.geometry;
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

/* ====================== base vertex (GL 3.2) ======================
 * Con ES 3.2 o EXT/OES_draw_elements_base_vertex se reenvia. Si no, se emula
 * desplazando, solo durante el draw, el puntero de cada atributo por
 * basevertex * stride (los atributos con divisor no se ven afectados). */
static size_t attrib_type_size(GLenum t)
{
    switch (t) {
        case GL_BYTE: case GL_UNSIGNED_BYTE:                      return 1;
        case GL_SHORT: case GL_UNSIGNED_SHORT: case GL_HALF_FLOAT: return 2;
        default:                                                  return 4;
    }
}

static void shift_attribs(GLint base, int apply)
{
    gl31_state_t* s = gl31_state();
    const gl31_attrib_t* a;
    int n = 0, i, touched = 0;
    a = gl31_vao_attribs(&n);
    if (!a) return;
    for (i = 0; i < n; i++) {
        size_t eff;
        const void* p;
        if (!a[i].enabled || a[i].divisor || !a[i].buffer) continue;
        if (a[i].stride) eff = (size_t)a[i].stride;
        else if (a[i].type == GL_INT_2_10_10_10_REV || a[i].type == GL_UNSIGNED_INT_2_10_10_10_REV) eff = 4;
        else eff = (size_t)a[i].size * attrib_type_size(a[i].type);
        p = (const void*)((uintptr_t)a[i].ptr + (apply ? (uintptr_t)((intptr_t)base * (intptr_t)eff) : 0u));
        BE(glBindBuffer)(GL_ARRAY_BUFFER, a[i].buffer);
        if (a[i].integer) BE(glVertexAttribIPointer)((GLuint)i, a[i].size, a[i].type, a[i].stride, p);
        else              BE(glVertexAttribPointer)((GLuint)i, a[i].size, a[i].type, (GLboolean)a[i].normalized, a[i].stride, p);
        touched = 1;
    }
    if (touched) BE(glBindBuffer)(GL_ARRAY_BUFFER, s->array_buffer);
}

void gl31_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void* indices, GLint basevertex)
{
    if (!pre_draw_elements(mode, count, type)) return;
    apply_restart(type);
    if (gl31_caps.base_vertex) { BE(glDrawElementsBaseVertex)(mode, count, type, indices, basevertex); return; }
    if (!basevertex) { BE(glDrawElements)(mode, count, type, indices); return; }
    shift_attribs(basevertex, 1);
    BE(glDrawElements)(mode, count, type, indices);
    shift_attribs(basevertex, 0);
}

void gl31_glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type,
                                        const void* indices, GLint basevertex)
{
    if (end < start) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw_elements(mode, count, type)) return;
    apply_restart(type);
    if (gl31_caps.base_vertex) {
        BE(glDrawRangeElementsBaseVertex)(mode, start, end, count, type, indices, basevertex);
        return;
    }
    if (!basevertex) { BE(glDrawRangeElements)(mode, start, end, count, type, indices); return; }
    shift_attribs(basevertex, 1);
    BE(glDrawRangeElements)(mode, start, end, count, type, indices);
    shift_attribs(basevertex, 0);
}

void gl31_glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void* indices,
                                            GLsizei inst, GLint basevertex)
{
    if (inst < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw_elements(mode, count, type)) return;
    apply_restart(type);
    if (gl31_caps.base_vertex) {
        BE(glDrawElementsInstancedBaseVertex)(mode, count, type, indices, inst, basevertex);
        return;
    }
    if (!basevertex) { BE(glDrawElementsInstanced)(mode, count, type, indices, inst); return; }
    shift_attribs(basevertex, 1);
    BE(glDrawElementsInstanced)(mode, count, type, indices, inst);
    shift_attribs(basevertex, 0);
}

void gl31_glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei* count, GLenum type,
                                        const void* const* indices, GLsizei drawcount, const GLint* basevertex)
{
    GLsizei i;
    if (drawcount < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw_elements(mode, 0, type)) return;
    if (drawcount && (!count || !indices || !basevertex)) { gl31_set_error(GL_INVALID_VALUE); return; }
    apply_restart(type);
    for (i = 0; i < drawcount; i++) {
        if (count[i] < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
        if (count[i] == 0) continue;
        if (gl31_caps.base_vertex) {
            BE(glDrawElementsBaseVertex)(mode, count[i], type, indices[i], basevertex[i]);
        } else if (!basevertex[i]) {
            BE(glDrawElements)(mode, count[i], type, indices[i]);
        } else {
            shift_attribs(basevertex[i], 1);
            BE(glDrawElements)(mode, count[i], type, indices[i]);
            shift_attribs(basevertex[i], 0);
        }
    }
}

/* ES siempre usa el ultimo vertice (que es el valor por defecto de GL 3.2).
 * FIRST_VERTEX_CONVENTION solo existe con EXT/ANGLE_provoking_vertex. */
void gl31_glProvokingVertex(GLenum mode)
{
    gl31_state_t* s = gl31_state();
    if (mode != GL_FIRST_VERTEX_CONVENTION && mode != GL_LAST_VERTEX_CONVENTION) {
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    if (gl31_caps.provoking_vertex) {
        BE(glProvokingVertex)(mode);
        s->provoking_vertex = mode;
        return;
    }
    if (mode == GL_FIRST_VERTEX_CONVENTION) {
        gl31_stub_warn("glProvokingVertex(GL_FIRST_VERTEX_CONVENTION)");
        return;                                  /* el estado sigue siendo LAST: es lo que hace el backend */
    }
    s->provoking_vertex = mode;
}
