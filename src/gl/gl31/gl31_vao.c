/* gl31_vao.c - vertex array objects (perfil core: VAO obligatorio, sin punteros cliente).
 * Ademas de reenviar, se recuerda por VAO la configuracion de cada atributo: la usa
 * gl31_draw.c para emular glDrawElementsBaseVertex cuando el backend no lo tiene. */
#include "gl31.h"
#include <stdlib.h>
#include <string.h>

typedef struct { GLuint id; gl31_attrib_t a[GL31_MAX_ATTRIBS]; } vao_t;
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
    a->size = size; a->type = type; a->normalized = norm; a->stride = stride;
    a->ptr = ptr; a->integer = integer; a->buffer = gl31_state()->array_buffer;
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
    { vao_t* v = cur_vao(); if (v && idx < GL31_MAX_ATTRIBS) v->a[idx].divisor = divisor; }
}

void gl31_vao_shutdown(void)
{
    free(g_vao);
    g_vao = NULL; g_nv = g_cv = 0;
}
