/* gl31_vao.c - vertex array objects (perfil core: VAO obligatorio, sin punteros cliente).
 * Ademas de reenviar, se recuerda por VAO la configuracion de cada atributo: la usa
 * gl31_draw.c para emular glDrawElementsBaseVertex cuando el backend no lo tiene. */
#include "gl31.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { GLuint id; gl31_attrib_t a[GL31_MAX_ATTRIBS]; gl31_vbind_t b[GL31_MAX_ATTRIBS]; } vao_t;
static vao_t* g_vao; static size_t g_nv, g_cv;      /* como el resto de tablas: sin locking */

static vao_t* vao_find(GLuint id)
{
    size_t i;
    for (i = 0; i < g_nv; i++) if (g_vao[i].id == id) return &g_vao[i];
    return NULL;
}

static vao_t* vao_get(GLuint id)
{
    vao_t* v = vao_find(id);
    if (v) return v;
    if (g_nv == g_cv) {
        size_t c = g_cv ? g_cv * 2 : 16;
        vao_t* p = (vao_t*)realloc(g_vao, c * sizeof *p);
        if (!p) return NULL;
        g_vao = p; g_cv = c;
    }
    v = &g_vao[g_nv++];
    memset(v, 0, sizeof *v);
    v->id = id;
    return v;
}

static vao_t* cur_vao(void)
{
    GLuint id = gl31_state()->vao;
    return id ? vao_get(id) : NULL;
}

const gl31_vbind_t* gl31_vao_bindings(int* count)
{
    vao_t* v = cur_vao();
    if (!v) return NULL;
    if (count) *count = GL31_MAX_ATTRIBS;
    return v->b;
}

/* recalcula la vista "efectiva" (buffer / ptr / stride / divisor) de los atributos de un binding */
static void resync(vao_t* v, GLuint binding)
{
    int i;
    for (i = 0; i < GL31_MAX_ATTRIBS; i++) {
        gl31_attrib_t* a = &v->a[i];
        if (a->binding != binding) continue;
        a->buffer  = v->b[binding].buffer;
        a->ptr     = (const void*)((uintptr_t)v->b[binding].offset + (uintptr_t)a->reloff);
        a->stride  = v->b[binding].stride;
        a->divisor = v->b[binding].divisor;
    }
}

const gl31_attrib_t* gl31_vao_attribs(int* count)
{
    vao_t* v = cur_vao();
    if (!v) return NULL;
    if (count) *count = GL31_MAX_ATTRIBS;
    return v->a;
}

void gl31_glGenVertexArrays(GLsizei n, GLuint* arrays)
{
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGenVertexArrays)(n, arrays);
}

void gl31_glDeleteVertexArrays(GLsizei n, const GLuint* arrays)
{
    GLsizei i;
    gl31_state_t* s = gl31_state();
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < n; i++) {
        size_t k;
        if (!arrays[i]) continue;
        if (arrays[i] == s->vao) s->vao = 0;
        for (k = 0; k < g_nv; k++)
            if (g_vao[k].id == arrays[i]) { g_vao[k] = g_vao[--g_nv]; break; }
    }
    BE(glDeleteVertexArrays)(n, arrays);
}

void gl31_glBindVertexArray(GLuint array)
{
    gl31_state()->vao = array;
    if (array) (void)vao_get(array);
    BE(glBindVertexArray)(array);
}

GLboolean gl31_glIsVertexArray(GLuint array)
{
    return array ? BE(glIsVertexArray)(array) : GL_FALSE;
}

static int check_vao(void)
{
    if (!gl31_state()->vao) { gl31_set_error(GL_INVALID_OPERATION); return 0; }
    return 1;
}

static int check_index(GLuint idx)
{
    GLint max = 16;
    BE(glGetIntegerv)(GL_MAX_VERTEX_ATTRIBS, &max);
    if ((GLint)idx >= max) { gl31_set_error(GL_INVALID_VALUE); return 0; }
    return 1;
}

static void record(GLuint idx, GLint size, GLenum type, GLboolean norm, GLsizei stride,
                   const void* ptr, int integer)
{
    vao_t* v = cur_vao();
    gl31_attrib_t* a;
    if (!v || idx >= GL31_MAX_ATTRIBS) return;
    a = &v->a[idx];
    a->size = size; a->type = type; a->normalized = norm; a->integer = integer;
    a->binding = idx; a->reloff = 0;
    v->b[idx].buffer = gl31_state()->array_buffer;
    v->b[idx].offset = (GLintptr)(uintptr_t)ptr;
    /* stride 0 = datos contiguos: ES guarda el stride efectivo en el binding */
    if (!stride) {
        size_t ts = (type == GL_BYTE || type == GL_UNSIGNED_BYTE) ? 1u :
                    (type == GL_SHORT || type == GL_UNSIGNED_SHORT || type == GL_HALF_FLOAT) ? 2u : 4u;
        if (type == GL_INT_2_10_10_10_REV || type == GL_UNSIGNED_INT_2_10_10_10_REV) stride = 4;
        else stride = (GLsizei)((size_t)size * ts);
    }
    v->b[idx].stride = stride;
    resync(v, idx);
}

static void set_flag(GLuint idx, int on)
{
    vao_t* v = cur_vao();
    if (v && idx < GL31_MAX_ATTRIBS) v->a[idx].enabled = on;
}

void gl31_glVertexAttribPointer(GLuint idx, GLint size, GLenum type, GLboolean norm, GLsizei stride, const void* ptr)
{
    gl31_state_t* s = gl31_state();
    if (!check_vao() || !check_index(idx)) return;
    /* core: sin ARRAY_BUFFER solo se admite offset 0 (sin punteros cliente) */
    if (!s->array_buffer && ptr) { gl31_set_error(GL_INVALID_OPERATION); return; }
    BE(glVertexAttribPointer)(idx, size, type, norm, stride, ptr);
    record(idx, size, type, norm, stride, ptr, 0);
}

void gl31_glVertexAttribIPointer(GLuint idx, GLint size, GLenum type, GLsizei stride, const void* ptr)
{
    gl31_state_t* s = gl31_state();
    if (!check_vao() || !check_index(idx)) return;
    if (!s->array_buffer && ptr) { gl31_set_error(GL_INVALID_OPERATION); return; }
    BE(glVertexAttribIPointer)(idx, size, type, stride, ptr);
    record(idx, size, type, GL_FALSE, stride, ptr, 1);
}

void gl31_glEnableVertexAttribArray(GLuint idx)
{
    if (!check_vao() || !check_index(idx)) return;
    BE(glEnableVertexAttribArray)(idx);
    set_flag(idx, 1);
}

void gl31_glDisableVertexAttribArray(GLuint idx)
{
    if (!check_vao() || !check_index(idx)) return;
    BE(glDisableVertexAttribArray)(idx);
    set_flag(idx, 0);
}

void gl31_glVertexAttribDivisor(GLuint idx, GLuint divisor)
{
    if (!check_vao() || !check_index(idx)) return;
    BE(glVertexAttribDivisor)(idx, divisor);
    { vao_t* v = cur_vao(); if (v && idx < GL31_MAX_ATTRIBS) { v->b[v->a[idx].binding].divisor = divisor; resync(v, v->a[idx].binding); } }
}

/* ---- GL 4.3: ARB_vertex_attrib_binding (nativo en ES 3.1) ---- */
static int need_es31(const char* fn)
{
    if (gl31_caps.es31) return 1;
    gl31_stub_warn(fn);
    gl31_set_error(GL_INVALID_OPERATION);
    return 0;
}

static int check_binding(GLuint b)
{
    GLint max = 16;
    BE(glGetIntegerv)(GL_MAX_VERTEX_ATTRIB_BINDINGS, &max);
    if (max <= 0) max = 16;
    if (max > GL31_MAX_ATTRIBS) max = GL31_MAX_ATTRIBS;
    if ((GLint)b >= max) { gl31_set_error(GL_INVALID_VALUE); return 0; }
    return 1;
}

void gl31_glVertexAttribFormat(GLuint idx, GLint size, GLenum type, GLboolean norm, GLuint reloff)
{
    vao_t* v;
    if (!need_es31("glVertexAttribFormat (backend sin ES 3.1)")) return;
    if (!check_vao() || !check_index(idx)) return;
    BE(glVertexAttribFormat)(idx, size, type, norm, reloff);
    v = cur_vao();
    if (!v) return;
    v->a[idx].size = size; v->a[idx].type = type; v->a[idx].normalized = norm;
    v->a[idx].integer = 0; v->a[idx].reloff = reloff;
    resync(v, v->a[idx].binding);
}

void gl31_glVertexAttribIFormat(GLuint idx, GLint size, GLenum type, GLuint reloff)
{
    vao_t* v;
    if (!need_es31("glVertexAttribIFormat (backend sin ES 3.1)")) return;
    if (!check_vao() || !check_index(idx)) return;
    BE(glVertexAttribIFormat)(idx, size, type, reloff);
    v = cur_vao();
    if (!v) return;
    v->a[idx].size = size; v->a[idx].type = type; v->a[idx].normalized = 0;
    v->a[idx].integer = 1; v->a[idx].reloff = reloff;
    resync(v, v->a[idx].binding);
}

/* los atributos de doble precision no existen en ES */
void gl31_glVertexAttribLFormat(GLuint idx, GLint size, GLenum type, GLuint reloff)
{
    (void)idx; (void)size; (void)type; (void)reloff;
    gl31_stub_warn("glVertexAttribLFormat (fp64)");
    gl31_set_error(GL_INVALID_OPERATION);
}

void gl31_glVertexAttribBinding(GLuint idx, GLuint binding)
{
    vao_t* v;
    if (!need_es31("glVertexAttribBinding (backend sin ES 3.1)")) return;
    if (!check_vao() || !check_index(idx) || !check_binding(binding)) return;
    BE(glVertexAttribBinding)(idx, binding);
    v = cur_vao();
    if (!v) return;
    v->a[idx].binding = binding;
    resync(v, binding);
}

void gl31_glBindVertexBuffer(GLuint binding, GLuint buffer, GLintptr offset, GLsizei stride)
{
    vao_t* v;
    if (!need_es31("glBindVertexBuffer (backend sin ES 3.1)")) return;
    if (!check_vao() || !check_binding(binding)) return;
    if (offset < 0 || stride < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glBindVertexBuffer)(binding, buffer, offset, stride);
    v = cur_vao();
    if (!v) return;
    v->b[binding].buffer = buffer; v->b[binding].offset = offset; v->b[binding].stride = stride;
    resync(v, binding);
}

void gl31_glVertexBindingDivisor(GLuint binding, GLuint divisor)
{
    vao_t* v;
    if (!need_es31("glVertexBindingDivisor (backend sin ES 3.1)")) return;
    if (!check_vao() || !check_binding(binding)) return;
    BE(glVertexBindingDivisor)(binding, divisor);
    v = cur_vao();
    if (!v) return;
    v->b[binding].divisor = divisor;
    resync(v, binding);
}

/* GL 4.4: varios bindings de una vez (emulado con un bucle) */
void gl31_glBindVertexBuffers(GLuint first, GLsizei count, const GLuint* buffers, const GLintptr* offsets, const GLsizei* strides)
{
    GLsizei i;
    if (count < 0) { gl31_set_error(GL_INVALID_OPERATION); return; }
    if (!buffers) {
        for (i = 0; i < count; i++) gl31_glBindVertexBuffer(first + (GLuint)i, 0, 0, 16);
        return;
    }
    if (!offsets || !strides) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < count; i++) gl31_glBindVertexBuffer(first + (GLuint)i, buffers[i], offsets[i], strides[i]);
}

void gl31_vao_shutdown(void)
{
    free(g_vao);
    g_vao = NULL; g_nv = g_cv = 0;
}

/* ---------- GL 3.3: ARB_vertex_type_2_10_10_10_rev (glVertexAttribP*) ----------
 * Se desempaqueta en CPU y se envia como valor generico de atributo (glVertexAttrib4f). */
static GLfloat unpack_c(GLuint v, int shift, int bits, int is_signed, GLboolean norm)
{
    GLuint mask = (1u << bits) - 1u;
    GLuint raw = (v >> shift) & mask;
    if (is_signed) {
        GLint sv = (GLint)raw;
        GLfloat f;
        if (sv & (1 << (bits - 1))) sv -= (1 << bits);
        f = (GLfloat)sv;
        if (norm) { f /= (GLfloat)((1 << (bits - 1)) - 1); if (f < -1.f) f = -1.f; }
        return f;
    }
    return norm ? (GLfloat)raw / (GLfloat)mask : (GLfloat)raw;
}

static void attribp(GLuint index, int n, GLenum type, GLboolean norm, GLuint v)
{
    int sgn;
    GLfloat c[4] = {0.f, 0.f, 0.f, 1.f};
    if (type == GL_UNSIGNED_INT_2_10_10_10_REV) sgn = 0;
    else if (type == GL_INT_2_10_10_10_REV)     sgn = 1;
    else { gl31_set_error(GL_INVALID_ENUM); return; }
    c[0] = unpack_c(v, 0, 10, sgn, norm);
    if (n >= 2) c[1] = unpack_c(v, 10, 10, sgn, norm);
    if (n >= 3) c[2] = unpack_c(v, 20, 10, sgn, norm);
    if (n >= 4) c[3] = unpack_c(v, 30, 2, sgn, norm);
    gl31_glVertexAttrib4f(index, c[0], c[1], c[2], c[3]);
}

void gl31_glVertexAttribP1ui(GLuint i, GLenum t, GLboolean n, GLuint v) { attribp(i, 1, t, n, v); }
void gl31_glVertexAttribP2ui(GLuint i, GLenum t, GLboolean n, GLuint v) { attribp(i, 2, t, n, v); }
void gl31_glVertexAttribP3ui(GLuint i, GLenum t, GLboolean n, GLuint v) { attribp(i, 3, t, n, v); }
void gl31_glVertexAttribP4ui(GLuint i, GLenum t, GLboolean n, GLuint v) { attribp(i, 4, t, n, v); }
void gl31_glVertexAttribP1uiv(GLuint i, GLenum t, GLboolean n, const GLuint* v) { if (v) attribp(i, 1, t, n, *v); else gl31_set_error(GL_INVALID_VALUE); }
void gl31_glVertexAttribP2uiv(GLuint i, GLenum t, GLboolean n, const GLuint* v) { if (v) attribp(i, 2, t, n, *v); else gl31_set_error(GL_INVALID_VALUE); }
void gl31_glVertexAttribP3uiv(GLuint i, GLenum t, GLboolean n, const GLuint* v) { if (v) attribp(i, 3, t, n, *v); else gl31_set_error(GL_INVALID_VALUE); }
void gl31_glVertexAttribP4uiv(GLuint i, GLenum t, GLboolean n, const GLuint* v) { if (v) attribp(i, 4, t, n, *v); else gl31_set_error(GL_INVALID_VALUE); }
