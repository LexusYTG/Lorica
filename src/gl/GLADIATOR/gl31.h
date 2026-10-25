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
#include <GLES3/gl32.h>
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
#ifndef GL_TIMESTAMP
#define GL_TIMESTAMP 0x8E28
#endif
#ifndef GL_TEXTURE_SWIZZLE_RGBA
#define GL_TEXTURE_SWIZZLE_RGBA 0x8E46
#endif
#ifndef GL_MAX_DUAL_SOURCE_DRAW_BUFFERS
#define GL_MAX_DUAL_SOURCE_DRAW_BUFFERS 0x88FC
#endif
#ifndef GL_SRC1_COLOR
#define GL_SRC1_COLOR 0x88F9
#define GL_ONE_MINUS_SRC1_COLOR 0x88FA
#define GL_ONE_MINUS_SRC1_ALPHA 0x88FB
#define GL_SRC1_ALPHA 0x8589
#endif
#ifndef GL_INTERNALFORMAT_SUPPORTED
#define GL_INTERNALFORMAT_SUPPORTED 0x826F
#endif
#ifndef GL_PROXY_TEXTURE_CUBE_MAP_ARRAY
#define GL_PROXY_TEXTURE_CUBE_MAP_ARRAY 0x900B
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif
#ifndef GL_STENCIL_INDEX
#define GL_STENCIL_INDEX 0x1901
#endif

#ifndef GL_GEOMETRY_SHADER
#define GL_GEOMETRY_SHADER 0x8DD9
#define GL_LINES_ADJACENCY 0x000A
#define GL_LINE_STRIP_ADJACENCY 0x000B
#define GL_TRIANGLES_ADJACENCY 0x000C
#define GL_TRIANGLE_STRIP_ADJACENCY 0x000D
#define GL_FRAMEBUFFER_ATTACHMENT_LAYERED 0x8DA7
#endif
#ifndef GL_TEXTURE_2D_MULTISAMPLE
#define GL_TEXTURE_2D_MULTISAMPLE 0x9100
#define GL_TEXTURE_2D_MULTISAMPLE_ARRAY 0x9102
#define GL_TEXTURE_BINDING_2D_MULTISAMPLE 0x9104
#define GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY 0x9105
#define GL_TEXTURE_SAMPLES 0x9106
#define GL_TEXTURE_FIXED_SAMPLE_LOCATIONS 0x9107
#define GL_SAMPLE_POSITION 0x8E50
#define GL_SAMPLE_MASK 0x8E51
#define GL_SAMPLE_MASK_VALUE 0x8E52
#define GL_MAX_SAMPLE_MASK_WORDS 0x8E59
#endif
#ifndef GL_PROXY_TEXTURE_2D_MULTISAMPLE
#define GL_PROXY_TEXTURE_2D_MULTISAMPLE 0x9101
#define GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY 0x9103
#endif
#ifndef GL_FIRST_VERTEX_CONVENTION
#define GL_FIRST_VERTEX_CONVENTION 0x8E4D
#endif
#ifndef GL_LAST_VERTEX_CONVENTION
#define GL_LAST_VERTEX_CONVENTION 0x8E4E
#endif
#ifndef GL_PROVOKING_VERTEX
#define GL_PROVOKING_VERTEX 0x8E4F
#endif
#ifndef GL_DEPTH_CLAMP
#define GL_DEPTH_CLAMP 0x864F
#endif
#ifndef GL_TEXTURE_CUBE_MAP_SEAMLESS
#define GL_TEXTURE_CUBE_MAP_SEAMLESS 0x884F
#endif
#ifndef GL_CONTEXT_PROFILE_MASK
#define GL_CONTEXT_PROFILE_MASK 0x9126
#define GL_CONTEXT_CORE_PROFILE_BIT 0x00000001
#define GL_CONTEXT_COMPATIBILITY_PROFILE_BIT 0x00000002
#endif
#ifndef GL_CONTEXT_FLAGS
#define GL_CONTEXT_FLAGS 0x821E
#endif
#ifndef GL_MAX_CLIP_DISTANCES
#define GL_MAX_CLIP_DISTANCES 0x0D32
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
    X(GLsync, glFenceSync, (GLenum,GLbitfield)) \
    X(GLboolean, glIsSync, (GLsync)) \
    X(void, glDeleteSync, (GLsync)) \
    X(GLenum, glClientWaitSync, (GLsync,GLbitfield,GLuint64)) \
    X(void, glWaitSync, (GLsync,GLbitfield,GLuint64)) \
    X(void, glGetSynciv, (GLsync,GLenum,GLsizei,GLsizei*,GLint*)) \
    X(void, glGetInteger64v, (GLenum,GLint64*)) \
    X(void, glGetInteger64i_v, (GLenum,GLuint,GLint64*)) \
    X(void, glGetBufferParameteri64v, (GLenum,GLenum,GLint64*)) \
    X(void, glSamplerParameteriv, (GLuint,GLenum,const GLint*)) \
    X(void, glSamplerParameterfv, (GLuint,GLenum,const GLfloat*)) \
    X(void, glGetSamplerParameterfv, (GLuint,GLenum,GLfloat*)) \
    X(void, glGetBufferPointerv, (GLenum,GLenum,void**)) \
    X(void, glGetProgramBinary, (GLuint,GLsizei,GLsizei*,GLenum*,void*)) \
    X(void, glProgramBinary, (GLuint,GLenum,const void*,GLsizei)) \
    X(void, glProgramParameteri, (GLuint,GLenum,GLint)) \
    X(void, glGenTransformFeedbacks, (GLsizei,GLuint*)) \
    X(void, glDeleteTransformFeedbacks, (GLsizei,const GLuint*)) \
    X(void, glBindTransformFeedback, (GLenum,GLuint)) \
    X(GLboolean, glIsTransformFeedback, (GLuint)) \
    X(void, glPauseTransformFeedback, (void)) \
    X(void, glResumeTransformFeedback, (void)) \
    X(void, glInvalidateFramebuffer, (GLenum,GLsizei,const GLenum*)) \
    X(void, glInvalidateSubFramebuffer, (GLenum,GLsizei,const GLenum*,GLint,GLint,GLsizei,GLsizei)) \
    X(void, glGetInternalformativ, (GLenum,GLenum,GLenum,GLsizei,GLint*)) \
    X(void, glGetShaderPrecisionFormat, (GLenum,GLenum,GLint*,GLint*)) \
    X(void, glReleaseShaderCompiler, (void))

/* Funciones de ES 3.1 que GL 4.x necesita: se cargan juntas si caps.es31 */
#define GL31_ES31_FUNCS(X) \
    X(void, glDispatchCompute, (GLuint,GLuint,GLuint)) \
    X(void, glDispatchComputeIndirect, (GLintptr)) \
    X(void, glMemoryBarrier, (GLbitfield)) \
    X(void, glMemoryBarrierByRegion, (GLbitfield)) \
    X(void, glBindImageTexture, (GLuint,GLuint,GLint,GLboolean,GLint,GLenum,GLenum)) \
    X(void, glDrawArraysIndirect, (GLenum,const void*)) \
    X(void, glDrawElementsIndirect, (GLenum,GLenum,const void*)) \
    X(void, glFramebufferParameteri, (GLenum,GLenum,GLint)) \
    X(void, glGetFramebufferParameteriv, (GLenum,GLenum,GLint*)) \
    X(void, glGetProgramInterfaceiv, (GLuint,GLenum,GLenum,GLint*)) \
    X(GLuint, glGetProgramResourceIndex, (GLuint,GLenum,const GLchar*)) \
    X(void, glGetProgramResourceName, (GLuint,GLenum,GLuint,GLsizei,GLsizei*,GLchar*)) \
    X(void, glGetProgramResourceiv, (GLuint,GLenum,GLuint,GLsizei,const GLenum*,GLsizei,GLsizei*,GLint*)) \
    X(GLint, glGetProgramResourceLocation, (GLuint,GLenum,const GLchar*)) \
    X(void, glVertexAttribFormat, (GLuint,GLint,GLenum,GLboolean,GLuint)) \
    X(void, glVertexAttribIFormat, (GLuint,GLint,GLenum,GLuint)) \
    X(void, glVertexAttribBinding, (GLuint,GLuint)) \
    X(void, glBindVertexBuffer, (GLuint,GLuint,GLintptr,GLsizei)) \
    X(void, glVertexBindingDivisor, (GLuint,GLuint)) \
    X(GLuint, glCreateShaderProgramv, (GLenum,GLsizei,const GLchar* const*)) \
    X(void, glUseProgramStages, (GLuint,GLbitfield,GLuint)) \
    X(void, glBindProgramPipeline, (GLuint)) \
    X(void, glGenProgramPipelines, (GLsizei,GLuint*)) \
    X(void, glDeleteProgramPipelines, (GLsizei,const GLuint*)) \
    X(GLboolean, glIsProgramPipeline, (GLuint)) \
    X(void, glValidateProgramPipeline, (GLuint)) \
    X(void, glGetProgramPipelineiv, (GLuint,GLenum,GLint*)) \
    X(void, glGetProgramPipelineInfoLog, (GLuint,GLsizei,GLsizei*,GLchar*)) \
    X(void, glActiveShaderProgram, (GLuint,GLuint)) \
    X(void, glProgramUniform1f, (GLuint,GLint,GLfloat)) \
    X(void, glProgramUniform2f, (GLuint,GLint,GLfloat,GLfloat)) \
    X(void, glProgramUniform3f, (GLuint,GLint,GLfloat,GLfloat,GLfloat)) \
    X(void, glProgramUniform4f, (GLuint,GLint,GLfloat,GLfloat,GLfloat,GLfloat)) \
    X(void, glProgramUniform1i, (GLuint,GLint,GLint)) \
    X(void, glProgramUniform2i, (GLuint,GLint,GLint,GLint)) \
    X(void, glProgramUniform3i, (GLuint,GLint,GLint,GLint,GLint)) \
    X(void, glProgramUniform4i, (GLuint,GLint,GLint,GLint,GLint,GLint)) \
    X(void, glProgramUniform1ui, (GLuint,GLint,GLuint)) \
    X(void, glProgramUniform2ui, (GLuint,GLint,GLuint,GLuint)) \
    X(void, glProgramUniform3ui, (GLuint,GLint,GLuint,GLuint,GLuint)) \
    X(void, glProgramUniform4ui, (GLuint,GLint,GLuint,GLuint,GLuint,GLuint)) \
    X(void, glProgramUniform1fv, (GLuint,GLint,GLsizei,const GLfloat*)) \
    X(void, glProgramUniform2fv, (GLuint,GLint,GLsizei,const GLfloat*)) \
    X(void, glProgramUniform3fv, (GLuint,GLint,GLsizei,const GLfloat*)) \
    X(void, glProgramUniform4fv, (GLuint,GLint,GLsizei,const GLfloat*)) \
    X(void, glProgramUniform1iv, (GLuint,GLint,GLsizei,const GLint*)) \
    X(void, glProgramUniform2iv, (GLuint,GLint,GLsizei,const GLint*)) \
    X(void, glProgramUniform3iv, (GLuint,GLint,GLsizei,const GLint*)) \
    X(void, glProgramUniform4iv, (GLuint,GLint,GLsizei,const GLint*)) \
    X(void, glProgramUniform1uiv, (GLuint,GLint,GLsizei,const GLuint*)) \
    X(void, glProgramUniform2uiv, (GLuint,GLint,GLsizei,const GLuint*)) \
    X(void, glProgramUniform3uiv, (GLuint,GLint,GLsizei,const GLuint*)) \
    X(void, glProgramUniform4uiv, (GLuint,GLint,GLsizei,const GLuint*)) \
    X(void, glProgramUniformMatrix2fv, (GLuint,GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glProgramUniformMatrix3fv, (GLuint,GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glProgramUniformMatrix4fv, (GLuint,GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glProgramUniformMatrix2x3fv, (GLuint,GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glProgramUniformMatrix3x2fv, (GLuint,GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glProgramUniformMatrix2x4fv, (GLuint,GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glProgramUniformMatrix4x2fv, (GLuint,GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glProgramUniformMatrix3x4fv, (GLuint,GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void, glProgramUniformMatrix4x3fv, (GLuint,GLint,GLsizei,GLboolean,const GLfloat*))

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
    X(void, glGetTexLevelParameteriv, (GLenum, GLint, GLenum, GLint*)) \
    X(void, glDrawElementsBaseVertex, (GLenum, GLsizei, GLenum, const void*, GLint)) \
    X(void, glDrawRangeElementsBaseVertex, (GLenum, GLuint, GLuint, GLsizei, GLenum, const void*, GLint)) \
    X(void, glDrawElementsInstancedBaseVertex, (GLenum, GLsizei, GLenum, const void*, GLsizei, GLint)) \
    X(void, glFramebufferTexture, (GLenum, GLenum, GLuint, GLint)) \
    X(void, glTexStorage2DMultisample, (GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean)) \
    X(void, glTexStorage3DMultisample, (GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, GLboolean)) \
    X(void, glGetMultisamplefv, (GLenum, GLuint, GLfloat*)) \
    X(void, glSampleMaski, (GLuint, GLbitfield)) \
    X(void, glProvokingVertex, (GLenum)) \
    /* ---- ES 3.2 / extensiones ---- */ \
    X(void, glPatchParameteri, (GLenum,GLint)) \
    X(void, glMinSampleShading, (GLfloat)) \
    X(void, glBlendEquationi, (GLuint,GLenum)) \
    X(void, glBlendEquationSeparatei, (GLuint,GLenum,GLenum)) \
    X(void, glBlendFunci, (GLuint,GLenum,GLenum)) \
    X(void, glBlendFuncSeparatei, (GLuint,GLenum,GLenum,GLenum,GLenum)) \
    X(void, glCopyImageSubData, (GLuint,GLenum,GLint,GLint,GLint,GLint,GLuint,GLenum,GLint,GLint,GLint,GLint,GLsizei,GLsizei,GLsizei)) \
    X(void, glTextureView, (GLuint,GLenum,GLuint,GLenum,GLuint,GLuint,GLuint,GLuint)) \
    X(void, glDrawArraysInstancedBaseInstance, (GLenum,GLint,GLsizei,GLsizei,GLuint)) \
    X(void, glDrawElementsInstancedBaseInstance, (GLenum,GLsizei,GLenum,const void*,GLsizei,GLuint)) \
    X(void, glDrawElementsInstancedBaseVertexBaseInstance, (GLenum,GLsizei,GLenum,const void*,GLsizei,GLint,GLuint)) \
    X(void, glDebugMessageControl, (GLenum,GLenum,GLenum,GLsizei,const GLuint*,GLboolean)) \
    X(void, glDebugMessageCallback, (GLDEBUGPROC,const void*)) \
    X(void, glDebugMessageInsert, (GLenum,GLenum,GLuint,GLenum,GLsizei,const GLchar*)) \
    X(void, glPushDebugGroup, (GLenum,GLuint,GLsizei,const GLchar*)) \
    X(void, glPopDebugGroup, (void)) \
    X(void, glObjectLabel, (GLenum,GLuint,GLsizei,const GLchar*)) \
    X(void, glGetObjectLabel, (GLenum,GLuint,GLsizei,GLsizei*,GLchar*)) \
    /* ---- GL 3.3 ---- */ \
    X(void, glQueryCounter, (GLuint,GLenum)) \
    X(void, glGetQueryObjecti64v, (GLuint,GLenum,GLint64*)) \
    X(void, glGetQueryObjectui64v, (GLuint,GLenum,GLuint64*))

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
    int base_vertex;            /* ES 3.2 o EXT/OES_draw_elements_base_vertex (si no: se emula) */
    int geometry;               /* ES 3.2 o EXT/OES_geometry_shader */
    int multisample_tex;        /* ES 3.1+: texturas 2D multisample, glSampleMaski, glGetMultisamplefv */
    int ms_array;               /* ES 3.2 o OES_texture_storage_multisample_2d_array */
    int provoking_vertex;       /* glProvokingVertex disponible (EXT/ANGLE/OES) */
    int depth_clamp;            /* EXT_depth_clamp */
    int clip_distance;          /* 0 = no, 1 = EXT_clip_cull_distance, 2 = ANGLE_clip_cull_distance */
    int glsl_es;                /* version GLSL ES a la que se convierten TODOS los shaders: 300/310/320 */
    int io_blocks_ext;          /* ES 3.1 con GL_EXT_shader_io_blocks (bloques in/out entre etapas) */
    /* GL 4.x */
    int es31;                   /* ES 3.1+: compute, SSBO, image load/store, atomic counters, draw indirect,
                                   vertex attrib binding, separate shader objects, program interface query */
    int tess;                   /* ES 3.2 o EXT/OES_tessellation_shader */
    int cube_array;             /* ES 3.2 o EXT/OES_texture_cube_map_array */
    int sample_shading;         /* ES 3.2 o OES_sample_shading */
    int texture_view;           /* ES 3.2 o EXT/OES_texture_view */
    int copy_image;             /* ES 3.2 o EXT/OES_copy_image */
    int base_instance;          /* EXT_base_instance (si no, se emula con punteros de atributos) */
    int debug;                  /* ES 3.2 o KHR_debug */
    int timer;                  /* EXT_disjoint_timer_query: GL_TIME_ELAPSED / glQueryCounter */
    int dual_src;               /* EXT_blend_func_extended: salidas index=1 */
} gl31_caps_t;

extern gl31_caps_t gl31_caps;

typedef struct {
#define X(ret, name, args) ret (*name) args;
    GL31_BACKEND_FUNCS(X)
    GL31_BACKEND_OPT_FUNCS(X)
    GL31_ES31_FUNCS(X)
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
    /* GL_UNPACK_* (glPixelStorei); lo usa la conversion BGRA->RGBA de las subidas de texturas */
    GLint     unpack_alignment, unpack_row_length, unpack_skip_rows, unpack_skip_pixels;
    GLint     unpack_image_height, unpack_skip_images;
    GLenum    provoking_vertex;
    GLuint    pipeline;            /* program pipeline enlazada (GL 4.1) */
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
/* Version de GL que se anuncia: la mayor que el backend soporta, con tope (por defecto 3.2).
 * El tope se sube con gl31_set_max_version(4, 3) o LORICA_GL_MAX_VERSION=4.3 (leida en gl31_init). */
void gl31_set_max_version(int major, int minor);
/* Version/perfil del contexto actual (0,0,0 = sin tope por contexto). Lo llama ActivateGLState. */
void gl31_set_context_version(int major, int minor, int profile);
void gl31_advertised_version(int* major, int* minor);
int  gl31_advertised_minor(void);   /* compat: minor de la version anunciada */
const GLubyte* gl31_glGetString(GLenum name);
const GLubyte* gl31_glGetStringi(GLenum name, GLuint index);
void gl31_glGetIntegerv(GLenum pname, GLint* data);

/* gl31_shader_glsl.c: GLSL 1.30/1.40/1.50/3.30 -> GLSL ES 3.00.
 * Devuelve memoria malloc'd (free) o NULL y rellena err. */
char* gl31_glsl_convert(const char* src, GLenum shader_type, char* err, size_t errlen);

/* gl31_shader_link.c */
void   gl31_link_shutdown(void);
void   gl31_vao_shutdown(void);
void   gl31_tex_shutdown(void);
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

/* ---------- GL 3.2 / 3.0 / 3.1: bloque 3 ---------- */
/* gl31_sync.c */
GLsync gl31_glFenceSync(GLenum condition, GLbitfield flags);
GLboolean gl31_glIsSync(GLsync sync);
void   gl31_glDeleteSync(GLsync sync);
GLenum gl31_glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout);
void   gl31_glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout);
void   gl31_glGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* values);
void   gl31_glGetInteger64v(GLenum pname, GLint64* data);
void   gl31_glGetInteger64i_v(GLenum target, GLuint index, GLint64* data);
void   gl31_glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64* params);

/* gl31_draw.c */
void gl31_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void* indices, GLint basevertex);
void gl31_glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void* indices, GLint basevertex);
void gl31_glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei inst, GLint basevertex);
void gl31_glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices, GLsizei drawcount, const GLint* basevertex);
void gl31_glProvokingVertex(GLenum mode);

/* gl31_vao.c: atributos por VAO (para emular base vertex) */
typedef struct {
    int       enabled, integer, normalized;
    GLint     size;
    GLenum    type;
    GLsizei   stride;
    const void* ptr;
    GLuint    buffer;
    GLuint    divisor;
    GLuint    binding;      /* indice de binding (GL 4.3); con glVertexAttribPointer es el propio indice */
    GLuint    reloff;       /* relativeoffset (glVertexAttribFormat) */
} gl31_attrib_t;
typedef struct { GLuint buffer; GLintptr offset; GLsizei stride; GLuint divisor; } gl31_vbind_t;
#define GL31_MAX_ATTRIBS 32
/* tabla de bindings del VAO activo (NULL si no hay VAO) */
const gl31_vbind_t* gl31_vao_bindings(int* count);
/* NULL si no hay VAO activo */
const gl31_attrib_t* gl31_vao_attribs(int* count);

/* gl31_texture.c */
void gl31_glTexSubImage1D(GLenum target, GLint level, GLint xo, GLsizei w, GLenum format, GLenum type, const void* pixels);
void gl31_glCopyTexImage1D(GLenum target, GLint level, GLenum ifmt, GLint x, GLint y, GLsizei w, GLint border);
void gl31_glCopyTexSubImage1D(GLenum target, GLint level, GLint xo, GLint x, GLint y, GLsizei w);
void gl31_glCompressedTexImage1D(GLenum target, GLint level, GLenum ifmt, GLsizei w, GLint border, GLsizei size, const void* data);
void gl31_glCompressedTexSubImage1D(GLenum target, GLint level, GLint xo, GLsizei w, GLenum format, GLsizei size, const void* data);
void gl31_glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum ifmt, GLsizei w, GLsizei h, GLboolean fixed);
void gl31_glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum ifmt, GLsizei w, GLsizei h, GLsizei d, GLboolean fixed);
void gl31_glGetMultisamplefv(GLenum pname, GLuint index, GLfloat* val);
void gl31_glSampleMaski(GLuint maskNumber, GLbitfield mask);
void gl31_glGetCompressedTexImage(GLenum target, GLint level, void* img);

/* gl31_state.c: samplers (adicionales) */
void gl31_glSamplerParameteriv(GLuint s, GLenum pname, const GLint* v);
void gl31_glSamplerParameterfv(GLuint s, GLenum pname, const GLfloat* v);
void gl31_glSamplerParameterIiv(GLuint s, GLenum pname, const GLint* v);
void gl31_glSamplerParameterIuiv(GLuint s, GLenum pname, const GLuint* v);
void gl31_glGetSamplerParameterfv(GLuint s, GLenum pname, GLfloat* v);
void gl31_glGetSamplerParameterIiv(GLuint s, GLenum pname, GLint* v);
void gl31_glGetSamplerParameterIuiv(GLuint s, GLenum pname, GLuint* v);

/* gl31_fbo.c */
void gl31_glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level);

/* gl31_shader_glsl.c: conversion con informacion adicional.
 * `samplers` (opcional) recibe los samplers de desktop que no existen en ES
 * (sampler1D / 1DArray / 2DRect) para poder informar su tipo real a la app. */
#define GL31_MAX_SAMPLER_INFO 32
typedef struct {
    int n;
    struct { char name[64]; GLenum type; } s[GL31_MAX_SAMPLER_INFO];
} gl31_sampler_info_t;
typedef struct { const char* name; GLuint location; GLuint index; } gl31_fragbind_t;
char* gl31_glsl_convert_ex(const char* src, GLenum shader_type,
                           const gl31_fragbind_t* binds, int nbinds,
                           gl31_sampler_info_t* samplers, char* err, size_t errlen);
/* tipo de desktop (GL_SAMPLER_1D...) de un uniform del programa, o `backend_type` si no esta en la tabla */
GLenum gl31_program_uniform_type(GLuint program, const char* uniform_name, GLenum backend_type);

/* ---- GL 4.x (gl31_compute.c, gl31_gl4.c y ampliaciones de otros modulos) ---- */
void gl31_glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint* buffers);
void gl31_glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint* buffers, const GLintptr* offsets, const GLsizeiptr* sizes);
void gl31_glDispatchCompute(GLuint x, GLuint y, GLuint z);
void gl31_glDispatchComputeIndirect(GLintptr offset);
void gl31_glMemoryBarrier(GLbitfield barriers);
void gl31_glMemoryBarrierByRegion(GLbitfield barriers);
void gl31_glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);
void gl31_glBindImageTextures(GLuint first, GLsizei count, const GLuint* textures);
void gl31_glFramebufferParameteri(GLenum target, GLenum pname, GLint param);
void gl31_glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint* params);
void gl31_glGetProgramInterfaceiv(GLuint program, GLenum iface, GLenum pname, GLint* params);
GLuint gl31_glGetProgramResourceIndex(GLuint program, GLenum iface, const GLchar* name);
void gl31_glGetProgramResourceName(GLuint program, GLenum iface, GLuint index, GLsizei bufSize, GLsizei* length, GLchar* name);
GLint gl31_glGetProgramResourceLocation(GLuint program, GLenum iface, const GLchar* name);
void gl31_glGetProgramResourceiv(GLuint program, GLenum iface, GLuint index, GLsizei propCount, const GLenum* props, GLsizei bufSize, GLsizei* length, GLint* params);
void gl31_glShaderStorageBlockBinding(GLuint program, GLuint index, GLuint binding);
void gl31_glGenProgramPipelines(GLsizei n, GLuint* pipelines);
void gl31_glDeleteProgramPipelines(GLsizei n, const GLuint* pipelines);
void gl31_glBindProgramPipeline(GLuint pipeline);
GLboolean gl31_glIsProgramPipeline(GLuint pipeline);
void gl31_glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program);
void gl31_glActiveShaderProgram(GLuint pipeline, GLuint program);
void gl31_glValidateProgramPipeline(GLuint pipeline);
void gl31_glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint* params);
void gl31_glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize, GLsizei* length, GLchar* log);
void gl31_glProgramUniform1f(GLuint program, GLint location, GLfloat v0);
void gl31_glProgramUniform2f(GLuint program, GLint location, GLfloat v0, GLfloat v1);
void gl31_glProgramUniform3f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void gl31_glProgramUniform4f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void gl31_glProgramUniform1i(GLuint program, GLint location, GLint v0);
void gl31_glProgramUniform2i(GLuint program, GLint location, GLint v0, GLint v1);
void gl31_glProgramUniform3i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2);
void gl31_glProgramUniform4i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
void gl31_glProgramUniform1ui(GLuint program, GLint location, GLuint v0);
void gl31_glProgramUniform2ui(GLuint program, GLint location, GLuint v0, GLuint v1);
void gl31_glProgramUniform3ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2);
void gl31_glProgramUniform4ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
void gl31_glProgramUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat* value);
void gl31_glProgramUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat* value);
void gl31_glProgramUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat* value);
void gl31_glProgramUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat* value);
void gl31_glProgramUniform1iv(GLuint program, GLint location, GLsizei count, const GLint* value);
void gl31_glProgramUniform2iv(GLuint program, GLint location, GLsizei count, const GLint* value);
void gl31_glProgramUniform3iv(GLuint program, GLint location, GLsizei count, const GLint* value);
void gl31_glProgramUniform4iv(GLuint program, GLint location, GLsizei count, const GLint* value);
void gl31_glProgramUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint* value);
void gl31_glProgramUniform2uiv(GLuint program, GLint location, GLsizei count, const GLuint* value);
void gl31_glProgramUniform3uiv(GLuint program, GLint location, GLsizei count, const GLuint* value);
void gl31_glProgramUniform4uiv(GLuint program, GLint location, GLsizei count, const GLuint* value);
void gl31_glProgramUniformMatrix2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glProgramUniformMatrix3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glProgramUniformMatrix4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glProgramUniformMatrix2x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glProgramUniformMatrix3x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glProgramUniformMatrix2x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glProgramUniformMatrix4x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glProgramUniformMatrix3x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glProgramUniformMatrix4x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void gl31_glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei inst, GLuint binst);
void gl31_glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei inst, GLuint binst);
void gl31_glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei inst, GLint basevertex, GLuint binst);
void gl31_glDrawArraysIndirect(GLenum mode, const void* indirect);
void gl31_glDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect);
void gl31_glMultiDrawArraysIndirect(GLenum mode, const void* indirect, GLsizei drawcount, GLsizei stride);
void gl31_glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect, GLsizei drawcount, GLsizei stride);
void gl31_glDrawTransformFeedback(GLenum mode, GLuint id);
void gl31_glDrawTransformFeedbackInstanced(GLenum mode, GLuint id, GLsizei inst);
void gl31_glDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream);
void gl31_glPatchParameteri(GLenum pname, GLint value);
void gl31_glPatchParameterfv(GLenum pname, const GLfloat* values);
void gl31_glMinSampleShading(GLfloat value);
void gl31_glBlendEquationi(GLuint buf, GLenum mode);
void gl31_glBlendEquationSeparatei(GLuint buf, GLenum rgb, GLenum alpha);
void gl31_glBlendFunci(GLuint buf, GLenum sfactor, GLenum dfactor);
void gl31_glBlendFuncSeparatei(GLuint buf, GLenum srgb, GLenum drgb, GLenum salpha, GLenum dalpha);
void gl31_glDebugMessageCallback(GLDEBUGPROC callback, const void* user);
void gl31_glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled);
void gl31_glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* buf);
void gl31_glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar* message);
void gl31_glPopDebugGroup(void);
void gl31_glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar* label);
void gl31_glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, GLchar* label);
GLuint gl31_glGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum* sources, GLenum* types, GLuint* ids, GLenum* severities, GLsizei* lengths, GLchar* messageLog);
void gl31_glViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h);
void gl31_glViewportIndexedfv(GLuint index, const GLfloat* v);
void gl31_glViewportArrayv(GLuint first, GLsizei count, const GLfloat* v);
void gl31_glScissorIndexed(GLuint index, GLint l, GLint b, GLsizei w, GLsizei h);
void gl31_glScissorIndexedv(GLuint index, const GLint* v);
void gl31_glScissorArrayv(GLuint first, GLsizei count, const GLint* v);
void gl31_glDepthRangeIndexed(GLuint index, GLdouble n, GLdouble f);
void gl31_glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble* v);
void gl31_glGetFloati_v(GLenum target, GLuint index, GLfloat* data);
void gl31_glGetDoublei_v(GLenum target, GLuint index, GLdouble* data);
void gl31_glDepthRangef(GLfloat n, GLfloat f);
void gl31_glClearDepthf(GLfloat d);
void gl31_glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);
void gl31_glReleaseShaderCompiler(void);
void gl31_glGenTransformFeedbacks(GLsizei n, GLuint* ids);
void gl31_glDeleteTransformFeedbacks(GLsizei n, const GLuint* ids);
void gl31_glBindTransformFeedback(GLenum target, GLuint id);
GLboolean gl31_glIsTransformFeedback(GLuint id);
void gl31_glPauseTransformFeedback(void);
void gl31_glResumeTransformFeedback(void);
void gl31_glInvalidateFramebuffer(GLenum target, GLsizei n, const GLenum* attachments);
void gl31_glInvalidateSubFramebuffer(GLenum target, GLsizei n, const GLenum* attachments, GLint x, GLint y, GLsizei w, GLsizei h);
void gl31_glInvalidateTexImage(GLuint texture, GLint level);
void gl31_glInvalidateTexSubImage(GLuint texture, GLint level, GLint xo, GLint yo, GLint zo, GLsizei w, GLsizei h, GLsizei d);
void gl31_glInvalidateBufferData(GLuint buffer);
void gl31_glInvalidateBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr length);
void gl31_glClearBufferData(GLenum target, GLenum ifmt, GLenum format, GLenum type, const void* data);
void gl31_glClearBufferSubData(GLenum target, GLenum ifmt, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void* data);
void gl31_glGetInternalformativ(GLenum target, GLenum ifmt, GLenum pname, GLsizei bufSize, GLint* params);
void gl31_glGetActiveAtomicCounterBufferiv(GLuint program, GLuint index, GLenum pname, GLint* params);
GLuint gl31_glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar* const* strings);
void gl31_glProgramParameteri(GLuint program, GLenum pname, GLint value);
void gl31_glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei* length, GLenum* format, void* binary);
void gl31_glProgramBinary(GLuint program, GLenum format, const void* binary, GLsizei length);
GLenum gl31_err_peek(void);
void gl31_glBindSamplers(GLuint first, GLsizei count, const GLuint* samplers);
GLenum gl31_tex_level0_format(GLuint id);
void gl31_glTextureView(GLuint texture, GLenum target, GLuint orig, GLenum ifmt, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers);
void gl31_glCopyImageSubData(GLuint sn, GLenum st, GLint sl, GLint sx, GLint sy, GLint sz, GLuint dn, GLenum dt, GLint dl, GLint dx, GLint dy, GLint dz, GLsizei w, GLsizei h, GLsizei d);
void gl31_glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum ifmt, GLsizei w, GLsizei h, GLboolean fixed);
void gl31_glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum ifmt, GLsizei w, GLsizei h, GLsizei d, GLboolean fixed);
void gl31_glTexBufferRange(GLenum target, GLenum ifmt, GLuint buffer, GLintptr offset, GLsizeiptr size);
void gl31_glBindTextures(GLuint first, GLsizei count, const GLuint* textures);
void gl31_glVertexAttribFormat(GLuint idx, GLint size, GLenum type, GLboolean norm, GLuint reloff);
void gl31_glVertexAttribIFormat(GLuint idx, GLint size, GLenum type, GLuint reloff);
void gl31_glVertexAttribLFormat(GLuint idx, GLint size, GLenum type, GLuint reloff);
void gl31_glVertexAttribBinding(GLuint idx, GLuint binding);
void gl31_glBindVertexBuffer(GLuint binding, GLuint buffer, GLintptr offset, GLsizei stride);
void gl31_glVertexBindingDivisor(GLuint binding, GLuint divisor);
void gl31_glBindVertexBuffers(GLuint first, GLsizei count, const GLuint* buffers, const GLintptr* offsets, const GLsizei* strides);


GLenum gl31_tex_target_of(GLuint id);
int    gl31_tex_bound_for(GLenum target, GLuint* out);
GLuint gl31_tex_active_unit(void);
void  gl31_query_reset(void);
/* ---- GL 3.3 ---- */
void  gl31_glQueryCounter(GLuint id, GLenum target);
void  gl31_glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64* params);
void  gl31_glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params);
void  gl31_glBeginQueryIndexed(GLenum target, GLuint index, GLuint id);
void  gl31_glEndQueryIndexed(GLenum target, GLuint index);
void  gl31_glGetQueryIndexediv(GLenum target, GLuint index, GLenum pname, GLint* params);
void  gl31_glBindFragDataLocationIndexed(GLuint program, GLuint colorNumber, GLuint index, const GLchar* name);
GLint gl31_glGetFragDataIndex(GLuint program, const GLchar* name);
void  gl31_glVertexAttribP1ui(GLuint index, GLenum type, GLboolean normalized, GLuint value);
void  gl31_glVertexAttribP2ui(GLuint index, GLenum type, GLboolean normalized, GLuint value);
void  gl31_glVertexAttribP3ui(GLuint index, GLenum type, GLboolean normalized, GLuint value);
void  gl31_glVertexAttribP4ui(GLuint index, GLenum type, GLboolean normalized, GLuint value);
void  gl31_glVertexAttribP1uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint* value);
void  gl31_glVertexAttribP2uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint* value);
void  gl31_glVertexAttribP3uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint* value);
void  gl31_glVertexAttribP4uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint* value);

/* ---- ARB_direct_state_access (gl31_dsa.c) ---- */
void gl31_glCreateTextures(GLenum target, GLsizei n, GLuint* textures);
void gl31_glBindTextureUnit(GLuint unit, GLuint texture);
void gl31_glTextureParameteri(GLuint t, GLenum p, GLint v);
void gl31_glTextureParameterf(GLuint t, GLenum p, GLfloat v);
void gl31_glTextureParameteriv(GLuint t, GLenum p, const GLint* v);
void gl31_glTextureParameterfv(GLuint t, GLenum p, const GLfloat* v);
void gl31_glTextureParameterIiv(GLuint t, GLenum p, const GLint* v);
void gl31_glTextureParameterIuiv(GLuint t, GLenum p, const GLuint* v);
void gl31_glGetTextureParameteriv(GLuint t, GLenum p, GLint* v);
void gl31_glGetTextureParameterfv(GLuint t, GLenum p, GLfloat* v);
void gl31_glGetTextureParameterIiv(GLuint t, GLenum p, GLint* v);
void gl31_glGetTextureParameterIuiv(GLuint t, GLenum p, GLuint* v);
void gl31_glGetTextureLevelParameteriv(GLuint t, GLint l, GLenum p, GLint* v);
void gl31_glGetTextureLevelParameterfv(GLuint t, GLint l, GLenum p, GLfloat* v);
void gl31_glGenerateTextureMipmap(GLuint t);
void gl31_glTextureStorage1D(GLuint t, GLsizei lv, GLenum f, GLsizei w);
void gl31_glTextureStorage2D(GLuint t, GLsizei lv, GLenum f, GLsizei w, GLsizei h);
void gl31_glTextureStorage3D(GLuint t, GLsizei lv, GLenum f, GLsizei w, GLsizei h, GLsizei d);
void gl31_glTextureStorage2DMultisample(GLuint t, GLsizei s, GLenum f, GLsizei w, GLsizei h, GLboolean fixed);
void gl31_glTextureStorage3DMultisample(GLuint t, GLsizei s, GLenum f, GLsizei w, GLsizei h, GLsizei d, GLboolean fixed);
void gl31_glTextureBuffer(GLuint t, GLenum f, GLuint buf);
void gl31_glTextureBufferRange(GLuint t, GLenum f, GLuint buf, GLintptr off, GLsizeiptr size);
void gl31_glTextureSubImage1D(GLuint t, GLint l, GLint x, GLsizei w, GLenum f, GLenum ty, const void* p);
void gl31_glTextureSubImage2D(GLuint t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const void* p);
void gl31_glTextureSubImage3D(GLuint t, GLint l, GLint x, GLint y, GLint z, GLsizei w, GLsizei h, GLsizei d,
                              GLenum f, GLenum ty, const void* p);
void gl31_glCompressedTextureSubImage1D(GLuint t, GLint l, GLint x, GLsizei w, GLenum f, GLsizei sz, const void* p);
void gl31_glCompressedTextureSubImage2D(GLuint t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLsizei sz, const void* p);
void gl31_glCompressedTextureSubImage3D(GLuint t, GLint l, GLint x, GLint y, GLint z, GLsizei w, GLsizei h, GLsizei d,
                                        GLenum f, GLsizei sz, const void* p);
void gl31_glCopyTextureSubImage1D(GLuint t, GLint l, GLint xo, GLint x, GLint y, GLsizei w);
void gl31_glCopyTextureSubImage2D(GLuint t, GLint l, GLint xo, GLint yo, GLint x, GLint y, GLsizei w, GLsizei h);
void gl31_glCopyTextureSubImage3D(GLuint t, GLint l, GLint xo, GLint yo, GLint zo, GLint x, GLint y, GLsizei w, GLsizei h);
void gl31_glGetTextureImage(GLuint t, GLint l, GLenum f, GLenum ty, GLsizei bufSize, void* pixels);
void gl31_glGetCompressedTextureImage(GLuint t, GLint l, GLsizei bufSize, void* pixels);
void gl31_glCreateBuffers(GLsizei n, GLuint* buffers);
void gl31_glNamedBufferData(GLuint b, GLsizeiptr size, const void* data, GLenum usage);
void gl31_glNamedBufferStorage(GLuint b, GLsizeiptr size, const void* data, GLbitfield flags);
void gl31_glNamedBufferSubData(GLuint b, GLintptr off, GLsizeiptr size, const void* data);
void gl31_glGetNamedBufferSubData(GLuint b, GLintptr off, GLsizeiptr size, void* data);
void gl31_glGetNamedBufferParameteriv(GLuint b, GLenum pn, GLint* v);
void gl31_glGetNamedBufferParameteri64v(GLuint b, GLenum pn, GLint64* v);
void gl31_glGetNamedBufferPointerv(GLuint b, GLenum pn, void** v);
void gl31_glClearNamedBufferData(GLuint b, GLenum ifmt, GLenum f, GLenum ty, const void* data);
void gl31_glClearNamedBufferSubData(GLuint b, GLenum ifmt, GLintptr off, GLsizeiptr size, GLenum f, GLenum ty, const void* data);
void* gl31_glMapNamedBuffer(GLuint b, GLenum access);
void* gl31_glMapNamedBufferRange(GLuint b, GLintptr off, GLsizeiptr len, GLbitfield access);
GLboolean gl31_glUnmapNamedBuffer(GLuint b);
void gl31_glFlushMappedNamedBufferRange(GLuint b, GLintptr off, GLsizeiptr len);
void gl31_glCopyNamedBufferSubData(GLuint rb, GLuint wb, GLintptr ro, GLintptr wo, GLsizeiptr size);
void gl31_glCreateFramebuffers(GLsizei n, GLuint* ids);
void gl31_glNamedFramebufferRenderbuffer(GLuint fbo, GLenum att, GLenum rbt, GLuint rb);
void gl31_glNamedFramebufferTexture(GLuint fbo, GLenum att, GLuint tex, GLint level);
void gl31_glNamedFramebufferTextureLayer(GLuint fbo, GLenum att, GLuint tex, GLint level, GLint layer);
void gl31_glNamedFramebufferDrawBuffer(GLuint fbo, GLenum mode);
void gl31_glNamedFramebufferDrawBuffers(GLuint fbo, GLsizei n, const GLenum* bufs);
void gl31_glNamedFramebufferReadBuffer(GLuint fbo, GLenum mode);
void gl31_glNamedFramebufferParameteri(GLuint fbo, GLenum pn, GLint v);
void gl31_glGetNamedFramebufferParameteriv(GLuint fbo, GLenum pn, GLint* v);
void gl31_glGetNamedFramebufferAttachmentParameteriv(GLuint fbo, GLenum att, GLenum pn, GLint* v);
GLenum gl31_glCheckNamedFramebufferStatus(GLuint fbo, GLenum target);
void gl31_glInvalidateNamedFramebufferData(GLuint fbo, GLsizei n, const GLenum* att);
void gl31_glInvalidateNamedFramebufferSubData(GLuint fbo, GLsizei n, const GLenum* att, GLint x, GLint y, GLsizei w, GLsizei h);
void gl31_glClearNamedFramebufferiv(GLuint fbo, GLenum buf, GLint db, const GLint* v);
void gl31_glClearNamedFramebufferuiv(GLuint fbo, GLenum buf, GLint db, const GLuint* v);
void gl31_glClearNamedFramebufferfv(GLuint fbo, GLenum buf, GLint db, const GLfloat* v);
void gl31_glClearNamedFramebufferfi(GLuint fbo, GLenum buf, GLint db, GLfloat d, GLint s);
void gl31_glBlitNamedFramebuffer(GLuint rfbo, GLuint dfbo, GLint sx0, GLint sy0, GLint sx1, GLint sy1,
                                 GLint dx0, GLint dy0, GLint dx1, GLint dy1, GLbitfield mask, GLenum filter);
void gl31_glCreateRenderbuffers(GLsizei n, GLuint* ids);
void gl31_glNamedRenderbufferStorage(GLuint rb, GLenum f, GLsizei w, GLsizei h);
void gl31_glNamedRenderbufferStorageMultisample(GLuint rb, GLsizei s, GLenum f, GLsizei w, GLsizei h);
void gl31_glGetNamedRenderbufferParameteriv(GLuint rb, GLenum pn, GLint* v);
void gl31_glCreateVertexArrays(GLsizei n, GLuint* ids);
void gl31_glVertexArrayElementBuffer(GLuint vao, GLuint buffer);
void gl31_glVertexArrayVertexBuffer(GLuint vao, GLuint binding, GLuint buffer, GLintptr off, GLsizei stride);
void gl31_glVertexArrayVertexBuffers(GLuint vao, GLuint first, GLsizei count, const GLuint* b, const GLintptr* o, const GLsizei* s);
void gl31_glVertexArrayAttribFormat(GLuint vao, GLuint idx, GLint size, GLenum type, GLboolean norm, GLuint reloff);
void gl31_glVertexArrayAttribIFormat(GLuint vao, GLuint idx, GLint size, GLenum type, GLuint reloff);
void gl31_glVertexArrayAttribLFormat(GLuint vao, GLuint idx, GLint size, GLenum type, GLuint reloff);
void gl31_glVertexArrayAttribBinding(GLuint vao, GLuint idx, GLuint binding);
void gl31_glVertexArrayBindingDivisor(GLuint vao, GLuint binding, GLuint divisor);
void gl31_glEnableVertexArrayAttrib(GLuint vao, GLuint idx);
void gl31_glDisableVertexArrayAttrib(GLuint vao, GLuint idx);
void gl31_glGetVertexArrayiv(GLuint vao, GLenum pname, GLint* param);
void gl31_glGetVertexArrayIndexediv(GLuint vao, GLuint idx, GLenum pname, GLint* param);
void gl31_glCreateSamplers(GLsizei n, GLuint* ids);
void gl31_glCreateProgramPipelines(GLsizei n, GLuint* ids);
void gl31_glCreateQueries(GLenum target, GLsizei n, GLuint* ids);
void gl31_glCreateTransformFeedbacks(GLsizei n, GLuint* ids);
void gl31_glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer);
void gl31_glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer, GLintptr off, GLsizeiptr size);
GLenum gl31_glGetGraphicsResetStatus(void);

#endif /* LORICA_GL31_H */
