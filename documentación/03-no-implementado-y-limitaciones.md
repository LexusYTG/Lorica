# Lo que no está implementado y limitaciones conocidas

## 1. No publicado (la app recibe `NULL`)

GLES no puede expresar estas funciones, así que `glGetProcAddress` devuelve `NULL` y las apps
pueden detectar que no existen.

| Función / característica | Motivo |
|---|---|
| `glClipControl` | ES no tiene equivalente |
| Bindless textures, sparse residency | Fuera del alcance de ES |
| `glUniform1d` y resto de uniforms de doble precisión | ES no tiene fp64 |
| `glUniformSubroutinesuiv` (subroutines) | Sin equivalente; `subroutine` en GLSL se rechaza |
| `glTextureBarrier` | Sin camino actual |
| `glGetTextureSubImage` | No implementado |
| `glGetTransformFeedback*` (getters de objetos de TF) | No implementado |
| `glGetVertexArrayIndexed64iv` | No implementado |
| `glGetnUniform*` | No implementado |
| Funciones de compatibilidad (`glVertexP*ui`, `glColorP*ui`, `glTexCoordP*ui`…) | No existen en perfil core |
| Versiones 4.4 – 4.6 | Faltan `ARB_query_buffer_object`, `ARB_clear_texture`, `ARB_enhanced_layouts`, `glClipControl`, mirror-clamp |

## 2. Stubs y funciones con aviso

Estas funciones existen, pero no hacen lo mismo que en GL de escritorio. Cada una avisa **una sola
vez** por `stderr` (`LORICA GL31: … no soportado sobre GLES (ignorado)`), y si el resultado sería
incorrecto genera además un error GL.

| Función | Comportamiento |
|---|---|
| `glPolygonMode` | Solo `GL_FILL`; `LINE` y `POINT` se ignoran con aviso |
| `glClampColor` | Sin efecto |
| `glLogicOp` | Sin efecto (ES no lo tiene) |
| `glPointSize` | El vertex shader debe escribir `gl_PointSize` |
| `glPointParameter(POINT_SPRITE_COORD_ORIGIN=LOWER_LEFT)` | No soportado |
| `glPixelStorei(UNPACK_SWAP_BYTES / LSB_FIRST)` | Sin efecto |
| `glPixelStorei(PACK_IMAGE_HEIGHT / SKIP_IMAGES)` | No soportado |
| `glPrimitiveRestartIndex` | Solo el índice fijo de ES; con otro valor el primitive restart se desactiva con aviso |
| `glProvokingVertex(GL_FIRST_VERTEX_CONVENTION)` | Solo con `EXT/ANGLE_provoking_vertex` |
| `glDrawBuffer(GL_COLOR_ATTACHMENTi, i>0)` | Aviso |
| `glReadPixels` con DEPTH/STENCIL o formato/tipo sin conversión | Aviso |
| `glGetFramebufferAttachmentParameteriv(GL_FRONT)` | Aviso |
| `glFramebufferTexture` sin geometry shaders | Adjunta solo la capa 0 |
| `glBufferStorage(PERSISTENT / COHERENT)` | Sin mapeo persistente |
| `glClearBufferData/SubData` | Solo patrón cero o `R32F/R32I/R32UI` |
| `glPatchParameterfv` | No aplica niveles por defecto: usar un tessellation control shader |
| `glPatchParameteri`, `glMinSampleShading`, `glEnable(GL_SAMPLE_SHADING)` | Aviso si el backend no ofrece la extensión |
| `glColorMaski`, `glEnablei`, `glDisablei` | Aviso sin `draw_buffers_indexed` |
| `glCopyImageSubData`, `glTextureView` | Aviso sin ES 3.2 / extensión |
| `glCreateShaderProgramv`, `glGetMultisamplefv`, `glSampleMaski` | Aviso sin ES 3.1 |
| `glQueryCounter(GL_TIMESTAMP)` | Sin `EXT_disjoint_timer_query`: resultado 0 |
| `glSamplerParameterIiv/Iuiv` | Aviso sin `border_clamp` |
| `glVertexAttribLFormat` | fp64 no soportado |
| `glCopyTexImage2D(1D_ARRAY)`, `glCompressedTexImage2D(RECTANGLE/1D_ARRAY)` | Casos no soportados |
| `glGetTexLevelParameter(COMPRESSED_IMAGE_SIZE)` | Aviso sin ES 3.1 |
| `glGetActiveAtomicCounterBufferiv` | Usar `glGetProgramResourceiv` |
| `glShaderStorageBlockBinding` | `GL_INVALID_OPERATION`; usar `layout(binding=N)` en el shader |
| `glDrawTransformFeedback*` | `GL_INVALID_OPERATION` |
| `glGetCompressedTexImage` | Siempre `GL_INVALID_OPERATION` |
| `glGetTexImage` | No soporta texturas comprimidas, depth/stencil ni formatos no renderizables |
| `glGetCompressedTextureImage`, `glCompressedTextureSubImage3D` sobre cube map | `GL_INVALID_OPERATION` |
| Dual-source blending sin `EXT_blend_func_extended` | `GL_INVALID_OPERATION`; el `layout(index)` se descarta |

## 3. Limitaciones de lo que sí está implementado

### Límites numéricos y de estado
- `GL_MAX_VIEWPORTS` = 1: viewport, scissor y depth range con arrays solo usan el índice 0.
- `GL_MAX_DUAL_SOURCE_DRAW_BUFFERS` = 1.
- `glBeginQueryIndexed` / `glEndQueryIndexed` / `glGetQueryIndexediv`: solo índice 0.
- `GL_DEPTH_CLAMP`, `GL_TEXTURE_CUBE_MAP_SEAMLESS` y `GL_CLIP_DISTANCEi` en `glEnable` se guardan como
  estado "blando" (se pueden consultar) pero **no se reenvían al backend**.
- 64 bits: `double` se degrada a `float`.

### Texturas
- Las texturas multisample usan almacenamiento inmutable de ES: volver a llamar
  `glTexImage2DMultisample` sobre la misma textura da `GL_INVALID_OPERATION` (en GL escritorio es legal).
- `GL_BGRA` en subidas solo con `GL_UNSIGNED_BYTE` / `UNSIGNED_INT_8_8_8_8_REV` y sin PBO de desempaquetado.
- Compresión RGTC (BC4/BC5) y S3TC solo si el backend las ofrece; no se transcodifican.
- Cube maps en DSA: `glTextureSubImage3D`, `glCopyTextureSubImage3D` y `glGetTextureImage` reparten en
  las 6 caras (sin `GL_UNPACK_IMAGE_HEIGHT`).
- Texture view y copy image requieren ES 3.2 o la extensión EXT/OES correspondiente.

### Dibujo
- Base vertex emulado (ES < 3.2): modifica y restaura los punteros de atributos en cada draw (costo extra
  de CPU) y solo cubre atributos con buffer y sin divisor.
- Base instance emulada: avisa si no es múltiplo del divisor de un atributo.
- `MultiDraw*Indirect` emulado con bucle; `baseVertex`/`baseInstance` dentro de la estructura indirecta los
  resuelve el backend.
- Formatos `image*` sujetos a ES (`r32f/r32i/r32ui` y `rgba*`).

### GLSL
- Sobre samplers 1D/Rect no se admiten `textureProj*`, `textureGrad`, `textureOffset`, `textureGather` ni
  sombras 1D/Rect (error de compilación con mensaje claro). `textureLod` sobre Rect no existe en GL.
- Los helpers de sampler solo se aplican cuando el sampler se declara **en el mismo shader**: un sampler 1D
  pasado a una función de otro shader no se detecta.
- Geometry shaders: el conversor solo ajusta versión, extensiones y layouts; las diferencias semánticas
  (límites de salida, bloques de interfaz) las valida el compilador del driver.
- `subroutine`, `image1D`, `sampler1DShadow`, `sampler2DMSArray` (ES 3.0) se rechazan.

### Contextos, hilos y framebuffer
- El estado se **comparte entre contextos** para shaders, programas, texturas y VAOs, y **no tiene locking**.
- El despacho de símbolos enlazados es global, igual que `glxContext`: con varios hilos activos sobre
  contextos de distinto tipo (core y legacy), gana el último `MakeCurrent`.
- `LIBGL_FB=2` (`usefbo`) no es compatible con contextos GL 3+: el puente cae a ES 2 y GLADIATOR queda
  inactivo. Soportarlo exigiría que GLADIATOR trate el main FBO como "framebuffer 0".
- El despacho por trampolines solo existe en Linux x86_64 y aarch64, y no en builds `-DNOX11`.

## 4. Qué exige cada versión de ES

| Función | ES 3.0 | ES 3.1 | ES 3.2 / extensiones |
|---|---|---|---|
| Compute, SSBO, imágenes, draw indirecto, pipelines | No | Sí | Sí |
| Texturas multisample 2D, `glSampleMaski` | No | Sí | Sí |
| Multisample 2D array | No | Solo con `OES_texture_storage_multisample_2d_array` | Sí |
| Texture buffer | No | Con `EXT/OES_texture_buffer` | Sí |
| Geometry shaders, adyacencia, `glFramebufferTexture` | No | Con `EXT/OES_geometry_shader` | Sí |
| Tessellation | No | Con extensión | Sí |
| `gl_ClipDistance` | No | Con `EXT_clip_cull_distance` | Con la extensión |
| Base vertex nativo | Emulado | Emulado | Nativo |
| Texture view, copy image, cube map array | No | Con extensión | Sí |
| `glProvokingVertex` FIRST | No | No | Con `EXT/ANGLE_provoking_vertex` |

Versión anunciada por backend: ver `01-arquitectura.md`.
