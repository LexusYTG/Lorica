/* gl31_buffer.c - buffers, buffer storage (emulado), UBOs, SSBOs, mapeo */
#include "gl31.h"
#include <string.h>

void gl31_glGenBuffers(GLsizei n, GLuint* buffers)
{
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGenBuffers)(n, buffers);
}

void gl31_glDeleteBuffers(GLsizei n, const GLuint* buffers)
{
    GLsizei i; int b;
    gl31_state_t* s = gl31_state();
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < n; i++) {
        GLuint id = buffers[i];
        if (!id) continue;
        if (s->array_buffer == id) s->array_buffer = 0;
        if (s->uniform_buffer == id) s->uniform_buffer = 0;
        if (s->copy_read_buffer == id) s->copy_read_buffer = 0;
        if (s->copy_write_buffer == id) s->copy_write_buffer = 0;
        if (s->pixel_pack_buffer == id) s->pixel_pack_buffer = 0;
        if (s->pixel_unpack_buffer == id) s->pixel_unpack_buffer = 0;
        for (b = 0; b < GL31_MAX_UBO_BINDINGS; b++)
            if (s->ubo[b].buffer == id) memset(&s->ubo[b], 0, sizeof(s->ubo[b]));
    }
    BE(glDeleteBuffers)(n, buffers);
}

void gl31_glBindBuffer(GLenum target, GLuint buffer)
{
    gl31_state_t* s = gl31_state();
    switch (target) {
        case GL_ARRAY_BUFFER:         s->array_buffer = buffer; break;
        case GL_UNIFORM_BUFFER:       s->uniform_buffer = buffer; break;
        case GL_COPY_READ_BUFFER:     s->copy_read_buffer = buffer; break;
        case GL_COPY_WRITE_BUFFER:    s->copy_write_buffer = buffer; break;
        case GL_PIXEL_PACK_BUFFER:    s->pixel_pack_buffer = buffer; break;
        case GL_PIXEL_UNPACK_BUFFER:  s->pixel_unpack_buffer = buffer; break;
        case GL_ELEMENT_ARRAY_BUFFER:
        case GL_TRANSFORM_FEEDBACK_BUFFER:
            break;
        case GL_SHADER_STORAGE_BUFFER:
        case GL_ATOMIC_COUNTER_BUFFER:
        case GL_DRAW_INDIRECT_BUFFER:
        case GL_DISPATCH_INDIRECT_BUFFER:
            if (!gl31_caps.es31) {
                gl31_stub_warn("SSBO / atomic counters / buffers indirectos (backend sin ES 3.1)");
                gl31_set_error(GL_INVALID_ENUM);
                return;
            }
            break;
        default:  /* incluye GL_TEXTURE_BUFFER (no soportado) */
            gl31_set_error(GL_INVALID_ENUM);
            return;
    }
    BE(glBindBuffer)(target, buffer);
}

void gl31_glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage)
{
    if (size < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glBufferData)(target, size, data, usage);
}

void gl31_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
{
    if (offset < 0 || size < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glBufferSubData)(target, offset, size, data);
}

/* glBufferStorage (GL 4.4) emulado sobre glBufferData. Sin mapeo persistente. */
void gl31_glBufferStorage(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags)
{
    GLenum usage;
    if (flags & (GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT)) {
        gl31_stub_warn("glBufferStorage(PERSISTENT/COHERENT)");
        gl31_set_error(GL_INVALID_OPERATION);
        return;
    }
    usage = (flags & (GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT)) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    gl31_glBufferData(target, size, data, usage);
}

void gl31_glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
    gl31_state_t* s = gl31_state();
    if (target == GL_UNIFORM_BUFFER) {
        gl31_state_load_limits();
        if ((GLint)index >= s->max_ubo_bindings) { gl31_set_error(GL_INVALID_VALUE); return; }
        s->ubo[index].buffer = buffer;
        s->ubo[index].offset = 0;
        s->ubo[index].size = 0;
        s->uniform_buffer = buffer;
    }
    BE(glBindBufferBase)(target, index, buffer);
}

void gl31_glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    gl31_state_t* s = gl31_state();
    if (offset < 0 || size < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (target == GL_UNIFORM_BUFFER) {
        gl31_state_load_limits();
        if ((GLint)index >= s->max_ubo_bindings) { gl31_set_error(GL_INVALID_VALUE); return; }
        if (buffer && (offset % s->ubo_alignment) != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
        s->ubo[index].buffer = buffer;
        s->ubo[index].offset = offset;
        s->ubo[index].size = size;
        s->uniform_buffer = buffer;
    }
    BE(glBindBufferRange)(target, index, buffer, offset, size);
}

void gl31_glCopyBufferSubData(GLenum rt, GLenum wt, GLintptr ro, GLintptr wo, GLsizeiptr size)
{
    if (ro < 0 || wo < 0 || size < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glCopyBufferSubData)(rt, wt, ro, wo, size);
}

void* gl31_glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    if (offset < 0 || length < 0) { gl31_set_error(GL_INVALID_VALUE); return NULL; }
    if (access & (GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT)) {
        gl31_set_error(GL_INVALID_OPERATION);
        return NULL;
    }
    return BE(glMapBufferRange)(target, offset, length, access);
}

void* gl31_glMapBuffer(GLenum target, GLenum access)
{
    GLint size = 0;
    GLbitfield flags;
    switch (access) {
        case GL_READ_ONLY:  flags = GL_MAP_READ_BIT; break;
        case GL_WRITE_ONLY: flags = GL_MAP_WRITE_BIT; break;
        case GL_READ_WRITE: flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT; break;
        default: gl31_set_error(GL_INVALID_ENUM); return NULL;
    }
    BE(glGetBufferParameteriv)(target, GL_BUFFER_SIZE, &size);
    if (size <= 0) { gl31_set_error(GL_INVALID_OPERATION); return NULL; }
    return BE(glMapBufferRange)(target, 0, size, flags);
}

GLboolean gl31_glUnmapBuffer(GLenum target) { return BE(glUnmapBuffer)(target); }

void gl31_glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params)
{
    BE(glGetBufferParameteriv)(target, pname, params);
}

GLboolean gl31_glIsBuffer(GLuint buffer) { return BE(glIsBuffer)(buffer); }

void gl31_glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length)
{
    if (offset < 0 || length < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glFlushMappedBufferRange)(target, offset, length);
}

void gl31_glGetBufferPointerv(GLenum target, GLenum pname, void** params)
{
    if (!params) return;
    if (pname != GL_BUFFER_MAP_POINTER) { gl31_set_error(GL_INVALID_ENUM); return; }
    BE(glGetBufferPointerv)(target, pname, params);
}

/* GL 4.4: enlazar varios buffers de una vez (emulado con un bucle) */
void gl31_glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint* buffers)
{
    GLsizei i;
    if (count < 0) { gl31_set_error(GL_INVALID_OPERATION); return; }
    for (i = 0; i < count; i++) {
        gl31_glBindBufferBase(target, first + (GLuint)i, buffers ? buffers[i] : 0);
        if (gl31_err_peek() != GL_NO_ERROR) return;
    }
}

void gl31_glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint* buffers,
                             const GLintptr* offsets, const GLsizeiptr* sizes)
{
    GLsizei i;
    if (count < 0) { gl31_set_error(GL_INVALID_OPERATION); return; }
    if (buffers && (!offsets || !sizes)) { gl31_set_error(GL_INVALID_VALUE); return; }
    for (i = 0; i < count; i++) {
        if (buffers && buffers[i]) gl31_glBindBufferRange(target, first + (GLuint)i, buffers[i], offsets[i], sizes[i]);
        else                       gl31_glBindBufferBase(target, first + (GLuint)i, 0);
        if (gl31_err_peek() != GL_NO_ERROR) return;
    }
}
