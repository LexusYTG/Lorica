/* gl31_compute.c - funciones de GL 4.x que ES 3.1 trae casi tal cual:
 * compute shaders, image load/store, memory barriers, program interface query,
 * framebuffers sin attachments y separate shader objects (pipelines + glProgramUniform*).
 * Todo requiere gl31_caps.es31; sin el, se avisa una vez y se genera GL_INVALID_OPERATION. */
#include "gl31.h"
#include <string.h>

#ifndef GL_ALL_BARRIER_BITS
#define GL_ALL_BARRIER_BITS 0xFFFFFFFFu
#endif
#define ES_BARRIER_MASK 0x3FEFu       /* bits de glMemoryBarrier que ES 3.1 conoce */

static int need_es31(const char* fn)
{
    if (gl31_caps.es31) return 1;
    gl31_stub_warn(fn);
    gl31_set_error(GL_INVALID_OPERATION);
    return 0;
}

/* ---------- compute ---------- */
static int have_program(void)
{
    gl31_state_t* s = gl31_state();
    return s->program != 0 || s->pipeline != 0;
}

void gl31_glDispatchCompute(GLuint x, GLuint y, GLuint z)
{
    if (!need_es31("glDispatchCompute (backend sin ES 3.1)")) return;
    if (!have_program()) { gl31_set_error(GL_INVALID_OPERATION); return; }
    BE(glDispatchCompute)(x, y, z);
}

void gl31_glDispatchComputeIndirect(GLintptr offset)
{
    if (!need_es31("glDispatchComputeIndirect (backend sin ES 3.1)")) return;
    if (offset < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!have_program()) { gl31_set_error(GL_INVALID_OPERATION); return; }
    BE(glDispatchComputeIndirect)(offset);
}

void gl31_glMemoryBarrier(GLbitfield barriers)
{
    GLbitfield es;
    if (!need_es31("glMemoryBarrier (backend sin ES 3.1)")) return;
    es = barriers & ES_BARRIER_MASK;
    /* bits de escritorio que ES no tiene (CLIENT_MAPPED_BUFFER 0x4000, QUERY_BUFFER 0x8000):
     * se cubren con una barrera completa */
    if (barriers && (barriers & ~(GLbitfield)ES_BARRIER_MASK)) es = ES_BARRIER_MASK;
    if (!es) return;
    BE(glMemoryBarrier)(es);
}

void gl31_glMemoryBarrierByRegion(GLbitfield barriers)
{
    GLbitfield es;
    if (!need_es31("glMemoryBarrierByRegion (backend sin ES 3.1)")) return;
    es = barriers & ES_BARRIER_MASK;
    if (barriers && (barriers & ~(GLbitfield)ES_BARRIER_MASK)) es = ES_BARRIER_MASK;
    if (!es) return;
    BE(glMemoryBarrierByRegion)(es);
}

/* ---------- image load/store ---------- */
void gl31_glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer,
                             GLenum access, GLenum format)
{
    GLint max = 4;
    if (!need_es31("glBindImageTexture (backend sin ES 3.1)")) return;
    BE(glGetIntegerv)(GL_MAX_IMAGE_UNITS, &max);
    if (max <= 0) max = 4;
    if ((GLint)unit >= max || level < 0 || layer < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (access != GL_READ_ONLY && access != GL_WRITE_ONLY && access != GL_READ_WRITE) {
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    BE(glBindImageTexture)(unit, texture, level, layered, layer, access, format);
}

/* GL 4.4: formato = el de la textura; nivel 0, en capas, lectura/escritura */
void gl31_glBindImageTextures(GLuint first, GLsizei count, const GLuint* textures)
{
    GLsizei i;
    if (count < 0) { gl31_set_error(GL_INVALID_OPERATION); return; }
    for (i = 0; i < count; i++) {
        GLuint t = textures ? textures[i] : 0;
        gl31_glBindImageTexture(first + (GLuint)i, t, 0, GL_TRUE, 0, GL_READ_WRITE,
                                t ? gl31_tex_level0_format(t) : GL_RGBA8);
        if (gl31_err_peek() != GL_NO_ERROR) return;
    }
}

/* ---------- framebuffer sin attachments ---------- */
void gl31_glFramebufferParameteri(GLenum target, GLenum pname, GLint param)
{
    if (!need_es31("glFramebufferParameteri (backend sin ES 3.1)")) return;
    if (target != GL_FRAMEBUFFER && target != GL_DRAW_FRAMEBUFFER && target != GL_READ_FRAMEBUFFER) {
        gl31_set_error(GL_INVALID_ENUM);
        return;
    }
    BE(glFramebufferParameteri)(target, pname, param);
}

void gl31_glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint* params)
{
    if (!params) return;
    if (!need_es31("glGetFramebufferParameteriv (backend sin ES 3.1)")) return;
    BE(glGetFramebufferParameteriv)(target, pname, params);
}

/* ---------- program interface query ---------- */
static int prog_linked(GLuint program)
{
    int st = gl31_program_status(program);
    if (st == -2) { gl31_set_error(GL_INVALID_OPERATION); return 0; }
    if (st == -1) { gl31_set_error(GL_INVALID_VALUE); return 0; }
    if (st == 0)  { gl31_set_error(GL_INVALID_OPERATION); return 0; }
    return 1;
}

void gl31_glGetProgramInterfaceiv(GLuint program, GLenum iface, GLenum pname, GLint* params)
{
    if (!params) return;
    if (!need_es31("glGetProgramInterfaceiv (backend sin ES 3.1)")) return;
    if (!prog_linked(program)) return;
    BE(glGetProgramInterfaceiv)(program, iface, pname, params);
}

GLuint gl31_glGetProgramResourceIndex(GLuint program, GLenum iface, const GLchar* name)
{
    if (!name) return GL_INVALID_INDEX;
    if (!need_es31("glGetProgramResourceIndex (backend sin ES 3.1)")) return GL_INVALID_INDEX;
    if (!prog_linked(program)) return GL_INVALID_INDEX;
    return BE(glGetProgramResourceIndex)(program, iface, name);
}

void gl31_glGetProgramResourceName(GLuint program, GLenum iface, GLuint index, GLsizei bufSize,
                                   GLsizei* length, GLchar* name)
{
    if (bufSize < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!need_es31("glGetProgramResourceName (backend sin ES 3.1)")) return;
    if (!prog_linked(program)) return;
    BE(glGetProgramResourceName)(program, iface, index, bufSize, length, name);
}

GLint gl31_glGetProgramResourceLocation(GLuint program, GLenum iface, const GLchar* name)
{
    if (!name) return -1;
    if (!need_es31("glGetProgramResourceLocation (backend sin ES 3.1)")) return -1;
    if (!prog_linked(program)) return -1;
    return BE(glGetProgramResourceLocation)(program, iface, name);
}

/* GL_TYPE de un uniform sampler1D/Rect: el backend ve 2D, se informa el tipo de la app */
void gl31_glGetProgramResourceiv(GLuint program, GLenum iface, GLuint index, GLsizei propCount,
                                 const GLenum* props, GLsizei bufSize, GLsizei* length, GLint* params)
{
    GLsizei i, n = 0;
    int scalar = 1;
    if (propCount < 0 || bufSize < 0 || (propCount > 0 && !props)) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!need_es31("glGetProgramResourceiv (backend sin ES 3.1)")) return;
    if (!prog_linked(program)) return;
    BE(glGetProgramResourceiv)(program, iface, index, propCount, props, bufSize, length, params);
    if (iface != GL_UNIFORM || !params) return;
    for (i = 0; i < propCount; i++) if (props[i] == GL_ACTIVE_VARIABLES) scalar = 0;
    if (!scalar) return;
    if (length) n = *length; else n = propCount < bufSize ? propCount : bufSize;
    for (i = 0; i < propCount && i < n; i++)
        if (props[i] == GL_TYPE) {
            char nm[256];
            GLsizei l = 0;
            nm[0] = 0;
            BE(glGetProgramResourceName)(program, iface, index, (GLsizei)sizeof nm, &l, nm);
            params[i] = (GLint)gl31_program_uniform_type(program, nm, (GLenum)params[i]);
        }
}

/* Los bloques SSBO solo se pueden enlazar con layout(binding=N) en ES: no hay equivalente de
 * glShaderStorageBlockBinding (ver GL4.md). */
void gl31_glShaderStorageBlockBinding(GLuint program, GLuint index, GLuint binding)
{
    (void)program; (void)index; (void)binding;
    gl31_stub_warn("glShaderStorageBlockBinding (usar layout(binding=N) en el shader)");
    gl31_set_error(GL_INVALID_OPERATION);
}

/* ---------- separate shader objects ---------- */
void gl31_glGenProgramPipelines(GLsizei n, GLuint* pipelines)
{
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!need_es31("glGenProgramPipelines (backend sin ES 3.1)")) return;
    BE(glGenProgramPipelines)(n, pipelines);
}

void gl31_glDeleteProgramPipelines(GLsizei n, const GLuint* pipelines)
{
    GLsizei i;
    gl31_state_t* s = gl31_state();
    if (n < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!need_es31("glDeleteProgramPipelines (backend sin ES 3.1)")) return;
    for (i = 0; i < n; i++) if (pipelines[i] == s->pipeline) s->pipeline = 0;
    BE(glDeleteProgramPipelines)(n, pipelines);
}

void gl31_glBindProgramPipeline(GLuint pipeline)
{
    if (!need_es31("glBindProgramPipeline (backend sin ES 3.1)")) return;
    gl31_state()->pipeline = pipeline;
    BE(glBindProgramPipeline)(pipeline);
}

GLboolean gl31_glIsProgramPipeline(GLuint pipeline)
{
    if (!gl31_caps.es31 || !pipeline) return GL_FALSE;
    return BE(glIsProgramPipeline)(pipeline);
}

void gl31_glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program)
{
    if (!need_es31("glUseProgramStages (backend sin ES 3.1)")) return;
    if (program && !prog_linked(program)) return;
    BE(glUseProgramStages)(pipeline, stages, program);
}

void gl31_glActiveShaderProgram(GLuint pipeline, GLuint program)
{
    if (!need_es31("glActiveShaderProgram (backend sin ES 3.1)")) return;
    if (program && !prog_linked(program)) return;
    BE(glActiveShaderProgram)(pipeline, program);
}

void gl31_glValidateProgramPipeline(GLuint pipeline)
{
    if (!need_es31("glValidateProgramPipeline (backend sin ES 3.1)")) return;
    BE(glValidateProgramPipeline)(pipeline);
}

void gl31_glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint* params)
{
    if (!params) return;
    if (!need_es31("glGetProgramPipelineiv (backend sin ES 3.1)")) return;
    BE(glGetProgramPipelineiv)(pipeline, pname, params);
}

void gl31_glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize, GLsizei* length, GLchar* log)
{
    if (bufSize < 0) { gl31_set_error(GL_INVALID_VALUE); return; }
    if (!need_es31("glGetProgramPipelineInfoLog (backend sin ES 3.1)")) return;
    BE(glGetProgramPipelineInfoLog)(pipeline, bufSize, length, log);
}

/* ---------- glProgramUniform* ---------- */
static int pu_ok(GLuint program, GLint location)
{
    if (!need_es31("glProgramUniform* (backend sin ES 3.1)")) return 0;
    if (!prog_linked(program)) return 0;
    return location != -1;               /* location -1 se ignora en silencio */
}

static int pu_count(GLsizei count, const void* v)
{
    if (count < 0) { gl31_set_error(GL_INVALID_VALUE); return 0; }
    if (count > 0 && !v) { gl31_set_error(GL_INVALID_VALUE); return 0; }
    return 1;
}

void gl31_glProgramUniform1f(GLuint program, GLint location, GLfloat v0)
{
    if (!pu_ok(program, location)) return;
    BE(glProgramUniform1f)(program, location, v0);
}

void gl31_glProgramUniform2f(GLuint program, GLint location, GLfloat v0, GLfloat v1)
{
    if (!pu_ok(program, location)) return;
    BE(glProgramUniform2f)(program, location, v0, v1);
}

void gl31_glProgramUniform3f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    if (!pu_ok(program, location)) return;
    BE(glProgramUniform3f)(program, location, v0, v1, v2);
}

void gl31_glProgramUniform4f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    if (!pu_ok(program, location)) return;
    BE(glProgramUniform4f)(program, location, v0, v1, v2, v3);
}

void gl31_glProgramUniform1i(GLuint program, GLint location, GLint v0)
{
    if (!pu_ok(program, location)) return;
    BE(glProgramUniform1i)(program, location, v0);
}

void gl31_glProgramUniform2i(GLuint program, GLint location, GLint v0, GLint v1)
{
    if (!pu_ok(program, location)) return;
    BE(glProgramUniform2i)(program, location, v0, v1);
}

void gl31_glProgramUniform3i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2)
{
    if (!pu_ok(program, location)) return;
    BE(glProgramUniform3i)(program, location, v0, v1, v2);
}

void gl31_glProgramUniform4i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
    if (!pu_ok(program, location)) return;
    BE(glProgramUniform4i)(program, location, v0, v1, v2, v3);
}

void gl31_glProgramUniform1ui(GLuint program, GLint location, GLuint v0)
{
    if (!pu_ok(program, location)) return;
    BE(glProgramUniform1ui)(program, location, v0);
}

void gl31_glProgramUniform2ui(GLuint program, GLint location, GLuint v0, GLuint v1)
{
    if (!pu_ok(program, location)) return;
    BE(glProgramUniform2ui)(program, location, v0, v1);
}

void gl31_glProgramUniform3ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    if (!pu_ok(program, location)) return;
    BE(glProgramUniform3ui)(program, location, v0, v1, v2);
}

void gl31_glProgramUniform4ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    if (!pu_ok(program, location)) return;
    BE(glProgramUniform4ui)(program, location, v0, v1, v2, v3);
}

void gl31_glProgramUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniform1fv)(program, location, count, value);
}

void gl31_glProgramUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniform2fv)(program, location, count, value);
}

void gl31_glProgramUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniform3fv)(program, location, count, value);
}

void gl31_glProgramUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniform4fv)(program, location, count, value);
}

void gl31_glProgramUniform1iv(GLuint program, GLint location, GLsizei count, const GLint* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniform1iv)(program, location, count, value);
}

void gl31_glProgramUniform2iv(GLuint program, GLint location, GLsizei count, const GLint* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniform2iv)(program, location, count, value);
}

void gl31_glProgramUniform3iv(GLuint program, GLint location, GLsizei count, const GLint* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniform3iv)(program, location, count, value);
}

void gl31_glProgramUniform4iv(GLuint program, GLint location, GLsizei count, const GLint* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniform4iv)(program, location, count, value);
}

void gl31_glProgramUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniform1uiv)(program, location, count, value);
}

void gl31_glProgramUniform2uiv(GLuint program, GLint location, GLsizei count, const GLuint* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniform2uiv)(program, location, count, value);
}

void gl31_glProgramUniform3uiv(GLuint program, GLint location, GLsizei count, const GLuint* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniform3uiv)(program, location, count, value);
}

void gl31_glProgramUniform4uiv(GLuint program, GLint location, GLsizei count, const GLuint* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniform4uiv)(program, location, count, value);
}

void gl31_glProgramUniformMatrix2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniformMatrix2fv)(program, location, count, transpose, value);
}

void gl31_glProgramUniformMatrix3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniformMatrix3fv)(program, location, count, transpose, value);
}

void gl31_glProgramUniformMatrix4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniformMatrix4fv)(program, location, count, transpose, value);
}

void gl31_glProgramUniformMatrix2x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniformMatrix2x3fv)(program, location, count, transpose, value);
}

void gl31_glProgramUniformMatrix3x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniformMatrix3x2fv)(program, location, count, transpose, value);
}

void gl31_glProgramUniformMatrix2x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniformMatrix2x4fv)(program, location, count, transpose, value);
}

void gl31_glProgramUniformMatrix4x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniformMatrix4x2fv)(program, location, count, transpose, value);
}

void gl31_glProgramUniformMatrix3x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniformMatrix3x4fv)(program, location, count, transpose, value);
}

void gl31_glProgramUniformMatrix4x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    if (!pu_ok(program, location) || !pu_count(count, value)) return;
    BE(glProgramUniformMatrix4x3fv)(program, location, count, transpose, value);
}
