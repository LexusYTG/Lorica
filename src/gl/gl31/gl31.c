/* gl31.c - tabla de despacho: nombre GL de escritorio -> implementacion gl31_* */
#include "gl31.h"
#include <stdio.h>
#include <string.h>

/* gl31_init / gl31_shutdown y la instancia gl31_be viven en gl31_caps.c */

typedef struct { const char* name; void* fn; } gl31_entry_t;
#define D(n) { #n, (void*)gl31_##n }

static const gl31_entry_t gl31_table[] = {
    D(glGetError), D(glEnable), D(glDisable), D(glIsEnabled), D(glPrimitiveRestartIndex),
    D(glGenSamplers), D(glDeleteSamplers), D(glBindSampler), D(glSamplerParameteri),
    D(glSamplerParameterf), D(glIsSampler), D(glGetSamplerParameteriv),
    D(glGenVertexArrays), D(glDeleteVertexArrays), D(glBindVertexArray), D(glIsVertexArray),
    D(glVertexAttribPointer), D(glVertexAttribIPointer), D(glEnableVertexAttribArray),
    D(glDisableVertexAttribArray), D(glVertexAttribDivisor),
    D(glGenBuffers), D(glDeleteBuffers), D(glBindBuffer), D(glBufferData), D(glBufferSubData),
    D(glBufferStorage), D(glBindBufferBase), D(glBindBufferRange), D(glCopyBufferSubData),
    D(glMapBuffer), D(glMapBufferRange), D(glUnmapBuffer), D(glGetBufferParameteriv), D(glIsBuffer),
    D(glGenTextures), D(glDeleteTextures), D(glBindTexture), D(glActiveTexture),
    D(glTexParameteri), D(glTexStorage2D), D(glTexStorage3D),
    D(glDrawArrays), D(glDrawElements), D(glDrawArraysInstanced), D(glDrawElementsInstanced),
    D(glDrawRangeElements), D(glMultiDrawArrays), D(glMultiDrawElements),
    D(glGetUniformBlockIndex), D(glUniformBlockBinding), D(glGetActiveUniformBlockiv),
    D(glGetActiveUniformBlockName), D(glGetUniformIndices), D(glGetActiveUniformsiv),
    D(glGetIntegeri_v),
    D(glGenQueries), D(glDeleteQueries), D(glIsQuery), D(glBeginQuery), D(glEndQuery),
    D(glGetQueryiv), D(glGetQueryObjectuiv), D(glGetQueryObjectiv),
    D(glGetString), D(glGetStringi), D(glGetIntegerv),
    D(glCreateShader), D(glShaderSource), D(glCompileShader), D(glGetShaderiv),
    D(glGetShaderInfoLog), D(glDeleteShader), D(glCreateProgram), D(glAttachShader),
    D(glDetachShader), D(glLinkProgram), D(glGetProgramiv), D(glGetProgramInfoLog),
    D(glUseProgram), D(glDeleteProgram), D(glBindAttribLocation), D(glGetAttribLocation),
    D(glGetUniformLocation),
    D(glTexBuffer), D(glBindFragDataLocation), D(glTexImage1D), D(glGetTexImage),
    D(glGetBufferSubData), D(glPolygonMode), D(glClampColor),

    /* ---- bloque 1: API completa ---- */
    D(glUniform1f), D(glUniform1i), D(glUniform1ui), D(glUniform2f), D(glUniform2i),
    D(glUniform2ui), D(glUniform3f), D(glUniform3i), D(glUniform3ui), D(glUniform4f),
    D(glUniform4i), D(glUniform4ui), D(glUniform1fv), D(glUniform1iv), D(glUniform1uiv),
    D(glUniform2fv), D(glUniform2iv), D(glUniform2uiv), D(glUniform3fv), D(glUniform3iv),
    D(glUniform3uiv), D(glUniform4fv), D(glUniform4iv), D(glUniform4uiv),
    D(glUniformMatrix2fv), D(glUniformMatrix3fv), D(glUniformMatrix4fv),
    D(glUniformMatrix2x3fv), D(glUniformMatrix3x2fv), D(glUniformMatrix2x4fv),
    D(glUniformMatrix4x2fv), D(glUniformMatrix3x4fv), D(glUniformMatrix4x3fv),
    D(glGetUniformfv), D(glGetUniformiv), D(glGetUniformuiv), D(glGetActiveUniform),
    D(glGetActiveUniformName), D(glGetActiveAttrib), D(glGetFragDataLocation),
    D(glVertexAttrib1f), D(glVertexAttrib1s), D(glVertexAttrib1d), D(glVertexAttrib1fv),
    D(glVertexAttrib1sv), D(glVertexAttrib1dv), D(glVertexAttrib2f), D(glVertexAttrib2s),
    D(glVertexAttrib2d), D(glVertexAttrib2fv), D(glVertexAttrib2sv), D(glVertexAttrib2dv),
    D(glVertexAttrib3f), D(glVertexAttrib3s), D(glVertexAttrib3d), D(glVertexAttrib3fv),
    D(glVertexAttrib3sv), D(glVertexAttrib3dv), D(glVertexAttrib4f), D(glVertexAttrib4s),
    D(glVertexAttrib4d), D(glVertexAttrib4fv), D(glVertexAttrib4sv), D(glVertexAttrib4dv),
    D(glVertexAttrib4Nub), D(glVertexAttrib4Nbv), D(glVertexAttrib4Nsv),
    D(glVertexAttrib4Nubv), D(glVertexAttrib4Nusv), D(glVertexAttrib4Nuiv),
    D(glVertexAttrib4bv), D(glVertexAttrib4iv), D(glVertexAttrib4ubv),
    D(glVertexAttrib4uiv), D(glVertexAttrib4usv), D(glVertexAttribI1i),
    D(glVertexAttribI1ui), D(glVertexAttribI1iv), D(glVertexAttribI1uiv),
    D(glVertexAttribI2i), D(glVertexAttribI2ui), D(glVertexAttribI2iv),
    D(glVertexAttribI2uiv), D(glVertexAttribI3i), D(glVertexAttribI3ui),
    D(glVertexAttribI3iv), D(glVertexAttribI3uiv), D(glVertexAttribI4i),
    D(glVertexAttribI4ui), D(glVertexAttribI4iv), D(glVertexAttribI4uiv),
    D(glVertexAttribI4bv), D(glVertexAttribI4sv), D(glVertexAttribI4ubv),
    D(glVertexAttribI4usv), D(glGetVertexAttribiv), D(glGetVertexAttribfv),
    D(glGetVertexAttribdv), D(glGetVertexAttribIiv), D(glGetVertexAttribIuiv),
    D(glGetVertexAttribPointerv), D(glFlushMappedBufferRange), D(glGetBufferPointerv),
    D(glTexImage2D), D(glTexImage3D), D(glTexSubImage2D), D(glTexSubImage3D),
    D(glCopyTexImage2D), D(glCopyTexSubImage2D), D(glCopyTexSubImage3D),
    D(glCompressedTexImage2D), D(glCompressedTexImage3D), D(glCompressedTexSubImage2D),
    D(glCompressedTexSubImage3D), D(glGenerateMipmap), D(glPixelStorei), D(glPixelStoref),
    D(glTexParameterf), D(glTexParameterfv), D(glTexParameteriv), D(glTexParameterIiv),
    D(glTexParameterIuiv), D(glGetTexParameteriv), D(glGetTexParameterfv),
    D(glGetTexParameterIiv), D(glGetTexParameterIuiv), D(glGetTexLevelParameteriv),
    D(glGetTexLevelParameterfv), D(glIsTexture), D(glGenFramebuffers),
    D(glDeleteFramebuffers), D(glBindFramebuffer), D(glIsFramebuffer),
    D(glCheckFramebufferStatus), D(glFramebufferTexture2D), D(glFramebufferTexture3D),
    D(glFramebufferTextureLayer), D(glFramebufferRenderbuffer),
    D(glGetFramebufferAttachmentParameteriv), D(glGenRenderbuffers),
    D(glDeleteRenderbuffers), D(glBindRenderbuffer), D(glIsRenderbuffer),
    D(glRenderbufferStorage), D(glRenderbufferStorageMultisample),
    D(glGetRenderbufferParameteriv), D(glBlitFramebuffer), D(glDrawBuffer),
    D(glDrawBuffers), D(glReadBuffer), D(glReadPixels), D(glClearBufferiv),
    D(glClearBufferuiv), D(glClearBufferfv), D(glClearBufferfi), D(glClear),
    D(glClearColor), D(glClearDepth), D(glClearStencil), D(glViewport), D(glScissor),
    D(glDepthRange), D(glDepthFunc), D(glDepthMask), D(glBlendFunc), D(glBlendFuncSeparate),
    D(glBlendEquation), D(glBlendEquationSeparate), D(glBlendColor), D(glStencilFunc),
    D(glStencilFuncSeparate), D(glStencilOp), D(glStencilOpSeparate), D(glStencilMask),
    D(glStencilMaskSeparate), D(glCullFace), D(glFrontFace), D(glColorMask),
    D(glColorMaski), D(glEnablei), D(glDisablei), D(glIsEnabledi), D(glPolygonOffset),
    D(glLineWidth), D(glPointSize), D(glHint), D(glSampleCoverage), D(glFlush), D(glFinish),
    D(glGetBooleanv), D(glGetFloatv), D(glGetDoublev), D(glIsShader), D(glIsProgram),
    D(glValidateProgram), D(glGetShaderSource), D(glGetAttachedShaders),
    D(glBeginTransformFeedback), D(glEndTransformFeedback), D(glTransformFeedbackVaryings),
    D(glGetTransformFeedbackVarying), D(glBeginConditionalRender),
    D(glEndConditionalRender),

    /* ---- bloque 2: funciones que existian pero no estaban publicadas ---- */
    D(glGetBooleani_v), D(glPointParameterf), D(glPointParameteri), D(glPointParameterfv),
    D(glPointParameteriv), D(glLogicOp), D(glFramebufferTexture1D), D(glTexStorage1D),
};

void* gl31_get_proc_address(const char* name)
{
    size_t i;
    if (!name) return NULL;
    for (i = 0; i < sizeof(gl31_table) / sizeof(gl31_table[0]); i++)
        if (strcmp(gl31_table[i].name, name) == 0)
            return gl31_table[i].fn;
    return NULL;
}
