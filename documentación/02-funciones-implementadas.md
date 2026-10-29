# Funciones implementadas

La capa GLADIATOR publica **571 funciones** en su tabla de despacho (`gl31.c`). Las llamadas que
GLES no puede expresar están en `03-no-implementado-y-limitaciones.md`. El índice completo por
área está al final de este documento.

## Resumen por área

| Área | Estado | Cómo se resuelve |
|---|---|---|
| Buffers (VBO, UBO, copy, map, `glBufferStorage`) | Nativo / parcial | `glBufferStorage` sin `PERSISTENT`/`COHERENT`; `glGetBufferSubData` emulado con `MapBufferRange(READ)` + `memcpy` |
| Vertex arrays y atributos | Nativo | Incluye el modelo `glVertexAttribFormat/Binding`, `glBindVertexBuffer(s)`, `glVertexBindingDivisor` |
| Texturas 2D, 3D, arrays, cube map | Nativo | Cube map array con ES 3.2 o extensión |
| Texturas 1D, 1D array, RECTANGLE | Emulado | Como 2D / 2D array |
| Texturas multisample | Nativo en ES ≥ 3.1 | Multisample 2D array con ES 3.2 u `OES_texture_storage_multisample_2d_array`; en ES 3.0 da error con aviso |
| Samplers | Nativo | Todos los `glSamplerParameter*`; `Iiv`/`Iuiv` requieren `border_clamp` |
| `GL_TEXTURE_SWIZZLE_RGBA` | Emulado | Se expande a los cuatro `SWIZZLE_R/G/B/A` |
| Texture buffer (`glTexBuffer[Range]`, `samplerBuffer`) | Condicionado | ES 3.2 nativo; ES 3.1 con `EXT/OES_texture_buffer`; ES 3.0 no |
| Framebuffers y renderbuffers | Nativo | Framebuffer 0 = superficie EGL (ver `01-arquitectura.md`) |
| `glBlitFramebuffer`, `glInvalidate*`, `glClearBuffer*` | Nativo / parcial | `glClearBufferData/SubData` solo con patrón cero o `R32F/R32I/R32UI` |
| Shaders y programas | Nativo + conversor GLSL | Ver sección GLSL |
| Uniforms, UBO, `glProgramUniform*` (33) | Nativo | Bloques de uniforms completos |
| Program pipelines, `glCreateShaderProgramv`, program binary, program interface query | Nativo en ES ≥ 3.1 | |
| Compute, `glDispatchCompute*`, SSBO, atomic counters, image load/store | Nativo en ES ≥ 3.1 | Formatos `image*` limitados a los de ES |
| `glMemoryBarrier[ByRegion]`, `glBindImageTexture(s)` | Nativo en ES ≥ 3.1 | |
| Draw instanciado, rangos, `MultiDraw*` | Nativo | |
| `glDrawElements*BaseVertex` | Emulado en ES < 3.2 | Desplaza punteros de atributos; nativo con ES 3.2 / `EXT_draw_elements_base_vertex` |
| Base instance | Emulado | Mueve offsets de buffers; avisa si no es múltiplo del divisor |
| Draws indirectos | Nativo en ES ≥ 3.1 | `MultiDraw*Indirect` emulado con un bucle |
| Tessellation (`GL_PATCHES`, `glPatchParameteri`) | Condicionado | Requiere tessellation en el backend |
| Geometry shaders y primitivas con adyacencia | Condicionado | ES 3.2 o `EXT/OES_geometry_shader` |
| Transform feedback y objetos de TF | Nativo | `glDrawTransformFeedback*` no (ver `03`) |
| Queries de oclusión | Nativo | `ARB_occlusion_query2` ya viene en ES 3.0 |
| Queries de tiempo (`GL_TIME_ELAPSED`, `glQueryCounter`) | Condicionado | Nativo con `EXT_disjoint_timer_query`; sin ella devuelve 0 con aviso |
| Sync objects | Nativo | |
| Blend por buffer (`glBlendFunci`…), `glEnablei` | Condicionado | Requiere `draw_buffers_indexed` |
| Blend de doble fuente (`GL_SRC1_*`, `layout(index=1)`) | Condicionado | Con `EXT_blend_func_extended`; sin ella `GL_INVALID_OPERATION` |
| `glVertexAttribP*ui` (2_10_10_10) | Emulado | Desempaquetado en CPU hacia `glVertexAttrib4f` |
| Sample shading, `glMinSampleShading` | Condicionado | `OES_sample_shading` o ES 3.2 |
| `glProvokingVertex` | Parcial | Solo `LAST`; `FIRST` con `EXT/ANGLE_provoking_vertex` |
| `glBindFragDataLocation` | Emulado | Reconvierte el fragment shader con `layout(location=N)` al linkear |
| `glGetTexImage` | Emulado | FBO temporal + `glReadPixels`; solo formatos renderizables |
| Subida `GL_BGRA` + `UNSIGNED_BYTE` | Emulado | Intercambio R/B en CPU |
| Texture view, `glCopyImageSubData` | Condicionado | ES 3.2 o `EXT/OES_texture_view` / `EXT_copy_image` |
| Multi-bind (`glBindBuffersBase/Range`, `glBindTextures`, `glBindSamplers`, `glBindImageTextures`, `glBindVertexBuffers`) | Nativo | |
| KHR_debug (`glDebugMessage*`, `glPushDebugGroup`, labels) | Condicionado | Requiere `KHR_debug` en el backend |
| DSA (`glCreate*`, `glNamed*`, `glTexture*`, `glVertexArray*`…, 90 funciones) | Emulado | Ver sección DSA |

"Condicionado" significa que la función existe en la tabla pero su comportamiento depende de la
versión de ES o de las extensiones que ofrezca el driver (se detectan en `gl31_init`).

## Conversor GLSL (`gl31_shader_glsl.c`)

- **Entradas aceptadas:** `#version` 110–150, 330 y 400–460; etapas vertex, fragment, geometry,
  compute, tess control y tess evaluation.
- **Salida:** `#version 300 es`, `310 es` o `320 es` según el backend. Se inyectan calificadores de
  precisión y los `#extension` EXT/OES que el backend exija por debajo de ES 3.2.
- **Samplers:** `sampler1D`, `sampler1DArray` y `sampler2DRect` (y variantes `i`/`u`) pasan a
  `sampler2D` / `sampler2DArray`. `texture`, `textureLod`, `texelFetch` y `textureSize` sobre ellos se
  redirigen a funciones `lorica_*` inyectadas (coordenada extra 0.5; en Rect, `p / textureSize`).
  `glGetActiveUniform` informa el tipo de escritorio (`GL_SAMPLER_1D`…).
- **Tipos de doble precisión:** `double`, `dvecN`, `dmatN` pasan a `float`, `vecN`, `matN`
  (**se pierde precisión**).
- **Se conservan** (en ES ≥ 3.1): `binding`, `offset`, `early_fragment_tests` y `location` entre etapas.
- **Se eliminan:** `gl_PerVertex` redeclarado y `precise` (si ES < 3.2).
- **Se rechazan con error claro:** `subroutine`, `image1D`, `sampler1DShadow`, `sampler2DMSArray`
  (en ES 3.0) y los usos no soportados de samplers 1D/Rect (ver `03`).

## Direct State Access (`gl31_dsa.c`)

ES no tiene acceso por nombre. Cada función DSA hace: enlazar el objeto → llamar a la función clásica
ya traducida → restaurar el enlace anterior. El estado enlazado que ve la app no cambia.

- Cubierto (90 funciones): texturas, buffers, framebuffers, renderbuffers, vertex arrays, samplers,
  queries, program pipelines y transform feedback (`glCreate*`, `glNamed*`, `glTexture*`,
  `glVertexArray*`, `glTransformFeedbackBuffer*`, `glGetGraphicsResetStatus`, que siempre devuelve
  `GL_NO_ERROR`).
- Los objetos creados con `glCreate*` quedan con tipo definido (en ES el tipo lo fija el primer
  enlace). Una textura generada solo con `glGenTextures` da `GL_INVALID_OPERATION` en las funciones
  `glTexture*`.
- Se publica como extensión `GL_ARB_direct_state_access` cuando la versión anunciada es ≥ 3.3.

## Contextos y GLX

Publicados por el front-end GLX: `glXCreateContext`, `glXCreateContextAttribsARB`, `glXMakeCurrent`,
`glXSwapBuffers`, `glXGetProcAddress`, `glXChooseFBConfig`, `glXGetVisualFromFBConfig` y el resto de
la API GLX que necesitan SDL 2, GLFW y cualquier app que hable con X11 (el script de instalación
exige al menos 40 símbolos `glX*` exportados).

## Núcleo gl4es (contextos legacy)

El núcleo heredado de gl4es sigue atendiendo los contextos legacy (pipeline de función fija sobre
ES 2.0, listas de display, evaluadores, ARB programs, etc.). Su documentación de funciones es la del
proyecto upstream (https://github.com/ptitSeb/gl4es); Lorica no cambió su cobertura.

## Índice completo de funciones publicadas (571)

Agrupadas automáticamente por nombre; algunas funciones pueden aparecer en un grupo aproximado.

### Shaders, programas y pipelines (44)

`glActiveShaderProgram`, `glAttachShader`, `glBindAttribLocation`, `glBindFragDataLocation`, `glBindFragDataLocationIndexed`, `glBindProgramPipeline`, `glCompileShader`, `glDeleteProgram`, `glDeleteProgramPipelines`, `glDeleteShader`, `glDetachShader`, `glGenProgramPipelines`, `glGetActiveAttrib`, `glGetAttachedShaders`, `glGetAttribLocation`, `glGetFragDataIndex`, `glGetFragDataLocation`, `glGetProgramBinary`, `glGetProgramInfoLog`, `glGetProgramInterfaceiv`, `glGetProgramPipelineInfoLog`, `glGetProgramPipelineiv`, `glGetProgramResourceIndex`, `glGetProgramResourceLocation`, `glGetProgramResourceName`, `glGetProgramResourceiv`, `glGetProgramiv`, `glGetShaderInfoLog`, `glGetShaderPrecisionFormat`, `glGetShaderSource`, `glGetShaderiv`, `glIsProgram`, `glIsProgramPipeline`, `glIsShader`, `glLinkProgram`, `glProgramBinary`, `glProgramParameteri`, `glReleaseShaderCompiler`, `glShaderSource`, `glShaderStorageBlockBinding`, `glUseProgram`, `glUseProgramStages`, `glValidateProgram`, `glValidateProgramPipeline`

### Texturas y samplers (61)

`glActiveTexture`, `glBindSampler`, `glBindSamplers`, `glBindTexture`, `glBindTextures`, `glCompressedTexImage1D`, `glCompressedTexImage2D`, `glCompressedTexImage3D`, `glCompressedTexSubImage1D`, `glCompressedTexSubImage2D`, `glCompressedTexSubImage3D`, `glCopyImageSubData`, `glCopyTexImage1D`, `glCopyTexImage2D`, `glCopyTexSubImage1D`, `glCopyTexSubImage2D`, `glCopyTexSubImage3D`, `glDeleteSamplers`, `glDeleteTextures`, `glGenSamplers`, `glGenTextures`, `glGenerateMipmap`, `glGetCompressedTexImage`, `glGetSamplerParameterIiv`, `glGetSamplerParameterIuiv`, `glGetSamplerParameterfv`, `glGetSamplerParameteriv`, `glGetTexImage`, `glGetTexLevelParameterfv`, `glGetTexLevelParameteriv`, `glGetTexParameterIiv`, `glGetTexParameterIuiv`, `glGetTexParameterfv`, `glGetTexParameteriv`, `glIsSampler`, `glIsTexture`, `glSamplerParameterIiv`, `glSamplerParameterIuiv`, `glSamplerParameterf`, `glSamplerParameterfv`, `glSamplerParameteri`, `glSamplerParameteriv`, `glTexImage1D`, `glTexImage2D`, `glTexImage2DMultisample`, `glTexImage3D`, `glTexImage3DMultisample`, `glTexParameterIiv`, `glTexParameterIuiv`, `glTexParameterf`, `glTexParameterfv`, `glTexParameteri`, `glTexParameteriv`, `glTexStorage1D`, `glTexStorage2D`, `glTexStorage2DMultisample`, `glTexStorage3D`, `glTexStorage3DMultisample`, `glTexSubImage1D`, `glTexSubImage2D`, `glTexSubImage3D`

### Estado, rasterizado, blending, viewport y consultas (77)

`glBeginConditionalRender`, `glBlendColor`, `glBlendEquation`, `glBlendEquationSeparate`, `glBlendEquationSeparatei`, `glBlendEquationi`, `glBlendFunc`, `glBlendFuncSeparate`, `glBlendFuncSeparatei`, `glBlendFunci`, `glClampColor`, `glColorMask`, `glColorMaski`, `glCullFace`, `glDeleteQueries`, `glDepthFunc`, `glDepthMask`, `glDepthRange`, `glDepthRangeArrayv`, `glDepthRangeIndexed`, `glDepthRangef`, `glDisable`, `glDisablei`, `glEnable`, `glEnablei`, `glEndConditionalRender`, `glFinish`, `glFlush`, `glFrontFace`, `glGenQueries`, `glGetBooleani_v`, `glGetBooleanv`, `glGetDoublei_v`, `glGetDoublev`, `glGetError`, `glGetFloati_v`, `glGetFloatv`, `glGetInteger64i_v`, `glGetInteger64v`, `glGetIntegeri_v`, `glGetIntegerv`, `glGetInternalformativ`, `glGetMultisamplefv`, `glGetString`, `glGetStringi`, `glHint`, `glIsEnabled`, `glIsEnabledi`, `glLineWidth`, `glLogicOp`, `glMinSampleShading`, `glPixelStoref`, `glPixelStorei`, `glPointParameterf`, `glPointParameterfv`, `glPointParameteri`, `glPointParameteriv`, `glPointSize`, `glPolygonMode`, `glPolygonOffset`, `glProvokingVertex`, `glSampleCoverage`, `glSampleMaski`, `glScissor`, `glScissorArrayv`, `glScissorIndexed`, `glScissorIndexedv`, `glStencilFunc`, `glStencilFuncSeparate`, `glStencilMask`, `glStencilMaskSeparate`, `glStencilOp`, `glStencilOpSeparate`, `glViewport`, `glViewportArrayv`, `glViewportIndexedf`, `glViewportIndexedfv`

### Queries y sincronización (18)

`glBeginQuery`, `glBeginQueryIndexed`, `glClientWaitSync`, `glDeleteSync`, `glEndQuery`, `glEndQueryIndexed`, `glFenceSync`, `glGetQueryIndexediv`, `glGetQueryObjecti64v`, `glGetQueryObjectiv`, `glGetQueryObjectui64v`, `glGetQueryObjectuiv`, `glGetQueryiv`, `glGetSynciv`, `glIsQuery`, `glIsSync`, `glQueryCounter`, `glWaitSync`

### Transform feedback (13)

`glBeginTransformFeedback`, `glBindTransformFeedback`, `glDeleteTransformFeedbacks`, `glDrawTransformFeedback`, `glDrawTransformFeedbackInstanced`, `glDrawTransformFeedbackStream`, `glEndTransformFeedback`, `glGenTransformFeedbacks`, `glGetTransformFeedbackVarying`, `glIsTransformFeedback`, `glPauseTransformFeedback`, `glResumeTransformFeedback`, `glTransformFeedbackVaryings`

### Buffers (23)

`glBindBuffer`, `glBindBufferBase`, `glBindBufferRange`, `glBindBuffersBase`, `glBindBuffersRange`, `glBufferData`, `glBufferStorage`, `glBufferSubData`, `glCopyBufferSubData`, `glCopyNamedBufferSubData`, `glDeleteBuffers`, `glFlushMappedBufferRange`, `glGenBuffers`, `glGetBufferParameteri64v`, `glGetBufferParameteriv`, `glGetBufferPointerv`, `glGetBufferSubData`, `glIsBuffer`, `glMapBuffer`, `glMapBufferRange`, `glTexBuffer`, `glTexBufferRange`, `glUnmapBuffer`

### Framebuffers, renderbuffers y clears (41)

`glBindFramebuffer`, `glBindRenderbuffer`, `glBlitFramebuffer`, `glCheckFramebufferStatus`, `glClear`, `glClearBufferData`, `glClearBufferSubData`, `glClearBufferfi`, `glClearBufferfv`, `glClearBufferiv`, `glClearBufferuiv`, `glClearColor`, `glClearDepth`, `glClearDepthf`, `glClearStencil`, `glDeleteFramebuffers`, `glDeleteRenderbuffers`, `glFramebufferParameteri`, `glFramebufferRenderbuffer`, `glFramebufferTexture`, `glFramebufferTexture1D`, `glFramebufferTexture2D`, `glFramebufferTexture3D`, `glFramebufferTextureLayer`, `glGenFramebuffers`, `glGenRenderbuffers`, `glGetFramebufferAttachmentParameteriv`, `glGetFramebufferParameteriv`, `glGetRenderbufferParameteriv`, `glInvalidateBufferData`, `glInvalidateBufferSubData`, `glInvalidateFramebuffer`, `glInvalidateSubFramebuffer`, `glInvalidateTexImage`, `glInvalidateTexSubImage`, `glIsFramebuffer`, `glIsRenderbuffer`, `glReadBuffer`, `glReadPixels`, `glRenderbufferStorage`, `glRenderbufferStorageMultisample`

### Compute, imágenes y barreras (7)

`glBindImageTexture`, `glBindImageTextures`, `glDispatchCompute`, `glDispatchComputeIndirect`, `glGetActiveAtomicCounterBufferiv`, `glMemoryBarrier`, `glMemoryBarrierByRegion`

### DSA (acceso directo por nombre, GL 4.5 ext) (93)

`glBindTextureUnit`, `glBlitNamedFramebuffer`, `glCheckNamedFramebufferStatus`, `glClearNamedBufferData`, `glClearNamedBufferSubData`, `glClearNamedFramebufferfi`, `glClearNamedFramebufferfv`, `glClearNamedFramebufferiv`, `glClearNamedFramebufferuiv`, `glCompressedTextureSubImage1D`, `glCompressedTextureSubImage2D`, `glCompressedTextureSubImage3D`, `glCopyTextureSubImage1D`, `glCopyTextureSubImage2D`, `glCopyTextureSubImage3D`, `glCreateBuffers`, `glCreateFramebuffers`, `glCreateProgram`, `glCreateProgramPipelines`, `glCreateQueries`, `glCreateRenderbuffers`, `glCreateSamplers`, `glCreateShader`, `glCreateShaderProgramv`, `glCreateTextures`, `glCreateTransformFeedbacks`, `glCreateVertexArrays`, `glDisableVertexArrayAttrib`, `glEnableVertexArrayAttrib`, `glFlushMappedNamedBufferRange`, `glGenerateTextureMipmap`, `glGetCompressedTextureImage`, `glGetGraphicsResetStatus`, `glGetNamedBufferParameteri64v`, `glGetNamedBufferParameteriv`, `glGetNamedBufferPointerv`, `glGetNamedBufferSubData`, `glGetNamedFramebufferAttachmentParameteriv`, `glGetNamedFramebufferParameteriv`, `glGetNamedRenderbufferParameteriv`, `glGetTextureImage`, `glGetTextureLevelParameterfv`, `glGetTextureLevelParameteriv`, `glGetTextureParameterIiv`, `glGetTextureParameterIuiv`, `glGetTextureParameterfv`, `glGetTextureParameteriv`, `glGetVertexArrayIndexediv`, `glGetVertexArrayiv`, `glInvalidateNamedFramebufferData`, `glInvalidateNamedFramebufferSubData`, `glMapNamedBuffer`, `glMapNamedBufferRange`, `glNamedBufferData`, `glNamedBufferStorage`, `glNamedBufferSubData`, `glNamedFramebufferDrawBuffer`, `glNamedFramebufferDrawBuffers`, `glNamedFramebufferParameteri`, `glNamedFramebufferReadBuffer`, `glNamedFramebufferRenderbuffer`, `glNamedFramebufferTexture`, `glNamedFramebufferTextureLayer`, `glNamedRenderbufferStorage`, `glNamedRenderbufferStorageMultisample`, `glTextureBuffer`, `glTextureBufferRange`, `glTextureParameterIiv`, `glTextureParameterIuiv`, `glTextureParameterf`, `glTextureParameterfv`, `glTextureParameteri`, `glTextureParameteriv`, `glTextureStorage1D`, `glTextureStorage2D`, `glTextureStorage2DMultisample`, `glTextureStorage3D`, `glTextureStorage3DMultisample`, `glTextureSubImage1D`, `glTextureSubImage2D`, `glTextureSubImage3D`, `glTextureView`, `glTransformFeedbackBufferBase`, `glTransformFeedbackBufferRange`, `glUnmapNamedBuffer`, `glVertexArrayAttribBinding`, `glVertexArrayAttribFormat`, `glVertexArrayAttribIFormat`, `glVertexArrayAttribLFormat`, `glVertexArrayBindingDivisor`, `glVertexArrayElementBuffer`, `glVertexArrayVertexBuffer`, `glVertexArrayVertexBuffers`

### Vertex arrays y atributos (85)

`glBindVertexArray`, `glBindVertexBuffer`, `glBindVertexBuffers`, `glDeleteVertexArrays`, `glDisableVertexAttribArray`, `glEnableVertexAttribArray`, `glGenVertexArrays`, `glGetVertexAttribIiv`, `glGetVertexAttribIuiv`, `glGetVertexAttribPointerv`, `glGetVertexAttribdv`, `glGetVertexAttribfv`, `glGetVertexAttribiv`, `glIsVertexArray`, `glVertexAttrib1d`, `glVertexAttrib1dv`, `glVertexAttrib1f`, `glVertexAttrib1fv`, `glVertexAttrib1s`, `glVertexAttrib1sv`, `glVertexAttrib2d`, `glVertexAttrib2dv`, `glVertexAttrib2f`, `glVertexAttrib2fv`, `glVertexAttrib2s`, `glVertexAttrib2sv`, `glVertexAttrib3d`, `glVertexAttrib3dv`, `glVertexAttrib3f`, `glVertexAttrib3fv`, `glVertexAttrib3s`, `glVertexAttrib3sv`, `glVertexAttrib4Nbv`, `glVertexAttrib4Nsv`, `glVertexAttrib4Nub`, `glVertexAttrib4Nubv`, `glVertexAttrib4Nuiv`, `glVertexAttrib4Nusv`, `glVertexAttrib4bv`, `glVertexAttrib4d`, `glVertexAttrib4dv`, `glVertexAttrib4f`, `glVertexAttrib4fv`, `glVertexAttrib4iv`, `glVertexAttrib4s`, `glVertexAttrib4sv`, `glVertexAttrib4ubv`, `glVertexAttrib4uiv`, `glVertexAttrib4usv`, `glVertexAttribBinding`, `glVertexAttribDivisor`, `glVertexAttribFormat`, `glVertexAttribI1i`, `glVertexAttribI1iv`, `glVertexAttribI1ui`, `glVertexAttribI1uiv`, `glVertexAttribI2i`, `glVertexAttribI2iv`, `glVertexAttribI2ui`, `glVertexAttribI2uiv`, `glVertexAttribI3i`, `glVertexAttribI3iv`, `glVertexAttribI3ui`, `glVertexAttribI3uiv`, `glVertexAttribI4bv`, `glVertexAttribI4i`, `glVertexAttribI4iv`, `glVertexAttribI4sv`, `glVertexAttribI4ubv`, `glVertexAttribI4ui`, `glVertexAttribI4uiv`, `glVertexAttribI4usv`, `glVertexAttribIFormat`, `glVertexAttribIPointer`, `glVertexAttribLFormat`, `glVertexAttribP1ui`, `glVertexAttribP1uiv`, `glVertexAttribP2ui`, `glVertexAttribP2uiv`, `glVertexAttribP3ui`, `glVertexAttribP3uiv`, `glVertexAttribP4ui`, `glVertexAttribP4uiv`, `glVertexAttribPointer`, `glVertexBindingDivisor`

### Debug y etiquetas (8)

`glDebugMessageCallback`, `glDebugMessageControl`, `glDebugMessageInsert`, `glGetDebugMessageLog`, `glGetObjectLabel`, `glObjectLabel`, `glPopDebugGroup`, `glPushDebugGroup`

### Draw calls (23)

`glDrawArrays`, `glDrawArraysIndirect`, `glDrawArraysInstanced`, `glDrawArraysInstancedBaseInstance`, `glDrawBuffer`, `glDrawBuffers`, `glDrawElements`, `glDrawElementsBaseVertex`, `glDrawElementsIndirect`, `glDrawElementsInstanced`, `glDrawElementsInstancedBaseInstance`, `glDrawElementsInstancedBaseVertex`, `glDrawElementsInstancedBaseVertexBaseInstance`, `glDrawRangeElements`, `glDrawRangeElementsBaseVertex`, `glMultiDrawArrays`, `glMultiDrawArraysIndirect`, `glMultiDrawElements`, `glMultiDrawElementsBaseVertex`, `glMultiDrawElementsIndirect`, `glPatchParameterfv`, `glPatchParameteri`, `glPrimitiveRestartIndex`

### Uniforms y bloques de uniforms (45)

`glGetActiveUniform`, `glGetActiveUniformBlockName`, `glGetActiveUniformBlockiv`, `glGetActiveUniformName`, `glGetActiveUniformsiv`, `glGetUniformBlockIndex`, `glGetUniformIndices`, `glGetUniformLocation`, `glGetUniformfv`, `glGetUniformiv`, `glGetUniformuiv`, `glUniform1f`, `glUniform1fv`, `glUniform1i`, `glUniform1iv`, `glUniform1ui`, `glUniform1uiv`, `glUniform2f`, `glUniform2fv`, `glUniform2i`, `glUniform2iv`, `glUniform2ui`, `glUniform2uiv`, `glUniform3f`, `glUniform3fv`, `glUniform3i`, `glUniform3iv`, `glUniform3ui`, `glUniform3uiv`, `glUniform4f`, `glUniform4fv`, `glUniform4i`, `glUniform4iv`, `glUniform4ui`, `glUniform4uiv`, `glUniformBlockBinding`, `glUniformMatrix2fv`, `glUniformMatrix2x3fv`, `glUniformMatrix2x4fv`, `glUniformMatrix3fv`, `glUniformMatrix3x2fv`, `glUniformMatrix3x4fv`, `glUniformMatrix4fv`, `glUniformMatrix4x2fv`, `glUniformMatrix4x3fv`

### glProgramUniform* (33)

`glProgramUniform1f`, `glProgramUniform1fv`, `glProgramUniform1i`, `glProgramUniform1iv`, `glProgramUniform1ui`, `glProgramUniform1uiv`, `glProgramUniform2f`, `glProgramUniform2fv`, `glProgramUniform2i`, `glProgramUniform2iv`, `glProgramUniform2ui`, `glProgramUniform2uiv`, `glProgramUniform3f`, `glProgramUniform3fv`, `glProgramUniform3i`, `glProgramUniform3iv`, `glProgramUniform3ui`, `glProgramUniform3uiv`, `glProgramUniform4f`, `glProgramUniform4fv`, `glProgramUniform4i`, `glProgramUniform4iv`, `glProgramUniform4ui`, `glProgramUniform4uiv`, `glProgramUniformMatrix2fv`, `glProgramUniformMatrix2x3fv`, `glProgramUniformMatrix2x4fv`, `glProgramUniformMatrix3fv`, `glProgramUniformMatrix3x2fv`, `glProgramUniformMatrix3x4fv`, `glProgramUniformMatrix4fv`, `glProgramUniformMatrix4x2fv`, `glProgramUniformMatrix4x3fv`
