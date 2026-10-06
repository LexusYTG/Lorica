/*
 * Lorica - GL 3.1 core layer (minimal)
 *
 * Traduce llamadas OpenGL 3.1 core a un backend GLES 3.0+ cargado
 * mediante un loader (eglGetProcAddress / dlsym).
 * Todas las llamadas al backend pasan por BE(nombre).
 */
#ifndef LORICA_GL31_H
#define LORICA_GL31_H

#include <stddef.h>
#include <stdint.h>

#ifdef LORICA_GL31_PLATFORM_HEADER
#include LORICA_GL31_PLATFORM_HEADER
#else
/* Headers oficiales de Khronos (ver third_party/khronos). Sin prototipos: todo
 * acceso al backend pasa por BE(), asi un glFoo() directo no compila. */
#ifndef GL_GLES_PROTOTYPES
#define GL_GLES_PROTOTYPES 0
#endif
#include <GLES3/gl3.h>
#endif

#define GL31_TLS __thread

/* tipos de GL desktop ausentes en GLES */
#ifndef GL_VERSION_1_1
typedef double GLdouble;
typedef double GLclampd;
#endif

/* ---------- enums de GL desktop ausentes en GLES ---------- */
#ifndef GL_QUADS
#define GL_QUADS 0x0007
#endif
#ifndef GL_PRIMITIVE_RESTART
#define GL_PRIMITIVE_RESTART 0x8F9D
#endif
#ifndef GL_PRIMITIVE_RESTART_INDEX
#define GL_PRIMITIVE_RESTART_INDEX 0x8F9E
#endif
#ifndef GL_PRIMITIVE_RESTART_FIXED_INDEX
#define GL_PRIMITIVE_RESTART_FIXED_INDEX 0x8D69
#endif
#ifndef GL_SAMPLES_PASSED
#define GL_SAMPLES_PASSED 0x8914
#endif
#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif
#ifndef GL_PRIMITIVES_GENERATED
#define GL_PRIMITIVES_GENERATED 0x8C87
#endif
#ifndef GL_TEXTURE_1D
#define GL_TEXTURE_1D 0x0DE0
#endif
#ifndef GL_TEXTURE_RECTANGLE
#define GL_TEXTURE_RECTANGLE 0x84F5
#endif
#ifndef GL_TEXTURE_BUFFER
#define GL_TEXTURE_BUFFER 0x8C2A
#endif
#ifndef GL_MAX_TEXTURE_BUFFER_SIZE
#define GL_MAX_TEXTURE_BUFFER_SIZE 0x8C2B
#endif
#ifndef GL_MAX_RECTANGLE_TEXTURE_SIZE
#define GL_MAX_RECTANGLE_TEXTURE_SIZE 0x84F8
#endif
#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif
#ifndef GL_READ_ONLY
#define GL_READ_ONLY 0x88B8
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_WRITE 0x88BA
#endif
#ifndef GL_MAP_PERSISTENT_BIT
#define GL_MAP_PERSISTENT_BIT 0x0040
#define GL_MAP_COHERENT_BIT 0x0080
#define GL_DYNAMIC_STORAGE_BIT 0x0100
#define GL_CLIENT_STORAGE_BIT 0x0200
#endif

#ifndef GL_PROXY_TEXTURE_2D
#define GL_PROXY_TEXTURE_2D       0x8064
#define GL_PROXY_TEXTURE_3D       0x8070
#define GL_PROXY_TEXTURE_CUBE_MAP 0x851B
#define GL_PROXY_TEXTURE_2D_ARRAY 0x8C1B
#endif
#ifndef GL_DRAW_BUFFER
#define GL_DRAW_BUFFER 0x0C01
#endif
#ifndef GL_FRONT_LEFT
#define GL_FRONT_LEFT  0x0400
#define GL_BACK_LEFT   0x0402
#define GL_FRONT_RIGHT 0x0401
#define GL_BACK_RIGHT  0x0403
#endif

#ifndef GL_TEXTURE_1D_ARRAY
#define GL_TEXTURE_1D_ARRAY 0x8C18
#endif
#ifndef GL_PROXY_TEXTURE_1D
#define GL_PROXY_TEXTURE_1D 0x8063
#define GL_PROXY_TEXTURE_1D_ARRAY 0x8C19
#define GL_PROXY_TEXTURE_RECTANGLE 0x84F7
#endif
#ifndef GL_TEXTURE_BUFFER_OES
#define GL_TEXTURE_BUFFER_OES 0x8C2A
#define GL_TEXTURE_BUFFER_BINDING_OES 0x8C2A
#endif

/* tipos de sampler de desktop sin equivalente en ES (el conversor GLSL los rechaza,
 * pero las consultas de uniforms y el sync de texturas los reconocen) */
#ifndef GL_SAMPLER_1D
#define GL_SAMPLER_1D                      0x8B5D
#define GL_SAMPLER_2D_RECT                 0x8B63
#define GL_SAMPLER_1D_ARRAY                0x8DC0
#define GL_INT_SAMPLER_1D                  0x8DC9
#define GL_INT_SAMPLER_2D_RECT             0x8DCD
#define GL_INT_SAMPLER_1D_ARRAY            0x8DCE
#define GL_UNSIGNED_INT_SAMPLER_1D         0x8DD1
#define GL_UNSIGNED_INT_SAMPLER_2D_RECT    0x8DD5
#define GL_UNSIGNED_INT_SAMPLER_1D_ARRAY   0x8DD6
#endif
#ifndef GL_STENCIL_INDEX
#define GL_STENCIL_INDEX 0x1901
#endif

#define GL31_MAX_UBO_BINDINGS 84
#define GL31_MAX_TEX_UNITS 96

/* ---------- backend (GLES) ---------- */
#define GL31_BACKEND_FUNCS(X) \
    X(void, glGenVertexArrays, (GLsizei, GLuint*)) \
    X(void, glDeleteVertexArrays, (GLsizei, const GLuint*)) \
    X(void, glBindVertexArray, (GLuint)) \
    X(GLboolean, glIsVertexArray, (GLuint)) \
    X(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
    X(void, glVertexAttribIPointer, (GLuint, GLint, GLenum, GLsizei, const void*)) \
    X(void, glEnableVertexAttribArray, (GLuint)) \
    X(void, glDisableVertexAttribArray, (GLuint)) \
    X(void, glVertexAttribDivisor, (GLuint, GLuint)) \
    X(void, glGenBuffers, (GLsizei, GLuint*)) \
    X(void, glDeleteBuffers, (GLsizei, const GLuint*)) \
    X(void, glBindBuffer, (GLenum, GLuint)) \
    X(void, glBufferData, (GLenum, GLsizeiptr, const void*, GLenum)) \
    X(void, glBufferSubData, (GLenum, GLintptr, GLsizeiptr, const void*)) \
    X(void, glBindBufferBase, (GLenum, GLuint, GLuint)) \
    X(void, glBindBufferRange, (GLenum, GLuint, GLuint, GLintptr, GLsizeiptr)) \
    X(void, glCopyBufferSubData, (GLenum, GLenum, GLintptr, GLintptr, GLsizeiptr)) \
    X(void*, glMapBufferRange, (GLenum, GLintptr, GLsizeiptr, GLbitfield)) \
    X(GLboolean, glUnmapBuffer, (GLenum)) \
    X(void, glGetBufferParameteriv, (GLenum, GLenum, GLint*)) \
    X(GLboolean, glIsBuffer, (GLuint)) \
    X(void, glGenTextures, (GLsizei, GLuint*)) \
    X(void, glDeleteTextures, (GLsizei, const GLuint*)) \
    X(void, glBindTexture, (GLenum, GLuint)) \
    X(void, glActiveTexture, (GLenum)) \
    X(void, glTexStorage2D, (GLenum, GLsizei, GLenum, GLsizei, GLsizei)) \
    X(void, glTexStorage3D, (GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei)) \
    X(void, glTexParameteri, (GLenum, GLenum, GLint)) \
    X(void, glDrawArrays, (GLenum, GLint, GLsizei)) \
    X(void, glDrawElements, (GLenum, GLsizei, GLenum, const void*)) \
    X(void, glDrawArraysInstanced, (GLenum, GLint, GLsizei, GLsizei)) \
    X(void, glDrawElementsInstanced, (GLenum, GLsizei, GLenum, const void*, GLsizei)) \
    X(void, glDrawRangeElements, (GLenum, GLuint, GLuint, GLsizei, GLenum, const void*)) \
    X(GLuint, glGetUniformBlockIndex, (GLuint, const GLchar*)) \
    X(void, glUniformBlockBinding, (GLuint, GLuint, GLuint)) \
    X(void, glGetActiveUniformBlockiv, (GLuint, GLuint, GLenum, GLint*)) \
    X(void, glGetActiveUniformBlockName, (GLuint, GLuint, GLsizei, GLsizei*, GLchar*)) \
    X(void, glGetUniformIndices, (GLuint, GLsizei, const GLchar* const*, GLuint*)) \
    X(void, glGetActiveUniformsiv, (GLuint, GLsizei, const GLuint*, GLenum, GLint*)) \
    X(void, glGetIntegeri_v, (GLenum, GLuint, GLint*)) \
    X(void, glGenQueries, (GLsizei, GLuint*)) \
    X(void, glDeleteQueries, (GLsizei, const GLuint*)) \
    X(GLboolean, glIsQuery, (GLuint)) \
    X(void, glBeginQuery, (GLenum, GLuint)) \
    X(void, glEndQuery, (GLenum)) \
    X(void, glGetQueryiv, (GLenum, GLenum, GLint*)) \
    X(void, glGetQueryObjectuiv, (GLuint, GLenum, GLuint*)) \
    X(const GLubyte*, glGetString, (GLenum)) \
    X(const GLubyte*, glGetStringi, (GLenum, GLuint)) \
    X(void, glGetIntegerv, (GLenum, GLint*)) \
    X(GLenum, glGetError, (void)) \
    X(void, glEnable, (GLenum)) \
    X(void, glDisable, (GLenum)) \
    X(GLboolean, glIsEnabled, (GLenum)) \
    X(void, glGenSamplers, (GLsizei, GLuint*)) \
    X(void, glDeleteSamplers, (GLsizei, const GLuint*)) \
    X(void, glBindSampler, (GLuint, GLuint)) \
    X(void, glSamplerParameteri, (GLuint, GLenum, GLint)) \
    X(void, glSamplerParameterf, (GLuint, GLenum, GLfloat)) \
    X(GLboolean, glIsSampler, (GLuint)) \
    X(void, glGetSamplerParameteriv, (GLuint, GLenum, GLint*)) \
    X(GLuint, glCreateShader, (GLenum)) \
    X(void, glShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*)) \
    X(void, glCompileShader, (GLuint)) \
    X(void, glGetShaderiv, (GLuint, GLenum, GLint*)) \
    X(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
    X(void, glDeleteShader, (GLuint)) \
    X(GLuint, glCreateProgram, (void)) \
    X(void, glAttachShader, (GLuint, GLuint)) \
    X(void, glDetachShader, (GLuint, GLuint)) \
    X(void, glLinkProgram, (GLuint)) \
    X(void, glGetProgramiv, (GLuint, GLenum, GLint*)) \
    X(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
    X(void, glUseProgram, (GLuint)) \
    X(void, glDeleteProgram, (GLuint)) \
    X(void, glBindAttribLocation, (GLuint, GLuint, const GLchar*)) \
    X(GLint, glGetAttribLocation, (GLuint, const GLchar*)) \
    X(GLint, glGetUniformLocation, (GLuint, const GLchar*)) \
    X(void, glUniform1f, (GLint,GLfloat)) \
    X(void, glUniform1i, (GLint,GLint)) \
    X(void, glUniform1ui, (GLint,GLuint)) \
    X(void, glUniform1fv, (GLint,GLsizei,const GLfloat*)) \
    X(void, glUniform1iv, (GLint,GLsizei,const GLint*)) \
    X(void, glUniform1uiv, (GLint,GLsizei,const GLuint*)) \
    X(void, glUniform2f, (GLint,GLfloat,GLfloat)) \
    X(void, glUniform2i, (GLint,GLint,GLint)) \
    X(void, glUniform2ui, (GLint,GLuint,GLuint)) \
    X(void, glUniform2fv, (GLint,GLsizei,const GLfloat*)) \
    X(void, glUniform2iv, (GLint,GLsizei,const GLint*)) \
    X(void, glUniform2uiv, (GLint,GLsizei,const GLuint*)) \
    X(void, glUniform3f, (GLint,GLfloat,GLfloat,GLfloat)) \
    X(void, glUniform3i, (GLint,GLint,GLint,GLint)) \
    X(void, glUniform3ui, (GLint,GLuint,GLuint,GLuint)) \
    X(void, glUniform3fv, (GLint,GLsizei,const GLfloat*)) \
    X(void, glUniform3iv, (GLint,GLsizei,const GLint*)) \
    X(void, glUniform3uiv, (GLint,GLsizei,const GLuint*)) \
    X(void, glUniform4f, (GLint,GLfloat,GLfloat,GLfloat,GLfloat)) \
    X(void, glUniform4i, (GLint,GLint,GLint,GLint,GLint)) \
    X(void, glUniform4ui, (GLint,GLuint,GLuint,GLuint,GLuint)) \
    X(void, glUniform4fv, (GLint,GLsizei,const GLfloat*)) \
    X(void, glUniform4iv, (GLint,GLsizei,const GLint*)) \
    X(void, glUniform4uiv, (GLint,GLsizei,const GLuint*)) \
    X(void, glUniformMatrix2fv, (GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glUniformMatrix3fv, (GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glUniformMatrix4fv, (GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glUniformMatrix2x3fv, (GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glUniformMatrix3x2fv, (GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glUniformMatrix2x4fv, (GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glUniformMatrix4x2fv, (GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glUniformMatrix3x4fv, (GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glUniformMatrix4x3fv, (GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glGetUniformfv, (GLuint,GLint,GLfloat*)) \
    X(void, glGetUniformiv, (GLuint,GLint,GLint*)) \
    X(void, glGetUniformuiv, (GLuint,GLint,GLuint*)) \
    X(void, glGetActiveUniform, (GLuint,GLuint,GLsizei,GLsizei*,GLint*,GLenum*,GLchar*)) \
    X(void, glGetActiveAttrib, (GLuint,GLuint,GLsizei,GLsizei*,GLint*,GLenum*,GLchar*)) \
    X(GLint, glGetFragDataLocation, (GLuint,const GLchar*)) \
    X(void, glValidateProgram, (GLuint)) \
    X(void, glVertexAttrib4f, (GLuint,GLfloat,GLfloat,GLfloat,GLfloat)) \
    X(void, glVertexAttribI4i, (GLuint,GLint,GLint,GLint,GLint)) \
    X(void, glVertexAttribI4ui, (GLuint,GLuint,GLuint,GLuint,GLuint)) \
    X(void, glGetVertexAttribiv, (GLuint,GLenum,GLint*)) \
    X(void, glGetVertexAttribfv, (GLuint,GLenum,GLfloat*)) \
    X(void, glGetVertexAttribIiv, (GLuint,GLenum,GLint*)) \
    X(void, glGetVertexAttribIuiv, (GLuint,GLenum,GLuint*)) \
    X(void, glGetVertexAttribPointerv, (GLuint,GLenum,void**)) \
    X(void, glTexImage2D, (GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*)) \
    X(void, glTexImage3D, (GLenum,GLint,GLint,GLsizei,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*)) \
    X(void, glTexSubImage2D, (GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,const void*)) \
    X(void, glTexSubImage3D, (GLenum,GLint,GLint,GLint,GLint,GLsizei,GLsizei,GLsizei,GLenum,GLenum,const void*)) \
    X(void, glCopyTexImage2D, (GLenum,GLint,GLenum,GLint,GLint,GLsizei,GLsizei,GLint)) \
    X(void, glCopyTexSubImage2D, (GLenum,GLint,GLint,GLint,GLint,GLint,GLsizei,GLsizei)) \
    X(void, glCopyTexSubImage3D, (GLenum,GLint,GLint,GLint,GLint,GLint,GLint,GLsizei,GLsizei)) \
    X(void, glCompressedTexImage2D, (GLenum,GLint,GLenum,GLsizei,GLsizei,GLint,GLsizei,const void*)) \
    X(void, glCompressedTexImage3D, (GLenum,GLint,GLenum,GLsizei,GLsizei,GLsizei,GLint,GLsizei,const void*)) \
    X(void, glCompressedTexSubImage2D, (GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLsizei,const void*)) \
    X(void, glCompressedTexSubImage3D, (GLenum,GLint,GLint,GLint,GLint,GLsizei,GLsizei,GLsizei,GLenum,GLsizei,const void*)) \
    X(void, glGenerateMipmap, (GLenum)) \
    X(void, glPixelStorei, (GLenum,GLint)) \
    X(void, glTexParameterf, (GLenum,GLenum,GLfloat)) \
    X(void, glTexParameterfv, (GLenum,GLenum,const GLfloat*)) \
    X(void, glTexParameteriv, (GLenum,GLenum,const GLint*)) \
    X(void, glGetTexParameteriv, (GLenum,GLenum,GLint*)) \
    X(void, glGetTexParameterfv, (GLenum,GLenum,GLfloat*)) \
    X(GLboolean, glIsTexture, (GLuint)) \
    X(void, glGenFramebuffers, (GLsizei,GLuint*)) \
    X(void, glDeleteFramebuffers, (GLsizei,const GLuint*)) \
    X(void, glBindFramebuffer, (GLenum,GLuint)) \
    X(GLboolean, glIsFramebuffer, (GLuint)) \
    X(GLenum, glCheckFramebufferStatus, (GLenum)) \
    X(void, glFramebufferTexture2D, (GLenum,GLenum,GLenum,GLuint,GLint)) \
    X(void, glFramebufferTextureLayer, (GLenum,GLenum,GLuint,GLint,GLint)) \
    X(void, glFramebufferRenderbuffer, (GLenum,GLenum,GLenum,GLuint)) \
    X(void, glGetFramebufferAttachmentParameteriv, (GLenum,GLenum,GLenum,GLint*)) \
    X(void, glGenRenderbuffers, (GLsizei,GLuint*)) \
    X(void, glDeleteRenderbuffers, (GLsizei,const GLuint*)) \
    X(void, glBindRenderbuffer, (GLenum,GLuint)) \
    X(GLboolean, glIsRenderbuffer, (GLuint)) \
    X(void, glRenderbufferStorage, (GLenum,GLenum,GLsizei,GLsizei)) \
    X(void, glRenderbufferStorageMultisample, (GLenum,GLsizei,GLenum,GLsizei,GLsizei)) \
    X(void, glGetRenderbufferParameteriv, (GLenum,GLenum,GLint*)) \
    X(void, glBlitFramebuffer, (GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum)) \
    X(void, glDrawBuffers, (GLsizei,const GLenum*)) \
    X(void, glReadBuffer, (GLenum)) \
    X(void, glReadPixels, (GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void*)) \
    X(void, glClearBufferiv, (GLenum,GLint,const GLint*)) \
    X(void, glClearBufferuiv, (GLenum,GLint,const GLuint*)) \
    X(void, glClearBufferfv, (GLenum,GLint,const GLfloat*)) \
    X(void, glClearBufferfi, (GLenum,GLint,GLfloat,GLint)) \
    X(void, glClear, (GLbitfield)) \
    X(void, glClearColor, (GLfloat,GLfloat,GLfloat,GLfloat)) \
    X(void, glClearDepthf, (GLfloat)) \
    X(void, glClearStencil, (GLint)) \
    X(void, glViewport, (GLint,GLint,GLsizei,GLsizei)) \
    X(void, glScissor, (GLint,GLint,GLsizei,GLsizei)) \
    X(void, glDepthRangef, (GLfloat,GLfloat)) \
    X(void, glDepthFunc, (GLenum)) \
    X(void, glDepthMask, (GLboolean)) \
    X(void, glBlendFunc, (GLenum,GLenum)) \
    X(void, glBlendFuncSeparate, (GLenum,GLenum,GLenum,GLenum)) \
    X(void, glBlendEquation, (GLenum)) \
    X(void, glBlendEquationSeparate, (GLenum,GLenum)) \
    X(void, glBlendColor, (GLfloat,GLfloat,GLfloat,GLfloat)) \
    X(void, glStencilFunc, (GLenum,GLint,GLuint)) \
    X(void, glStencilFuncSeparate, (GLenum,GLenum,GLint,GLuint)) \
    X(void, glStencilOp, (GLenum,GLenum,GLenum)) \
    X(void, glStencilOpSeparate, (GLenum,GLenum,GLenum,GLenum)) \
    X(void, glStencilMask, (GLuint)) \
    X(void, glStencilMaskSeparate, (GLenum,GLuint)) \
    X(void, glCullFace, (GLenum)) \
    X(void, glFrontFace, (GLenum)) \
    X(void, glColorMask, (GLboolean,GLboolean,GLboolean,GLboolean)) \
    X(void, glPolygonOffset, (GLfloat,GLfloat)) \
    X(void, glLineWidth, (GLfloat)) \
    X(void, glHint, (GLenum,GLenum)) \
    X(void, glSampleCoverage, (GLfloat,GLboolean)) \
    X(void, glFlush, (void)) \
    X(void, glFinish, (void)) \
    X(void, glGetBooleanv, (GLenum,GLboolean*)) \
    X(void, glGetFloatv, (GLenum,GLfloat*)) \
    X(void, glBeginTransformFeedback, (GLenum)) \
    X(void, glEndTransformFeedback, (void)) \
    X(void, glTransformFeedbackVaryings, (GLuint,GLsizei,const GLchar* const*,GLenum)) \
    X(void, glGetTransformFeedbackVarying, (GLuint,GLuint,GLsizei,GLsizei*,GLsizei*,GLenum*,GLchar*)) \
    X(void, glFlushMappedBufferRange, (GLenum,GLintptr,GLsizeiptr)) \
    X(void, glGetBufferPointerv, (GLenum,GLenum,void**))

/* Funciones OPCIONALES del backend: no existen en GLES 3.0 base. Se cargan en
 * gl31_init() solo si la capacidad correspondiente (gl31_caps) esta presente,
 * probando name, nameEXT, nameOES y nameKHR. Si no estan, el miembro es NULL. */
#define GL31_BACKEND_OPT_FUNCS(X) \
    X(void, glEnablei, (GLenum, GLuint)) \
    X(void, glDisablei, (GLenum, GLuint)) \
    X(GLboolean, glIsEnabledi, (GLenum, GLuint)) \
    X(void, glColorMaski, (GLuint, GLboolean, GLboolean, GLboolean, GLboolean)) \
    X(void, glTexBuffer, (GLenum, GLenum, GLuint)) \
    X(void, glTexBufferRange, (GLenum, GLenum, GLuint, GLintptr, GLsizeiptr)) \
    X(void, glTexParameterIiv, (GLenum, GLenum, const GLint*)) \
    X(void, glTexParameterIuiv, (GLenum, GLenum, const GLuint*)) \
    X(void, glGetTexParameterIiv, (GLenum, GLenum, GLint*)) \
    X(void, glGetTexParameterIuiv, (GLenum, GLenum, GLuint*)) \
    X(void, glSamplerParameterIiv, (GLuint, GLenum, const GLint*)) \
    X(void, glSamplerParameterIuiv, (GLuint, GLenum, const GLuint*)) \
    X(void, glGetTexLevelParameteriv, (GLenum, GLint, GLenum, GLint*))

/* capacidades del backend (detectadas en init) */
typedef struct {
    int es_major, es_minor;     /* versión GLES */
    int has_pvrtc, has_s3tc;    /* compresión */
    int has_astc, has_bptc;
    int has_basis;
    int tex_buffer;             /* glTexBuffer / samplerBuffer */
    int draw_instanced;         /* EXT_draw_instanced (en ES 3.0 el instancing ya es core) */
    int draw_buf_indexed;       /* ES 3.2 o EXT/OES_draw_buffers_indexed (Enablei, ColorMaski...) */
    int half_float;             /* EXT_color_buffer_half_float */
    int float_buffer;           /* EXT_color_buffer_float */
    int border_clamp;           /* ES 3.2 o EXT/OES_texture_border_clamp */
    int aniso;                  /* EXT_texture_filter_anisotropic */
    int level_query;            /* ES 3.1+: glGetTexLevelParameter nativo */
} gl31_caps_t;

extern gl31_caps_t gl31_caps;

typedef struct {
#define X(ret, name, args) ret (*name) args;
    GL31_BACKEND_FUNCS(X)
    GL31_BACKEND_OPT_FUNCS(X)
#undef X
} gl31_backend_t;

/* Unica instancia del backend; definida en gl31_caps.c */
extern gl31_backend_t gl31_be;


#define BE(fn) (gl31_be.fn)

/* ---------- estado GL 3.1 por contexto/hilo ---------- */
typedef struct {
    GLuint     buffer;
    GLintptr   offset;
    GLsizeiptr size;   /* 0 = buffer completo (BindBufferBase) */
} gl31_ubo_binding_t;

#define GL31_MAX_SOFTCAPS 32      /* caps de desktop servidas por estado propio (gl31_render.c) */
#define GL31_MAX_DRAW_BUFFERS 8

typedef struct {
    int     inited;                /* estado por hilo inicializado (defaults aplicados) */
    GLenum  error;
    GLuint  vao;
    GLuint  program;
    GLuint  array_buffer;
    GLuint  uniform_buffer;
    GLuint  copy_read_buffer;
    GLuint  copy_write_buffer;
    GLuint  pixel_pack_buffer;
    GLuint  pixel_unpack_buffer;
    GLuint  draw_fbo;
    GLuint  read_fbo;
    GLuint  renderbuffer;
    gl31_ubo_binding_t ubo[GL31_MAX_UBO_BINDINGS];
    GLuint  sampler[GL31_MAX_TEX_UNITS];
    GLboolean prim_restart;
    GLuint    prim_restart_index;
    GLboolean restart_fixed_applied;
    int     xfb_active;            /* transform feedback en curso */
    /* estado de rasterizacion/fragmento que ES no tiene (gl31_render.c) */
    GLboolean softcap[GL31_MAX_SOFTCAPS];
    GLboolean colormask[GL31_MAX_DRAW_BUFFERS][4];
    GLfloat   point_size;
    GLfloat   point_fade_threshold;
    GLenum    point_sprite_origin;
    /* GL_PACK_* (glPixelStorei); lo usa glReadPixels */
    GLint     pack_alignment, pack_row_length, pack_skip_rows, pack_skip_pixels;
    int     limits_loaded;
    GLint   ubo_alignment;
    GLint   max_ubo_bindings;
    GLint   max_tex_units;
} gl31_state_t;

/* gl31.c */
typedef void* (*gl31_loader_fn)(const char* name);
int   gl31_init(gl31_loader_fn loader);          /* 0 = OK */
void  gl31_shutdown(void);
void* gl31_get_proc_address(const char* name);   /* NULL si no es de GL31 */

/* gl31_state.c */
gl31_state_t* gl31_state(void);
void   gl31_state_init(void);
void   gl31_state_load_limits(void);
void   gl31_set_error(GLenum e);
GLenum gl31_glGetError(void);
void   gl31_glEnable(GLenum cap);
void   gl31_glDisable(GLenum cap);
GLboolean gl31_glIsEnabled(GLenum cap);
void   gl31_glPrimitiveRestartIndex(GLuint index);
void   gl31_glGenSamplers(GLsizei n, GLuint* s);
void   gl31_glDeleteSamplers(GLsizei n, const GLuint* s);
void   gl31_glBindSampler(GLuint unit, GLuint sampler);
void   gl31_glSamplerParameteri(GLuint s, GLenum pname, GLint v);
void   gl31_glSamplerParameterf(GLuint s, GLenum pname, GLfloat v);
GLboolean gl31_glIsSampler(GLuint s);
void   gl31_glGetSamplerParameteriv(GLuint s, GLenum pname, GLint* v);

/* gl31_vao.c */
void gl31_glGenVertexArrays(GLsizei n, GLuint* arrays);
void gl31_glDeleteVertexArrays(GLsizei n, const GLuint* arrays);
void gl31_glBindVertexArray(GLuint array);
GLboolean gl31_glIsVertexArray(GLuint array);
void gl31_glVertexAttribPointer(GLuint idx, GLint size, GLenum type, GLboolean norm, GLsizei stride, const void* ptr);
void gl31_glVertexAttribIPointer(GLuint idx, GLint size, GLenum type, GLsizei stride, const void* ptr);
void gl31_glEnableVertexAttribArray(GLuint idx);
void gl31_glDisableVertexAttribArray(GLuint idx);
void gl31_glVertexAttribDivisor(GLuint idx, GLuint divisor);

/* gl31_buffer.c */
void gl31_glGenBuffers(GLsizei n, GLuint* buffers);
void gl31_glDeleteBuffers(GLsizei n, const GLuint* buffers);
void gl31_glBindBuffer(GLenum target, GLuint buffer);
void gl31_glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
void gl31_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
void gl31_glBufferStorage(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags);
void gl31_glBindBufferBase(GLenum target, GLuint index, GLuint buffer);
void gl31_glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
void gl31_glCopyBufferSubData(GLenum rt, GLenum wt, GLintptr ro, GLintptr wo, GLsizeiptr size);
void* gl31_glMapBuffer(GLenum target, GLenum access);
void* gl31_glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
GLboolean gl31_glUnmapBuffer(GLenum target);
void gl31_glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params);
GLboolean gl31_glIsBuffer(GLuint buffer);

/* gl31_texture.c */
void gl31_glGenTextures(GLsizei n, GLuint* t);
void gl31_glDeleteTextures(GLsizei n, const GLuint* t);
void gl31_glBindTexture(GLenum target, GLuint texture);
void gl31_glActiveTexture(GLenum unit);
void gl31_glTexParameteri(GLenum target, GLenum pname, GLint param);
void gl31_glTexStorage2D(GLenum target, GLsizei levels, GLenum ifmt, GLsizei w, GLsizei h);
void gl31_glTexStorage3D(GLenum target, GLsizei levels, GLenum ifmt, GLsizei w, GLsizei h, GLsizei d);

/* gl31_draw.c */
void gl31_glDrawArrays(GLenum mode, GLint first, GLsizei count);
void gl31_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);
void gl31_glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei inst);
void gl31_glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei inst);
void gl31_glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void* indices);
void gl31_glMultiDrawArrays(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount);
void gl31_glMultiDrawElements(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices, GLsizei drawcount);

/* gl31_uniform.c */
GLuint gl31_glGetUniformBlockIndex(GLuint program, const GLchar* name);
void gl31_glUniformBlockBinding(GLuint program, GLuint block, GLuint binding);
void gl31_glGetActiveUniformBlockiv(GLuint program, GLuint block, GLenum pname, GLint* params);
void gl31_glGetActiveUniformBlockName(GLuint program, GLuint block, GLsizei bufSize, GLsizei* length, GLchar* name);
void gl31_glGetUniformIndices(GLuint program, GLsizei count, const GLchar* const* names, GLuint* indices);
void gl31_glGetActiveUniformsiv(GLuint program, GLsizei count, const GLuint* indices, GLenum pname, GLint* params);
void gl31_glGetIntegeri_v(GLenum target, GLuint index, GLint* data);

/* gl31_query.c */
void gl31_glGenQueries(GLsizei n, GLuint* ids);
void gl31_glDeleteQueries(GLsizei n, const GLuint* ids);
GLboolean gl31_glIsQuery(GLuint id);
void gl31_glBeginQuery(GLenum target, GLuint id);
void gl31_glEndQuery(GLenum target);
void gl31_glGetQueryiv(GLenum target, GLenum pname, GLint* params);
void gl31_glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params);
void gl31_glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params);
const GLubyte* gl31_glGetString(GLenum name);
const GLubyte* gl31_glGetStringi(GLenum name, GLuint index);
void gl31_glGetIntegerv(GLenum pname, GLint* data);

/* gl31_shader_glsl.c: GLSL 1.30/1.40/1.50/3.30 -> GLSL ES 3.00.
 * Devuelve memoria malloc'd (free) o NULL y rellena err. */
char* gl31_glsl_convert(const char* src, GLenum shader_type, char* err, size_t errlen);

/* gl31_shader_link.c */
void   gl31_link_shutdown(void);
GLuint gl31_glCreateShader(GLenum type);
void   gl31_glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
void   gl31_glCompileShader(GLuint shader);
void   gl31_glGetShaderiv(GLuint shader, GLenum pname, GLint* params);
void   gl31_glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* log);
void   gl31_glDeleteShader(GLuint shader);
GLuint gl31_glCreateProgram(void);
void   gl31_glAttachShader(GLuint program, GLuint shader);
void   gl31_glDetachShader(GLuint program, GLuint shader);
void   gl31_glLinkProgram(GLuint program);
void   gl31_glGetProgramiv(GLuint program, GLenum pname, GLint* params);
void   gl31_glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* log);
void   gl31_glUseProgram(GLuint program);
void   gl31_glDeleteProgram(GLuint program);
void   gl31_glBindAttribLocation(GLuint program, GLuint index, const GLchar* name);
GLint  gl31_glGetAttribLocation(GLuint program, const GLchar* name);
GLint  gl31_glGetUniformLocation(GLuint program, const GLchar* name);

/* gl31_stubs.c */
void gl31_stub_warn(const char* fn);
void gl31_glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer);
void gl31_glBindFragDataLocation(GLuint program, GLuint color, const GLchar* name);
void gl31_glTexImage1D(GLenum target, GLint level, GLint ifmt, GLsizei w, GLint border, GLenum format, GLenum type, const void* pixels);
void gl31_glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels);
void gl31_glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void* data);
void gl31_glPolygonMode(GLenum face, GLenum mode);
void gl31_glClampColor(GLenum target, GLenum clamp);

/* gl31_uniform.c: glUniform* / consultas de uniforms (ademas de los UBO de arriba) */
void gl31_glUniform1f(GLint location, GLfloat v0);
void gl31_glUniform1i(GLint location, GLint v0);
void gl31_glUniform1ui(GLint location, GLuint v0);
void gl31_glUniform2f(GLint location, GLfloat v0, GLfloat v1);
void gl31_glUniform2i(GLint location, GLint v0, GLint v1);
void gl31_glUniform2ui(GLint location, GLuint v0, GLuint v1);
void gl31_glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void gl31_glUniform3i(GLint location, GLint v0, GLint v1, GLint v2);
void gl31_glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2);
void gl31_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void gl31_glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
void gl31_glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
void gl31_glUniform1fv(GLint location, GLsizei count, const GLfloat* value);
void gl31_glUniform1iv(GLint location, GLsizei count, const GLint* value);
void gl31_glUniform1uiv(GLint location, GLsizei count, const GLuint* value);
void gl31_glUniform2fv(GLint location, GLsizei count, const GLfloat* value);
void gl31_glUniform2iv(GLint location, GLsizei count, const GLint* value);
void gl31_glUniform2uiv(GLint location, GLsizei count, const GLuint* value);
void gl31_glUniform3fv(GLint location, GLsizei count, const GLfloat* value);
void gl31_glUniform3iv(GLint location, GLsizei count, const GLint* value);
void gl31_glUniform3uiv(GLint location, GLsizei count, const GLuint* value);
void gl31_glUniform4fv(GLint location, GLsizei count, const GLfloat* value);
void gl31_glUniform4iv(GLint location, GLsizei count, const GLint* value);
void gl31_glUniform4uiv(GLint location, GLsizei count, const GLuint* value);
void gl31_glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glGetUniformfv(GLuint program, GLint location, GLfloat* params);
void gl31_glGetUniformiv(GLuint program, GLint location, GLint* params);
void gl31_glGetUniformuiv(GLuint program, GLint location, GLuint* params);
void gl31_glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
void gl31_glGetActiveUniformName(GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei* length, GLchar* name);
void gl31_glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
GLint gl31_glGetFragDataLocation(GLuint program, const GLchar* name);

/* gl31_attrib.c: atributos genericos */
void gl31_glVertexAttrib1f(GLuint index, GLfloat v0);
void gl31_glVertexAttrib1s(GLuint index, GLshort v0);
void gl31_glVertexAttrib1d(GLuint index, GLdouble v0);
void gl31_glVertexAttrib1fv(GLuint index, const GLfloat* v);
void gl31_glVertexAttrib1sv(GLuint index, const GLshort* v);
void gl31_glVertexAttrib1dv(GLuint index, const GLdouble* v);
void gl31_glVertexAttrib2f(GLuint index, GLfloat v0, GLfloat v1);
void gl31_glVertexAttrib2s(GLuint index, GLshort v0, GLshort v1);
void gl31_glVertexAttrib2d(GLuint index, GLdouble v0, GLdouble v1);
void gl31_glVertexAttrib2fv(GLuint index, const GLfloat* v);
void gl31_glVertexAttrib2sv(GLuint index, const GLshort* v);
void gl31_glVertexAttrib2dv(GLuint index, const GLdouble* v);
void gl31_glVertexAttrib3f(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2);
void gl31_glVertexAttrib3s(GLuint index, GLshort v0, GLshort v1, GLshort v2);
void gl31_glVertexAttrib3d(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2);
void gl31_glVertexAttrib3fv(GLuint index, const GLfloat* v);
void gl31_glVertexAttrib3sv(GLuint index, const GLshort* v);
void gl31_glVertexAttrib3dv(GLuint index, const GLdouble* v);
void gl31_glVertexAttrib4f(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void gl31_glVertexAttrib4s(GLuint index, GLshort v0, GLshort v1, GLshort v2, GLshort v3);
void gl31_glVertexAttrib4d(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3);
void gl31_glVertexAttrib4fv(GLuint index, const GLfloat* v);
void gl31_glVertexAttrib4sv(GLuint index, const GLshort* v);
void gl31_glVertexAttrib4dv(GLuint index, const GLdouble* v);
void gl31_glVertexAttrib4Nub(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
void gl31_glVertexAttrib4Nbv(GLuint index, const GLbyte* v);
void gl31_glVertexAttrib4Nsv(GLuint index, const GLshort* v);
void gl31_glVertexAttrib4Nubv(GLuint index, const GLubyte* v);
void gl31_glVertexAttrib4Nusv(GLuint index, const GLushort* v);
void gl31_glVertexAttrib4Nuiv(GLuint index, const GLuint* v);
void gl31_glVertexAttrib4bv(GLuint index, const GLbyte* v);
void gl31_glVertexAttrib4iv(GLuint index, const GLint* v);
void gl31_glVertexAttrib4ubv(GLuint index, const GLubyte* v);
void gl31_glVertexAttrib4uiv(GLuint index, const GLuint* v);
void gl31_glVertexAttrib4usv(GLuint index, const GLushort* v);
void gl31_glVertexAttribI1i(GLuint index, GLint v0);
void gl31_glVertexAttribI1ui(GLuint index, GLuint v0);
void gl31_glVertexAttribI1iv(GLuint index, const GLint* v);
void gl31_glVertexAttribI1uiv(GLuint index, const GLuint* v);
void gl31_glVertexAttribI2i(GLuint index, GLint v0, GLint v1);
void gl31_glVertexAttribI2ui(GLuint index, GLuint v0, GLuint v1);
void gl31_glVertexAttribI2iv(GLuint index, const GLint* v);
void gl31_glVertexAttribI2uiv(GLuint index, const GLuint* v);
void gl31_glVertexAttribI3i(GLuint index, GLint v0, GLint v1, GLint v2);
void gl31_glVertexAttribI3ui(GLuint index, GLuint v0, GLuint v1, GLuint v2);
void gl31_glVertexAttribI3iv(GLuint index, const GLint* v);
void gl31_glVertexAttribI3uiv(GLuint index, const GLuint* v);
void gl31_glVertexAttribI4i(GLuint index, GLint v0, GLint v1, GLint v2, GLint v3);
void gl31_glVertexAttribI4ui(GLuint index, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
void gl31_glVertexAttribI4iv(GLuint index, const GLint* v);
void gl31_glVertexAttribI4uiv(GLuint index, const GLuint* v);
void gl31_glVertexAttribI4bv(GLuint index, const GLbyte* v);
void gl31_glVertexAttribI4sv(GLuint index, const GLshort* v);
void gl31_glVertexAttribI4ubv(GLuint index, const GLubyte* v);
void gl31_glVertexAttribI4usv(GLuint index, const GLushort* v);
void gl31_glGetVertexAttribiv(GLuint index, GLenum pname, GLint* params);
void gl31_glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params);
void gl31_glGetVertexAttribdv(GLuint index, GLenum pname, GLdouble* params);
void gl31_glGetVertexAttribIiv(GLuint index, GLenum pname, GLint* params);
void gl31_glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint* params);
void gl31_glGetVertexAttribPointerv(GLuint index, GLenum pname, void** pointer);

/* gl31_buffer.c (adicionales) */
void gl31_glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length);
void gl31_glGetBufferPointerv(GLenum target, GLenum pname, void** params);

/* gl31_texture.c (adicionales) */
void gl31_glTexImage2D(GLenum target, GLint level, GLint ifmt, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const void* pixels);
void gl31_glTexImage3D(GLenum target, GLint level, GLint ifmt, GLsizei w, GLsizei h, GLsizei d, GLint border, GLenum format, GLenum type, const void* pixels);
void gl31_glTexSubImage2D(GLenum target, GLint level, GLint xo, GLint yo, GLsizei w, GLsizei h, GLenum format, GLenum type, const void* pixels);
void gl31_glTexSubImage3D(GLenum target, GLint level, GLint xo, GLint yo, GLint zo, GLsizei w, GLsizei h, GLsizei d, GLenum format, GLenum type, const void* pixels);
void gl31_glCopyTexImage2D(GLenum target, GLint level, GLenum ifmt, GLint x, GLint y, GLsizei w, GLsizei h, GLint border);
void gl31_glCopyTexSubImage2D(GLenum target, GLint level, GLint xo, GLint yo, GLint x, GLint y, GLsizei w, GLsizei h);
void gl31_glCopyTexSubImage3D(GLenum target, GLint level, GLint xo, GLint yo, GLint zo, GLint x, GLint y, GLsizei w, GLsizei h);
void gl31_glCompressedTexImage2D(GLenum target, GLint level, GLenum ifmt, GLsizei w, GLsizei h, GLint border, GLsizei size, const void* data);
void gl31_glCompressedTexImage3D(GLenum target, GLint level, GLenum ifmt, GLsizei w, GLsizei h, GLsizei d, GLint border, GLsizei size, const void* data);
void gl31_glCompressedTexSubImage2D(GLenum target, GLint level, GLint xo, GLint yo, GLsizei w, GLsizei h, GLenum format, GLsizei size, const void* data);
void gl31_glCompressedTexSubImage3D(GLenum target, GLint level, GLint xo, GLint yo, GLint zo, GLsizei w, GLsizei h, GLsizei d, GLenum format, GLsizei size, const void* data);
void gl31_glGenerateMipmap(GLenum target);
void gl31_glPixelStorei(GLenum pname, GLint param);
void gl31_glPixelStoref(GLenum pname, GLfloat param);
void gl31_glTexParameterf(GLenum target, GLenum pname, GLfloat param);
void gl31_glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params);
void gl31_glTexParameteriv(GLenum target, GLenum pname, const GLint* params);
void gl31_glTexParameterIiv(GLenum target, GLenum pname, const GLint* params);
void gl31_glTexParameterIuiv(GLenum target, GLenum pname, const GLuint* params);
void gl31_glGetTexParameteriv(GLenum target, GLenum pname, GLint* params);
void gl31_glGetTexParameterfv(GLenum target, GLenum pname, GLfloat* params);
void gl31_glGetTexParameterIiv(GLenum target, GLenum pname, GLint* params);
void gl31_glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint* params);
void gl31_glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params);
void gl31_glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat* params);
GLboolean gl31_glIsTexture(GLuint texture);

/* gl31_fbo.c: framebuffers, renderbuffers, lectura y clear por buffer */
void gl31_glGenFramebuffers(GLsizei n, GLuint* ids);
void gl31_glDeleteFramebuffers(GLsizei n, const GLuint* ids);
void gl31_glBindFramebuffer(GLenum target, GLuint fbo);
GLboolean gl31_glIsFramebuffer(GLuint fbo);
GLenum gl31_glCheckFramebufferStatus(GLenum target);
void gl31_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
void gl31_glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
void gl31_glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
void gl31_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum rbtarget, GLuint rb);
void gl31_glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params);
void gl31_glGenRenderbuffers(GLsizei n, GLuint* ids);
void gl31_glDeleteRenderbuffers(GLsizei n, const GLuint* ids);
void gl31_glBindRenderbuffer(GLenum target, GLuint rb);
GLboolean gl31_glIsRenderbuffer(GLuint rb);
void gl31_glRenderbufferStorage(GLenum target, GLenum ifmt, GLsizei w, GLsizei h);
void gl31_glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum ifmt, GLsizei w, GLsizei h);
void gl31_glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params);
void gl31_glBlitFramebuffer(GLint sx0, GLint sy0, GLint sx1, GLint sy1, GLint dx0, GLint dy0, GLint dx1, GLint dy1, GLbitfield mask, GLenum filter);
void gl31_glDrawBuffer(GLenum mode);
void gl31_glDrawBuffers(GLsizei n, const GLenum* bufs);
void gl31_glReadBuffer(GLenum mode);
void gl31_glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void* pixels);
void gl31_glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value);
void gl31_glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint* value);
void gl31_glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value);
void gl31_glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);

/* gl31_render.c: clear, viewport y estado de rasterizacion/fragmento */
void gl31_glClear(GLbitfield mask);
void gl31_glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void gl31_glClearDepth(GLdouble depth);
void gl31_glClearStencil(GLint s);
void gl31_glViewport(GLint x, GLint y, GLsizei w, GLsizei h);
void gl31_glScissor(GLint x, GLint y, GLsizei w, GLsizei h);
void gl31_glDepthRange(GLdouble n, GLdouble f);
void gl31_glDepthFunc(GLenum func);
void gl31_glDepthMask(GLboolean flag);
void gl31_glBlendFunc(GLenum sfactor, GLenum dfactor);
void gl31_glBlendFuncSeparate(GLenum srgb, GLenum drgb, GLenum salpha, GLenum dalpha);
void gl31_glBlendEquation(GLenum mode);
void gl31_glBlendEquationSeparate(GLenum rgb, GLenum alpha);
void gl31_glBlendColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void gl31_glStencilFunc(GLenum func, GLint ref, GLuint mask);
void gl31_glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
void gl31_glStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass);
void gl31_glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
void gl31_glStencilMask(GLuint mask);
void gl31_glStencilMaskSeparate(GLenum face, GLuint mask);
void gl31_glCullFace(GLenum mode);
void gl31_glFrontFace(GLenum mode);
void gl31_glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a);
void gl31_glColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a);
void gl31_glEnablei(GLenum cap, GLuint index);
void gl31_glDisablei(GLenum cap, GLuint index);
GLboolean gl31_glIsEnabledi(GLenum cap, GLuint index);
void gl31_glPolygonOffset(GLfloat factor, GLfloat units);
void gl31_glLineWidth(GLfloat width);
void gl31_glPointSize(GLfloat size);
void gl31_glHint(GLenum target, GLenum mode);
void gl31_glSampleCoverage(GLfloat value, GLboolean invert);
void gl31_glFlush(void);
void gl31_glFinish(void);

/* gl31_query.c (adicionales) */
void gl31_glGetBooleanv(GLenum pname, GLboolean* data);
void gl31_glGetFloatv(GLenum pname, GLfloat* data);
void gl31_glGetDoublev(GLenum pname, GLdouble* data);

/* gl31_shader_link.c (adicionales) */
GLboolean gl31_glIsShader(GLuint shader);
GLboolean gl31_glIsProgram(GLuint program);
void gl31_glValidateProgram(GLuint program);
void gl31_glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* source);
void gl31_glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders);
/* helper interno: -2 = es un shader, -1 = no existe, 0 = no linkeado, 1 = linkeado */
int  gl31_program_status(GLuint program);

/* gl31_xfb.c: transform feedback */
void gl31_glBeginTransformFeedback(GLenum primitiveMode);
void gl31_glEndTransformFeedback(void);
void gl31_glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar* const* varyings, GLenum bufferMode);
void gl31_glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLsizei* size, GLenum* type, GLchar* name);

/* gl31_stubs.c (adicionales) */
void gl31_glBeginConditionalRender(GLuint id, GLenum mode);
void gl31_glEndConditionalRender(void);

/* ---------- internos entre modulos (no son API GL) ---------- */
/* gl31_state.c: vuelca a gl31_set_error los errores pendientes del backend, para
 * que un error nuestro no tape uno anterior de la app. */
void gl31_err_flush(void);

/* gl31_caps.c */
void* gl31_get_optional(const char* name);       /* NULL si no esta cargada */

/* gl31_render.c */
void gl31_render_state_defaults(gl31_state_t* s);
int  gl31_soft_cap_get(GLenum cap, GLboolean* out);   /* 1 = servida desde estado propio */
void gl31_glGetBooleani_v(GLenum target, GLuint index, GLboolean* data);
void gl31_glPointParameterf(GLenum pname, GLfloat param);
void gl31_glPointParameteri(GLenum pname, GLint param);
void gl31_glPointParameterfv(GLenum pname, const GLfloat* params);
void gl31_glPointParameteriv(GLenum pname, const GLint* params);
void gl31_glLogicOp(GLenum op);

/* gl31_query.c: render condicional. 1 = el draw/clear actual debe omitirse. */
int  gl31_cond_skip(void);

/* gl31_texture.c */
GLenum gl31_tex_be_target(GLenum desktop_target);      /* 0 = no existe en el backend */
void   gl31_tex_sync_for_draw(void);                   /* llamar antes de cada draw */
int    gl31_tex_get_binding(GLenum pname, GLint* out); /* 1 = servido (TEXTURE_BINDING_*) */
void   gl31_glTexStorage1D(GLenum target, GLsizei levels, GLenum ifmt, GLsizei w);

/* gl31_fbo.c */
void gl31_glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
int  gl31_read_pixels_ex(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void* pixels);

/* gl31_shader_link.c: tipo de sampler tal como lo ve la app (hoy el conversor GLSL
 * solo admite samplers que existen en ES, asi que coincide con el tipo del backend). */
GLenum gl31_program_sampler_type(GLuint program, const char* uniform_name, GLenum backend_type);

#endif /* LORICA_GL31_H */
