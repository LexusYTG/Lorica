/* gl31_uniform.c - uniform blocks (UBO) y consulta de bindings indexados */
#include "gl31.h"

GLuint gl31_glGetUniformBlockIndex(GLuint program, const GLchar* name)
{
    if (!name) { gl31_set_error(GL_INVALID_VALUE); return GL_INVALID_INDEX; }
    return BE(glGetUniformBlockIndex)(program, name);
}

void gl31_glUniformBlockBinding(GLuint program, GLuint block, GLuint binding)
{
    gl31_state_load_limits();
    if ((GLint)binding >= gl31_state()->max_ubo_bindings) {
        gl31_set_error(GL_INVALID_VALUE);
        return;
    }
    BE(glUniformBlockBinding)(program, block, binding);
}

void gl31_glGetActiveUniformBlockiv(GLuint program, GLuint block, GLenum pname, GLint* params)
{
    if (!params) return;
    BE(glGetActiveUniformBlockiv)(program, block, pname, params);
}

void gl31_glGetActiveUniformBlockName(GLuint program, GLuint block, GLsizei bufSize, GLsizei* length, GLchar* name)
{
    if (bufSize < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGetActiveUniformBlockName)(program, block, bufSize, length, name);
}

void gl31_glGetUniformIndices(GLuint program, GLsizei count, const GLchar* const* names, GLuint* indices)
{
    if (count < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (count == 0) return;
    if (!names || !indices) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGetUniformIndices)(program, count, names, indices);
}

void gl31_glGetActiveUniformsiv(GLuint program, GLsizei count, const GLuint* indices, GLenum pname, GLint* params)
{
    if (count < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (count == 0) return;
    if (!indices || !params) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGetActiveUniformsiv)(program, count, indices, pname, params);
}

/* Los bindings de UBO se sirven desde el estado propio (coherente con
 * BindBufferBase/Range de gl31_buffer.c); el resto va al backend. */
void gl31_glGetIntegeri_v(GLenum target, GLuint index, GLint* data)
{
    gl31_state_t* s = gl31_state();
    if (!data) return;
    switch (target) {
        case GL_UNIFORM_BUFFER_BINDING:
        case GL_UNIFORM_BUFFER_START:
        case GL_UNIFORM_BUFFER_SIZE:
            gl31_state_load_limits();
            if ((GLint)index >= s->max_ubo_bindings) { gl31_set_error(GL_INVALID_VALUE); return; }
            if (target == GL_UNIFORM_BUFFER_BINDING)    *data = (GLint)s->ubo[index].buffer;
            else if (target == GL_UNIFORM_BUFFER_START) *data = (GLint)s->ubo[index].offset;
            else                                        *data = (GLint)s->ubo[index].size;
            return;
        default:
            BE(glGetIntegeri_v)(target, index, data);
    }
}

/* ======================================================================
 * glUniform* / glGetUniform* / glGetActive* / glGetFragDataLocation
 *
 * Los nombres de programa son los del backend, asi que casi todo es un
 * reenvio con las comprobaciones de GL 3.1 que ES no hace igual:
 *   - sin programa en uso -> INVALID_OPERATION
 *   - count < 0           -> INVALID_VALUE
 *   - location == -1      -> se ignora en silencio
 *   - nombre que es un shader -> INVALID_OPERATION; nombre inexistente
 *     -> INVALID_VALUE (seccion 2.11.1)
 * Las condiciones que solo el backend conoce (location invalida, tipo que
 * no coincide, count > 1 en no-array) las detecta GLES y salen por
 * glGetError con el codigo de GLES.
 * ====================================================================== */

#define U_NEED_PROGRAM() \
    do { if (!gl31_state()->program) { gl31_set_error(GL_INVALID_OPERATION); return; } } while (0)

#define U_CHECK_SCALAR(loc) \
    do { U_NEED_PROGRAM(); if ((loc) == -1) return; } while (0)

#define U_CHECK_VECTOR(loc, count, ptr) \
    do { \
        U_NEED_PROGRAM(); \
        if ((count) < 0) { gl31_set_error(GL_INVALID_VALUE); return; } \
        if ((loc) == -1) return; \
        if ((count) > 0 && !(ptr)) { gl31_set_error(GL_INVALID_VALUE); return; } \
    } while (0)

#define U_S1(sfx, T) \
    void gl31_glUniform1##sfx(GLint l, T v0) \
    { U_CHECK_SCALAR(l); BE(glUniform1##sfx)(l, v0); }
#define U_S2(sfx, T) \
    void gl31_glUniform2##sfx(GLint l, T v0, T v1) \
    { U_CHECK_SCALAR(l); BE(glUniform2##sfx)(l, v0, v1); }
#define U_S3(sfx, T) \
    void gl31_glUniform3##sfx(GLint l, T v0, T v1, T v2) \
    { U_CHECK_SCALAR(l); BE(glUniform3##sfx)(l, v0, v1, v2); }
#define U_S4(sfx, T) \
    void gl31_glUniform4##sfx(GLint l, T v0, T v1, T v2, T v3) \
    { U_CHECK_SCALAR(l); BE(glUniform4##sfx)(l, v0, v1, v2, v3); }

U_S1(f, GLfloat) U_S1(i, GLint) U_S1(ui, GLuint)
U_S2(f, GLfloat) U_S2(i, GLint) U_S2(ui, GLuint)
U_S3(f, GLfloat) U_S3(i, GLint) U_S3(ui, GLuint)
U_S4(f, GLfloat) U_S4(i, GLint) U_S4(ui, GLuint)

#define U_V(n, sfx, T) \
    void gl31_glUniform##n##sfx##v(GLint l, GLsizei count, const T* v) \
    { U_CHECK_VECTOR(l, count, v); BE(glUniform##n##sfx##v)(l, count, v); }

U_V(1, f, GLfloat) U_V(1, i, GLint) U_V(1, ui, GLuint)
U_V(2, f, GLfloat) U_V(2, i, GLint) U_V(2, ui, GLuint)
U_V(3, f, GLfloat) U_V(3, i, GLint) U_V(3, ui, GLuint)
U_V(4, f, GLfloat) U_V(4, i, GLint) U_V(4, ui, GLuint)

#define U_M(name) \
    void gl31_glUniform##name(GLint l, GLsizei count, GLboolean transpose, const GLfloat* v) \
    { U_CHECK_VECTOR(l, count, v); BE(glUniform##name)(l, count, transpose, v); }

U_M(Matrix2fv)   U_M(Matrix3fv)   U_M(Matrix4fv)
U_M(Matrix2x3fv) U_M(Matrix3x2fv) U_M(Matrix2x4fv)
U_M(Matrix4x2fv) U_M(Matrix3x4fv) U_M(Matrix4x3fv)

/* ---------- consultas ---------- */

/* 1 = programa linkeado. Si no, genera el error y devuelve 0. */
static int prog_linked_or_error(GLuint program)
{
    switch (gl31_program_status(program)) {
        case 1:  return 1;
        case 0:
        case -2: gl31_set_error(GL_INVALID_OPERATION); return 0;
        default: gl31_set_error(GL_INVALID_VALUE);     return 0;
    }
}

/* Valida (program, index, bufSize) para GetActiveUniform/Attrib/UniformName.
 * Un programa sin linkear tiene 0 activos, por eso cualquier index es
 * INVALID_VALUE. */
static int prog_for_active(GLuint program, GLenum count_pname, GLuint index, GLsizei bufSize)
{
    GLint n = 0;
    int st = gl31_program_status(program);
    if (st == -2) { gl31_set_error(GL_INVALID_OPERATION); return 0; }
    if (st < 0 || bufSize < 0) { gl31_set_error(GL_INVALID_VALUE); return 0; }
    if (st == 1) BE(glGetProgramiv)(program, count_pname, &n);
    if (index >= (GLuint)n) { gl31_set_error(GL_INVALID_VALUE); return 0; }
    return 1;
}

#define GETU(sfx, T) \
    void gl31_glGetUniform##sfx(GLuint program, GLint location, T* params) \
    { \
        if (!prog_linked_or_error(program)) return; \
        if (location < 0) { gl31_set_error(GL_INVALID_OPERATION); return; } \
        if (!params) return; \
        BE(glGetUniform##sfx)(program, location, params); \
    }

GETU(fv, GLfloat)
GETU(iv, GLint)
GETU(uiv, GLuint)

void gl31_glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length,
                             GLint* size, GLenum* type, GLchar* name)
{
    GLsizei len = 0; GLint sz = 0; GLenum ty = 0;
    if (!prog_for_active(program, GL_ACTIVE_UNIFORMS, index, bufSize)) return;
    if (!name) bufSize = 0;
    BE(glGetActiveUniform)(program, index, bufSize, &len, &sz, &ty, name);
    if (length) *length = len;
    if (size)   *size = sz;
    if (type)   *type = ty;
}

/* GL 3.1 (ARB_uniform_buffer_object). ES 3.0 no lo tiene, pero el indice es
 * el mismo que usa glGetActiveUniform / glGetUniformIndices. */
void gl31_glGetActiveUniformName(GLuint program, GLuint uniformIndex, GLsizei bufSize,
                                 GLsizei* length, GLchar* name)
{
    GLsizei len = 0; GLint sz = 0; GLenum ty = 0;
    if (!prog_for_active(program, GL_ACTIVE_UNIFORMS, uniformIndex, bufSize)) return;
    if (!name) bufSize = 0;
    BE(glGetActiveUniform)(program, uniformIndex, bufSize, &len, &sz, &ty, name);
    if (length) *length = len;
}

void gl31_glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length,
                            GLint* size, GLenum* type, GLchar* name)
{
    GLsizei len = 0; GLint sz = 0; GLenum ty = 0;
    if (!prog_for_active(program, GL_ACTIVE_ATTRIBUTES, index, bufSize)) return;
    if (!name) bufSize = 0;
    BE(glGetActiveAttrib)(program, index, bufSize, &len, &sz, &ty, name);
    if (length) *length = len;
    if (size)   *size = sz;
    if (type)   *type = ty;
}

GLint gl31_glGetFragDataLocation(GLuint program, const GLchar* name)
{
    if (!prog_linked_or_error(program)) return -1;
    if (!name) return -1;
    return BE(glGetFragDataLocation)(program, name);
}
