/* gl31_sync.c - objetos sync (GL 3.2, ARB_sync). ES 3.0 los tiene con la misma
 * semantica, asi que casi todo es validacion + reenvio. */
#include "gl31.h"

#ifndef GL_SYNC_FLUSH_COMMANDS_BIT
#define GL_SYNC_FLUSH_COMMANDS_BIT 0x00000001
#endif

GLsync gl31_glFenceSync(GLenum condition, GLbitfield flags)
{
    if (condition != GL_SYNC_GPU_COMMANDS_COMPLETE) { gl31_set_error(GL_INVALID_ENUM); return NULL; }
    if (flags != 0) { gl31_set_error(GL_INVALID_VALUE); return NULL; }
    return BE(glFenceSync)(condition, flags);
}

GLboolean gl31_glIsSync(GLsync sync) { return sync ? BE(glIsSync)(sync) : GL_FALSE; }

void gl31_glDeleteSync(GLsync sync)
{
    if (!sync) return;                       /* GL: 0 se ignora en silencio */
    BE(glDeleteSync)(sync);
}

GLenum gl31_glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    if (!sync || !BE(glIsSync)(sync)) { gl31_set_error(GL_INVALID_VALUE); return GL_WAIT_FAILED; }
    if (flags & ~(GLbitfield)GL_SYNC_FLUSH_COMMANDS_BIT) { gl31_set_error(GL_INVALID_VALUE); return GL_WAIT_FAILED; }
    return BE(glClientWaitSync)(sync, flags, timeout);
}

void gl31_glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    if (!sync || !BE(glIsSync)(sync)) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (flags != 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (timeout != GL_TIMEOUT_IGNORED) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glWaitSync)(sync, flags, timeout);
}

void gl31_glGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* values)
{
    if (bufSize < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!sync || !BE(glIsSync)(sync)) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (bufSize > 0 && !values) { gl31_set_error(GL_INVALID_VALUE); return; }
    BE(glGetSynciv)(sync, pname, bufSize, length, values);
}

void gl31_glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64* params)
{
    if (!params) return;
    BE(glGetBufferParameteri64v)(target, pname, params);
}
