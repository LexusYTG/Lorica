/* gl31_draw.c - draw calls del perfil core (sin FPE, sin punteros cliente),
 * variantes con base vertex (GL 3.2) y provoking vertex. */
#include "gl31.h"
#include "gl31_idx.h"
#include <stdint.h>
#include <string.h>

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
        case GL_PATCHES:                      /* GL 4.0: tessellation */
            return gl31_caps.tess;
    }
    return 0; /* GL_QUADS / GL_POLYGON no existen en core */
}

static int pre_draw(GLenum mode, GLsizei count)
{
    gl31_state_t* s = gl31_state();
    if (!mode_ok(mode)) { gl31_set_error(GL_INVALID_ENUM); return 0; }
    if (count < 0)      { gl31_set_error(GL_INVALID_VALUE); return 0; }
    if (!s->vao || (!s->program && !s->pipeline)) { gl31_set_error(GL_INVALID_OPERATION); return 0; }
    if (gl31_cond_skip()) return 0;       /* dentro de glBeginConditionalRender y query en 0 */
    gl31_tex_sync_for_draw();             /* elige 1D/2D/RECT segun el sampler del programa */
    gl31_lu_sync();                       /* tamano de punto, clip control, clip distances... */
    gl31_ssbo_sync();                     /* glShaderStorageBlockBinding */
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

/* GLES solo tiene "restart" con indice fijo (0xFF / 0xFFFF / 0xFFFFFFFF segun el tipo). Un indice
 * distinto lo resuelve el motor de reescritura de indices (gl31_idxgen.c) en la GPU; si eso no esta
 * disponible, se avisa y se dibuja sin restart. */
static void set_restart_fixed(GLboolean want)
{
    gl31_state_t* s = gl31_state();
    if (want != s->restart_fixed_applied) {
        if (want) BE(glEnable)(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        else      BE(glDisable)(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        s->restart_fixed_applied = want;
    }
}

static void apply_restart(GLenum type)
{
    gl31_state_t* s = gl31_state();
    GLuint fixed = (type == GL_UNSIGNED_BYTE)  ? 0xFFu :
                   (type == GL_UNSIGNED_SHORT) ? 0xFFFFu : 0xFFFFFFFFu;
    GLboolean want = s->prim_restart;
    if (want && s->prim_restart_index != fixed) {
        gl31_stub_warn("glPrimitiveRestartIndex(indice distinto del fijo) sin compute de GPU disponible");
        want = GL_FALSE;
    }
    set_restart_fixed(want);
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

/* Desplaza los datos de los atributos: basevertex*stride para los de divisor 0 y
 * (baseinstance/divisor)*stride para los instanciados. `apply`=0 restaura.
 * Con ES 3.1 (vertex attrib binding) se mueve el offset del binding; sin el, el puntero. */
static void shift_attribs(GLint base, GLuint binst, int apply)
{
    gl31_state_t* s = gl31_state();
    const gl31_attrib_t* a;
    int n = 0, i, touched = 0;
    a = gl31_vao_attribs(&n);
    if (!a) return;
    if (gl31_caps.es31) {
        int nb = 0;
        const gl31_vbind_t* b = gl31_vao_bindings(&nb);
        unsigned char done[GL31_MAX_ATTRIBS];
        memset(done, 0, sizeof done);
        for (i = 0; i < n; i++) {
            GLuint bi = a[i].binding;
            intptr_t amount;
            if (!a[i].enabled || bi >= (GLuint)nb || done[bi] || !b[bi].buffer) continue;
            done[bi] = 1;
            amount = b[bi].divisor ? (intptr_t)(binst / b[bi].divisor) : (intptr_t)base;
            if (!amount) continue;
            BE(glBindVertexBuffer)(bi, b[bi].buffer,
                                   b[bi].offset + (apply ? (GLintptr)(amount * (intptr_t)b[bi].stride) : 0),
                                   b[bi].stride);
        }
        return;
    }
    for (i = 0; i < n; i++) {
        size_t eff;
        intptr_t amount;
        const void* p;
        if (!a[i].enabled || !a[i].buffer) continue;
        amount = a[i].divisor ? (intptr_t)(binst / a[i].divisor) : (intptr_t)base;
        if (a[i].stride) eff = (size_t)a[i].stride;
        else if (a[i].type == GL_INT_2_10_10_10_REV || a[i].type == GL_UNSIGNED_INT_2_10_10_10_REV) eff = 4;
        else eff = (size_t)a[i].size * attrib_type_size(a[i].type);
        p = (const void*)((uintptr_t)a[i].ptr + (apply ? (uintptr_t)(amount * (intptr_t)eff) : 0u));
        BE(glBindBuffer)(GL_ARRAY_BUFFER, a[i].buffer);
        if (a[i].integer) BE(glVertexAttribIPointer)((GLuint)i, a[i].size, a[i].type, a[i].stride, p);
        else              BE(glVertexAttribPointer)((GLuint)i, a[i].size, a[i].type, (GLboolean)a[i].normalized, a[i].stride, p);
        touched = 1;
    }
    if (touched) BE(glBindBuffer)(GL_ARRAY_BUFFER, s->array_buffer);
}

/* GL exige que baseinstance sea multiplo del divisor de cada atributo instanciado para poder
 * emularlo: 1 = se puede */
static int baseinstance_ok(GLuint binst)
{
    const gl31_attrib_t* a;
    int n = 0, i;
    if (!binst) return 1;
    a = gl31_vao_attribs(&n);
    if (!a) return 1;
    for (i = 0; i < n; i++)
        if (a[i].enabled && a[i].divisor && (binst % a[i].divisor)) return 0;
    return 1;
}


/* ====================== nucleo de dibujo ======================
 * Todas las variantes (arrays/elements, instanciado, base vertex/instance, rango, multi) terminan
 * aqui: un solo sitio decide si el draw se hace tal cual o con indices reescritos en GPU. */
typedef struct {
    int indexed, ranged;
    GLenum mode; GLint first; GLsizei count; GLenum type; const void* indices;
    GLsizei inst;                 /* < 0: llamada no instanciada */
    GLint basevertex; GLuint binst;
    GLuint start, end;
} draw_t;

/* Emite el draw con las funciones del backend; los indices ya son los finales. */
static void emit(const draw_t* d)
{
    if (!d->indexed) {
        if (d->inst < 0) { BE(glDrawArrays)(d->mode, d->first, d->count); return; }
        if (!d->binst)   { BE(glDrawArraysInstanced)(d->mode, d->first, d->count, d->inst); return; }
        if (gl31_caps.base_instance) { BE(glDrawArraysInstancedBaseInstance)(d->mode, d->first, d->count, d->inst, d->binst); return; }
        if (!baseinstance_ok(d->binst)) {
            gl31_stub_warn("baseinstance no multiplo del divisor de un atributo");
            gl31_set_error(GL_INVALID_OPERATION);
            return;
        }
        shift_attribs(0, d->binst, 1);
        BE(glDrawArraysInstanced)(d->mode, d->first, d->count, d->inst);
        shift_attribs(0, d->binst, 0);
        return;
    }
    if (d->inst < 0) {
        if (d->ranged) {
            if (!d->basevertex) { BE(glDrawRangeElements)(d->mode, d->start, d->end, d->count, d->type, d->indices); return; }
            if (gl31_caps.base_vertex) { BE(glDrawRangeElementsBaseVertex)(d->mode, d->start, d->end, d->count, d->type, d->indices, d->basevertex); return; }
            shift_attribs(d->basevertex, 0, 1);
            BE(glDrawRangeElements)(d->mode, d->start, d->end, d->count, d->type, d->indices);
            shift_attribs(d->basevertex, 0, 0);
            return;
        }
        if (!d->basevertex) { BE(glDrawElements)(d->mode, d->count, d->type, d->indices); return; }
        if (gl31_caps.base_vertex) { BE(glDrawElementsBaseVertex)(d->mode, d->count, d->type, d->indices, d->basevertex); return; }
        shift_attribs(d->basevertex, 0, 1);
        BE(glDrawElements)(d->mode, d->count, d->type, d->indices);
        shift_attribs(d->basevertex, 0, 0);
        return;
    }
    if (!d->binst) {
        if (!d->basevertex) { BE(glDrawElementsInstanced)(d->mode, d->count, d->type, d->indices, d->inst); return; }
        if (gl31_caps.base_vertex) { BE(glDrawElementsInstancedBaseVertex)(d->mode, d->count, d->type, d->indices, d->inst, d->basevertex); return; }
        shift_attribs(d->basevertex, 0, 1);
        BE(glDrawElementsInstanced)(d->mode, d->count, d->type, d->indices, d->inst);
        shift_attribs(d->basevertex, 0, 0);
        return;
    }
    if (gl31_caps.base_instance && gl31_be.glDrawElementsInstancedBaseVertexBaseInstance) {
        BE(glDrawElementsInstancedBaseVertexBaseInstance)(d->mode, d->count, d->type, d->indices, d->inst, d->basevertex, d->binst);
        return;
    }
    if (gl31_caps.base_instance && !d->basevertex) {
        BE(glDrawElementsInstancedBaseInstance)(d->mode, d->count, d->type, d->indices, d->inst, d->binst);
        return;
    }
    if (!baseinstance_ok(d->binst)) {
        gl31_stub_warn("baseinstance no multiplo del divisor de un atributo");
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    shift_attribs(d->basevertex, d->binst, 1);
    BE(glDrawElementsInstanced)(d->mode, d->count, d->type, d->indices, d->inst);
    shift_attribs(d->basevertex, d->binst, 0);
}

static void core_draw(const draw_t* d)
{
    GLenum om = d->mode;
    int unavailable = 0, op;
    if (d->count <= 0) { if (d->indexed) apply_restart(d->type); return; }
    op = gl31_idx_needed(d->mode, d->indexed, d->type, &om, &unavailable);
    if (op != IDX_NONE && !unavailable) {
        GLsizei oc = 0;
        GLuint buf = gl31_idx_generate(op, d->mode, d->indexed, d->type, d->first, d->count, (uintptr_t)d->indices, &oc);
        if (buf) {
            if (oc > 0) {
                draw_t r = *d;
                GLint saved = 0;
                BE(glGetIntegerv)(GL_ELEMENT_ARRAY_BUFFER_BINDING, &saved);
                BE(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, buf);
                set_restart_fixed(GL_TRUE);              /* las ranuras vacias son 0xFFFFFFFF */
                r.indexed = 1; r.ranged = 0; r.mode = om; r.count = oc; r.type = GL_UNSIGNED_INT; r.indices = NULL;
                if (!d->indexed) r.basevertex = 0;
                emit(&r);
                BE(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, (GLuint)saved);
            }
            gl31_xfb_account(d->mode, d->count, d->inst < 0 ? 1 : d->inst);
            return;
        }
        unavailable = 1;
    }
    if (op != IDX_NONE && unavailable && op != IDX_RESTART)
        gl31_stub_warn(op == IDX_PROVOKE ? "glProvokingVertex(FIRST_VERTEX_CONVENTION)" : "glPolygonMode(LINE/POINT)");
    if (d->indexed) apply_restart(d->type);
    emit(d);
    gl31_xfb_account(d->mode, d->count, d->inst < 0 ? 1 : d->inst);
}

static draw_t mk_arrays(GLenum mode, GLint first, GLsizei count, GLsizei inst, GLuint binst)
{
    draw_t d; memset(&d, 0, sizeof d);
    d.mode = mode; d.first = first; d.count = count; d.inst = inst; d.binst = binst;
    return d;
}

static draw_t mk_elems(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei inst, GLint bv, GLuint binst)
{
    draw_t d; memset(&d, 0, sizeof d);
    d.indexed = 1; d.mode = mode; d.count = count; d.type = type; d.indices = indices;
    d.inst = inst; d.basevertex = bv; d.binst = binst;
    return d;
}

void gl31_glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    draw_t d;
    if (first < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw(mode, count)) return;
    d = mk_arrays(mode, first, count, -1, 0);
    core_draw(&d);
}

void gl31_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    draw_t d;
    if (!pre_draw_elements(mode, count, type)) return;
    d = mk_elems(mode, count, type, indices, -1, 0, 0);
    core_draw(&d);
}

void gl31_glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei inst)
{
    draw_t d;
    if (first < 0 || inst < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw(mode, count)) return;
    d = mk_arrays(mode, first, count, inst, 0);
    core_draw(&d);
}

void gl31_glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei inst)
{
    draw_t d;
    if (inst < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw_elements(mode, count, type)) return;
    d = mk_elems(mode, count, type, indices, inst, 0, 0);
    core_draw(&d);
}

void gl31_glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void* indices)
{
    draw_t d;
    if (end < start) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw_elements(mode, count, type)) return;
    d = mk_elems(mode, count, type, indices, -1, 0, 0);
    d.ranged = 1; d.start = start; d.end = end;
    core_draw(&d);
}

/* GLES 3.0 no tiene MultiDraw*: se emula con un bucle. */
void gl31_glMultiDrawArrays(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount)
{
    GLsizei i;
    if (drawcount < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw(mode, 0)) return;
    if (drawcount && (!first || !count)) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < drawcount; i++) {
        draw_t d;
        if (first[i] < 0 || count[i] < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
        if (count[i] > 0) { d = mk_arrays(mode, first[i], count[i], -1, 0); core_draw(&d); }
    }
}

void gl31_glMultiDrawElements(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices, GLsizei drawcount)
{
    GLsizei i;
    if (drawcount < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw_elements(mode, 0, type)) return;
    if (drawcount && (!count || !indices)) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < drawcount; i++) {
        draw_t d;
        if (count[i] < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
        if (count[i] > 0) { d = mk_elems(mode, count[i], type, indices[i], -1, 0, 0); core_draw(&d); }
    }
}

void gl31_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void* indices, GLint basevertex)
{
    draw_t d;
    if (!pre_draw_elements(mode, count, type)) return;
    d = mk_elems(mode, count, type, indices, -1, basevertex, 0);
    core_draw(&d);
}

void gl31_glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type,
                                        const void* indices, GLint basevertex)
{
    draw_t d;
    if (end < start) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw_elements(mode, count, type)) return;
    d = mk_elems(mode, count, type, indices, -1, basevertex, 0);
    d.ranged = 1; d.start = start; d.end = end;
    core_draw(&d);
}

void gl31_glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void* indices,
                                            GLsizei inst, GLint basevertex)
{
    draw_t d;
    if (inst < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw_elements(mode, count, type)) return;
    d = mk_elems(mode, count, type, indices, inst, basevertex, 0);
    core_draw(&d);
}

void gl31_glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei* count, GLenum type,
                                        const void* const* indices, GLsizei drawcount, const GLint* basevertex)
{
    GLsizei i;
    if (drawcount < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw_elements(mode, 0, type)) return;
    if (drawcount && (!count || !indices || !basevertex)) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < drawcount; i++) {
        draw_t d;
        if (count[i] < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
        if (count[i] == 0) continue;
        d = mk_elems(mode, count[i], type, indices[i], -1, basevertex[i], 0);
        core_draw(&d);
    }
}

/* ---- GL 4.2: base instance (nativo con EXT_base_instance, si no emulado) ---- */
void gl31_glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei inst, GLuint binst)
{
    draw_t d;
    if (first < 0 || inst < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw(mode, count)) return;
    d = mk_arrays(mode, first, count, inst, binst);
    core_draw(&d);
}

void gl31_glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const void* indices,
                                              GLsizei inst, GLuint binst)
{
    gl31_glDrawElementsInstancedBaseVertexBaseInstance(mode, count, type, indices, inst, 0, binst);
}

void gl31_glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const void* indices,
                                                        GLsizei inst, GLint basevertex, GLuint binst)
{
    draw_t d;
    if (inst < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw_elements(mode, count, type)) return;
    d = mk_elems(mode, count, type, indices, inst, basevertex, binst);
    core_draw(&d);
}

/* ES siempre usa el ultimo vertice (el valor por defecto de GL 3.2). FIRST_VERTEX_CONVENTION se usa
 * nativo con EXT/ANGLE_provoking_vertex; sin ella, el motor de indices (gl31_idxgen.c) rota cada
 * primitiva en la GPU para que el vertice provocador quede el ultimo. */
void gl31_glProvokingVertex(GLenum mode)
{
    gl31_state_t* s = gl31_state();
    if (mode != GL_FIRST_VERTEX_CONVENTION && mode != GL_LAST_VERTEX_CONVENTION) {
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    if (gl31_caps.provoking_vertex) BE(glProvokingVertex)(mode);
    s->provoking_vertex = mode;
}

/* ====================== GL 4.0 / 4.3: draws indirectos ======================
 * Nativos en ES 3.1. Los campos baseVertex / baseInstance de los comandos en el buffer los
 * interpreta el backend (ES exige reservedMustBeZero = 0), no se emulan. */
static int indirect_ok(const char* fn)
{
    if (gl31_caps.es31) return 1;
    gl31_stub_warn(fn);
    gl31_set_error(GL_INVALID_OPERATION);
    return 0;
}

void gl31_glDrawArraysIndirect(GLenum mode, const void* indirect)
{
    if (!indirect_ok("glDrawArraysIndirect (backend sin ES 3.1)")) return;
    if (!pre_draw(mode, 0)) return;
    BE(glDrawArraysIndirect)(mode, indirect);
}

void gl31_glDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect)
{
    if (!indirect_ok("glDrawElementsIndirect (backend sin ES 3.1)")) return;
    if (!pre_draw_elements(mode, 0, type)) return;
    apply_restart(type);
    BE(glDrawElementsIndirect)(mode, type, indirect);
}

/* GL 4.3 multi-draw indirect: un bucle sobre los comandos consecutivos del buffer */
void gl31_glMultiDrawArraysIndirect(GLenum mode, const void* indirect, GLsizei drawcount, GLsizei stride)
{
    GLsizei i;
    if (!indirect_ok("glMultiDrawArraysIndirect (backend sin ES 3.1)")) return;
    if (drawcount < 0 || stride < 0 || (stride & 3)) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw(mode, 0)) return;
    if (!stride) stride = 16;                       /* DrawArraysIndirectCommand */
    for (i = 0; i < drawcount; i++)
        BE(glDrawArraysIndirect)(mode, (const void*)((uintptr_t)indirect + (uintptr_t)i * (uintptr_t)stride));
}

void gl31_glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect, GLsizei drawcount, GLsizei stride)
{
    GLsizei i;
    if (!indirect_ok("glMultiDrawElementsIndirect (backend sin ES 3.1)")) return;
    if (drawcount < 0 || stride < 0 || (stride & 3)) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!pre_draw_elements(mode, 0, type)) return;
    apply_restart(type);
    if (!stride) stride = 20;                       /* DrawElementsIndirectCommand */
    for (i = 0; i < drawcount; i++)
        BE(glDrawElementsIndirect)(mode, type, (const void*)((uintptr_t)indirect + (uintptr_t)i * (uintptr_t)stride));
}
