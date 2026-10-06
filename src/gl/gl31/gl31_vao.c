/* gl31_vao.c - vertex array objects (perfil core: VAO obligatorio, sin punteros cliente) */
#include "gl31.h"

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
    for (i = 0; i < n; i++)
        if (arrays[i] && arrays[i] == s->vao) s->vao = 0;
    BE(glDeleteVertexArrays)(n, arrays);
}

void gl31_glBindVertexArray(GLuint array)
{
    gl31_state()->vao = array;
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

void gl31_glVertexAttribPointer(GLuint idx, GLint size, GLenum type, GLboolean norm, GLsizei stride, const void* ptr)
{
    gl31_state_t* s = gl31_state();
    if (!check_vao() || !check_index(idx)) return;
    /* core: sin ARRAY_BUFFER solo se admite offset 0 (sin punteros cliente) */
    if (!s->array_buffer && ptr) { gl31_set_error(GL_INVALID_OPERATION); return; }
    BE(glVertexAttribPointer)(idx, size, type, norm, stride, ptr);
}

void gl31_glVertexAttribIPointer(GLuint idx, GLint size, GLenum type, GLsizei stride, const void* ptr)
{
    gl31_state_t* s = gl31_state();
    if (!check_vao() || !check_index(idx)) return;
    if (!s->array_buffer && ptr) { gl31_set_error(GL_INVALID_OPERATION); return; }
    BE(glVertexAttribIPointer)(idx, size, type, stride, ptr);
}

void gl31_glEnableVertexAttribArray(GLuint idx)
{
    if (!check_vao() || !check_index(idx)) return;
    BE(glEnableVertexAttribArray)(idx);
}

void gl31_glDisableVertexAttribArray(GLuint idx)
{
    if (!check_vao() || !check_index(idx)) return;
    BE(glDisableVertexAttribArray)(idx);
}

void gl31_glVertexAttribDivisor(GLuint idx, GLuint divisor)
{
    if (!check_vao() || !check_index(idx)) return;
    BE(glVertexAttribDivisor)(idx, divisor);
}
