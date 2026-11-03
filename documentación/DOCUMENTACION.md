# Documentación de Lorica 1.4.5

Este documento **reemplaza a toda la documentación anterior**: los siete archivos numerados que había
en esta carpeta y las notas sueltas de versiones previas (`GL32.md`, `GL33.md`, `GL4.md`, `GL45.md`,
el README de GLADIATOR y los README de `debian/`). Aparte de este archivo, el único README del
proyecto es el de la raíz.

## Contenido

1. [Arquitectura](#1-arquitectura)
2. [Variables de entorno](#2-variables-de-entorno)
3. [Funciones implementadas](#3-funciones-implementadas)
4. [GPU en lugar de CPU y apoyo de Vulkan](#4-gpu-en-lugar-de-cpu-y-apoyo-de-vulkan)
5. [No publicado, stubs y limitaciones](#5-no-publicado-stubs-y-limitaciones)
6. [Qué exige cada versión de ES](#6-qué-exige-cada-versión-de-es)
7. [EGL y GLX](#7-egl-y-glx)
8. [Compilación, instalación y pruebas](#8-compilación-instalación-y-pruebas)
9. [Problemas conocidos](#9-problemas-conocidos)
10. [Novedades de la 1.4.5](#10-novedades-de-la-145)
11. [Apéndice A: funciones de GLADIATOR](#apéndice-a-funciones-de-gladiator)
12. [Apéndice B: funciones del núcleo legacy](#apéndice-b-funciones-del-núcleo-legacy)

## Objetivo del proyecto

Lorica busca **compatibilidad y corrección**: que las aplicaciones de OpenGL de escritorio funcionen y
decir con honestidad lo que GLES no puede expresar. No es un proyecto de rendimiento: aquí no se mide
ni se compara velocidad, y ninguna decisión de diseño se justifica por ella.

## Cómo leer los estados

- **Nativo**: se reenvía al backend GLES sin cambios de comportamiento.
- **Emulado**: Lorica lo reconstruye con otras llamadas de GLES; funciona, con costo o con casos límite.
- **Condicionado**: la función existe, pero su comportamiento depende de la versión de ES o de las
  extensiones del driver (se detectan en `gl31_init`).
- **Parcial / con aviso**: funciona solo en algunos casos; en los demás avisa por `stderr` y/o genera un
  error GL. Cada aviso sale **una sola vez** por función.
- **Stub**: se publica pero no hace nada.
- **No publicado**: `glGetProcAddress` devuelve `NULL` para que la app detecte que no existe.

## De dónde sale la información

- La lista de funciones sale de la tabla de despacho `src/gl/GLADIATOR/gl31.c` (572 entradas).
- Los avisos salen de las llamadas a `gl31_stub_warn()` en `src/gl/GLADIATOR/*.c`.
- Todo se contrastó con el código de la 1.4.5.

---

## 1. Arquitectura

### Qué es Lorica

Lorica traduce OpenGL de escritorio a OpenGL ES. La app hace llamadas GL normales; Lorica las traduce al
vuelo para una GPU que solo habla GLES. Su motor de traducción central desciende de un proyecto MIT de Sebastien Chevalier y Ryan Hileman (gl4es,
ver «Terceros y licencias»); todo lo que hay encima (negociación de versiones, la capa moderna GLADIATOR, el
puente GLX, DSA, las rutas de GPU y Vulkan) se escribió para Lorica.

### Capas

| Capa | Ruta | Para qué contextos | Backend |
|---|---|---|---|
| Núcleo legacy | `src/gl/` | Contextos **legacy** (GL 1.x–2.1, perfil compatibility, sin versión) | ES 2.0 |
| GLADIATOR | `src/gl/GLADIATOR/` | Contextos **core** GL ≥ 3.1 | ES 3.0 / 3.1 / 3.2 |
| Apoyo Vulkan | `src/vk/` | Trabajo propio de Lorica que GLES no puede hacer (ver sección 4) | Vulkan 1.1 (opcional) |
| Front-end GLX | `src/glx/` | Creación de contextos y superficies sobre el EGL real, `glXSwapBuffers`, despacho | — |
| Wrapper EGL (opcional) | `src/egl/` | EGL implementado sobre GLX (`libEGL.so.1`), ver sección 7 | — |
| Puente | `src/glx/gl31_bridge.c` | Decide qué capa atiende cada contexto | — |

Cada contexto se atiende con una sola capa. Un contexto legacy dentro del mismo proceso sigue reportando
GL 2.1 aunque exista otro contexto core.

### Qué versión de GL se anuncia

`glGetString(GL_VERSION)`, `GL_MAJOR_VERSION` y `GL_MINOR_VERSION` devuelven **lo menor entre lo que el
backend permite y el tope fijado por el usuario**. Los contextos legacy siempre reciben 2.1.

| Versión | Requisitos del backend | GLSL anunciado |
|---|---|---|
| 2.1 | ES 2.0 (contexto legacy) | 1.20 |
| 3.1 | ES 3.0 | 1.40 |
| 3.2 / 3.3 | ES 3.2 o extensión de geometry shader | 1.50 / 3.30 |
| 4.0 – 4.2 | ES 3.1 (compute, imágenes) + tessellation, cube map array, sample shading, blend indexado | 4.00 – 4.20 |
| 4.3 | + texture view, copy image, multisample 2D array | 4.30 |

No se llega a 4.4–4.6: hay funciones (bindless, sparse) que GLES no puede expresar.
`GL_ARB_direct_state_access` se publica como extensión cuando la versión anunciada es ≥ 3.3, aunque Lorica
no anuncie 4.5.

### Cómo se fija el tope

1. En la capa GLADIATOR el tope por defecto es **3.3** (`gl31_query.c`).
2. El puente lo sube o baja siguiendo `LIBGL_GL` (p. ej. `LIBGL_GL=43`), salvo que se defina
   `LORICA_GL_MAX_VERSION` (p. ej. `4.3`), que manda.
3. En Android/Termux, `lorica_android_defaults()` fija por defecto `LIBGL_GL=43` y `LIBGL_FB=3` (pbuffer,
   ver sección 7) y las rutas de `libEGL.so` / `libGLESv2.so` del sistema, si el usuario no las definió.
   Fuera de Android, con `LIBGL_GL` sin definir el tope es 2.1 (`LIBGL_GL_COMPAT` controla la versión de
   los contextos que no piden una).
4. En C también se puede usar `gl31_set_max_version(mayor, menor)`.

### El puente GLX ↔ GLADIATOR (`gl31_bridge.c`)

- **Contexto de backend.** Si la app pide GL ≥ 3.1 con `glXCreateContextAttribsARB` (y no perfil ES), se
  crea un `EGLContext` de ES 3.x: prueba 3.2, luego 3.1, luego 3.0 y se queda con el primero que el driver
  acepte. Hace falta una config con `EGL_OPENGL_ES3_BIT`. Si no la hay, cae a ES 2 con un aviso y
  GLADIATOR queda inactivo.
- **Arranque.** `gl31_init` se llama la primera vez que un contexto ES 3.x queda actual (`glXMakeCurrent`),
  con un cargador que busca primero en la librería GLES y luego en `eglGetProcAddress`. `gl31_shutdown` se
  llama al destruir el último contexto ES 3.x.
- **`glGetProcAddress`.** Primero se consulta GLADIATOR, que devuelve la función solo si el contexto actual
  es de **perfil core**, GL ≥ 3.1, con backend ES 3.x y GLADIATOR iniciado. En cualquier otro caso se atiende
  por gl4es. Los nombres con sufijo `ARB` se resuelven al nombre base si está en la tabla
  (`glBindBufferARB` → `glBindBuffer`). El sufijo `EXT` **no** se traduce, porque en varias funciones (DSA)
  cambia la firma.
- **Funciones enlazadas directamente.** Los símbolos GL exportados (`glEnable`, `glDrawArrays`…) son un
  salto indirecto que `glXMakeCurrent`, `glXMakeContextCurrent` y `glXDestroyContext` repuntan: a GLADIATOR
  con un contexto core ≥ 3.1 y a gl4es en cualquier otro caso. Los slots están en la sección `lorica_disp`.
  Se desactiva con `-DLORICA_NO_DISPATCH`. Solo Linux x86_64 y aarch64; en builds `-DNOX11` no se activa.
- **Framebuffer por defecto.** Con `LIBGL_FB` distinto de 2 (modos en la sección 7), el framebuffer 0 es la
  superficie EGL real y GLADIATOR lo trata directamente (`GL_BACK*` → `GL_BACK`; `GL_FRONT*` avisa). Con
  `LIBGL_FB=2` el puente **no** crea contextos ES 3.x (cae a ES 2) y GLADIATOR queda inactivo.
- **Presentación cero-copia (AHB).** `glx_ahb.c` implementa presentación con `AHardwareBuffer` +
  DRI3/Present. Solo existe en Android; en otros sistemas son funciones vacías.

### Módulos de GLADIATOR (`src/gl/GLADIATOR/`)

| Archivo | Responsabilidad |
|---|---|
| `gl31.c`, `gl31.h` | Tabla de despacho nombre → implementación, declaración de funciones del backend |
| `gl31_caps.c` | `gl31_init`/`gl31_shutdown`, detección de capacidades, carga de funciones opcionales, `LORICA_DISABLE_CAPS` |
| `gl31_state.c`, `gl31_render.c` | Estado por hilo, `glEnable`/`glDisable`, caps «blandas», estado de rasterizado, `glClipControl`, `glPointSize`, depth clamp |
| `gl31_buffer.c`, `gl31_vao.c`, `gl31_attrib.c` | Buffers (incl. `glBufferStorage`), vertex arrays, atributos y binding de vértices |
| `gl31_texture.c` | Texturas, samplers, emulación 1D/Rect/BGRA, `glGetTexImage`, copia de bloques comprimidos y decodificador GPU |
| `gl31_fbo.c` | Framebuffers y renderbuffers, mapeo arbitrario de draw buffers (FBO proxy), lectura de píxeles en GPU |
| `gl31_shader_glsl.c` | Conversor GLSL de escritorio → GLSL ES (incluye reescritura de samplers 1D/Rect, wrappers del VS/FS) |
| `gl31_shader_link.c`, `gl31_uniform.c` | Linkeo de programas, uniforms, bloques, `glProgramUniform*`, `glShaderStorageBlockBinding` |
| `gl31_draw.c`, `gl31_idxgen.c`, `gl31_idx.h` | Draw calls, base vertex/instance, indirectos; kernels de índices en GPU (primitive restart, polygon mode, provoking vertex) |
| `gl31_compute.c` | Compute, imágenes, barreras |
| `gl31_xfb.c`, `gl31_query.c`, `gl31_sync.c` | Transform feedback, queries (incl. tiempo y primitivas generadas), sync |
| `gl31_gl4.c` | Funciones de GL 4.x (pipelines, texture view, multi-bind, debug…) |
| `gl31_dsa.c` | Direct State Access |
| `gl31_vk.c`, `gl31_vk.h` | Puente con Vulkan: fence nativo de EGL → sync fd, tabla de queries de tiempo |
| `gl31_stubs.c` | Funciones que GLES no puede ofrecer (avisan una sola vez), `glGetBufferSubData`, `glPolygonMode`, `glClampColor` |

Todas las llamadas al backend pasan por la macro `BE(fn)` (`gl31_be.fn`). El estado vive en
almacenamiento local de hilo (`__thread`).

### Terceros y licencias

- **Lorica**: GPL-3.0 (`LICENSE`).
- **gl4es** (Sebastien Chevalier y Ryan Hileman): MIT; conserva su copyright (`NOTICE/LICENSE-MIT.txt`).
- **Headers de Khronos** en `src/gl/GLADIATOR/third_party/khronos/`: copia literal, sin modificar, de
  `GLES3/gl3.h`, `GLES3/gl3platform.h`, `GLES2/gl2.h`, `GLES2/gl2platform.h` (OpenGL-Registry, `api/`) y
  `KHR/khrplatform.h` (EGL-Registry, `api/KHR/`). Licencia MIT, indicada en cada cabecera.
- `debian/` es el empaquetado heredado de gl4es.

---

## 2. Variables de entorno

### Versión y superficie

| Variable | Efecto |
|---|---|
| `LIBGL_GL` | Tope de versión de GL por proceso (`33`, `43`…). Los contextos legacy siguen recibiendo 2.1 |
| `LORICA_GL_MAX_VERSION` | Igual, con formato `4.3`; si está definida, manda sobre `LIBGL_GL` |
| `LIBGL_GL_COMPAT` | Versión de los contextos que no piden una |
| `LIBGL_FB` | Modo de superficie: 0 ventana directa, 1 framebuffer, 2 framebuffer + FBO (**desactiva GLADIATOR**), 3 pbuffer, 4 GBM (ver sección 7) |
| `LIBGL_EGL`, `LIBGL_GLES` | Ruta de `libEGL` y de la librería GLES reales |

### Depuración y diagnóstico

| Variable | Efecto |
|---|---|
| `LIBGL_DEBUG` / `LORICA_DEBUG` | `gl31_init` imprime por `stderr` la línea `[caps]` con las capacidades detectadas |
| `LORICA_TRACE_ERR=1` | Vuelca la pila en el punto donde GLADIATOR genera un error GL |
| `LORICA_DISABLE_CAPS=lista` | Apaga capacidades del backend para probar rutas de respaldo o emular un driver más limitado. Nombres, separados por comas: `es31`, `copy_image`, `draw_buf_indexed`, `sample_shading`, `multisample_tex`, `ms_array`, `tess`, `geometry`, `texture_view`, `cube_array`, `dual_src` |
| `LIBGL_BANNER=0` | Oculta el banner (solo se muestra si `stderr` es una terminal); `NO_COLOR` lo deja sin colores |
| `LIBGL_FPS` | Contador de cuadros por segundo en pantalla |

### Apoyo de Vulkan y memoria

| Variable | Efecto |
|---|---|
| `LORICA_VK=0` | Desactiva por completo el apoyo de Vulkan |
| `LORICA_VK_LIB=ruta` | Fuerza la `libvulkan` a cargar (p. ej. SwiftShader en contenedores sin GPU) |
| `LORICA_VK_DEBUG=1` | Traza del apoyo de Vulkan |
| `LORICA_VK_TIMER=1` | Ignora `EXT_disjoint_timer_query` y mide los timers con timestamps de Vulkan |
| `LORICA_BUFSTORE=native\|vk` | Fuerza la ruta de `glBufferStorage`: nativa (`EXT_buffer_storage`) o memoria de Vulkan |
| `LORICA_COMPRESSED_SHADOW_MB` | Presupuesto de memoria para las copias de bloques de texturas comprimidas (256 por defecto, ver sección 4) |

### Presentación cero-copia y otras (solo existen en el código)

`LIBGL_AHB`, `LIBGL_AHB_SWIZZLE`, `LIBGL_AHB_SIGNALS` (presentación con `AHardwareBuffer`), `LIBGL_OCCL`,
`LIBGL_OCCL_FALLBACK`, `LIBGL_STRINGS`, `LIBGL_GL4ES_COMPAT`, `LIBGL_FBO`. Su semántica detallada sigue
pendiente de documentar. Además, el núcleo gl4es hereda las variables `LIBGL_*` de upstream. La lista sale de
buscar `getenv` en el código.

---

## 3. Funciones implementadas

La capa GLADIATOR (contextos core GL ≥ 3.1) publica **572 funciones** en su tabla de despacho (`gl31.c`), y el
núcleo legacy (contextos GL 1.x–2.1) publica **640 nombres distintos** (ver «Núcleo legacy»). Lo que GLES no
puede expresar está en la sección 5. Los índices completos están en los apéndices A y B.

### Resumen por área

| Área | Estado | Cómo se resuelve |
|---|---|---|
| Buffers (VBO, UBO, copy, map, `glBufferStorage`) | Nativo / condicionado | `glBufferStorage` con almacenamiento inmutable y mapeo `PERSISTENT`/`COHERENT`: nativo con `EXT_buffer_storage`; sin ella, memoria de Vulkan importada por `EXT_memory_object_fd`; sin ninguna, `INVALID_OPERATION`. `glGetBufferSubData` emulado (lectura por buffer temporal si el almacenamiento es inmutable) |
| Vertex arrays y atributos | Nativo / emulado | Incluye el modelo `glVertexAttribFormat/Binding`, `glBindVertexBuffer(s)`, `glVertexBindingDivisor`; sin la capacidad nativa se emula con `glVertexAttribPointer` + divisor |
| Texturas 2D, 3D, arrays, cube map | Nativo | Cube map array con ES 3.2 o extensión |
| Texturas 1D, 1D array, RECTANGLE | Emulado | Como 2D / 2D array |
| Texturas multisample | Nativo en ES ≥ 3.1 | Multisample 2D array con ES 3.2 u `OES_texture_storage_multisample_2d_array`; en ES 3.0 da error con aviso |
| Samplers | Nativo / emulado | Todos los `glSamplerParameter*`; `Iiv`/`Iuiv` se guardan por sampler y sin `border_clamp` se muestrea con `CLAMP_TO_EDGE` |
| `GL_TEXTURE_SWIZZLE_RGBA` | Emulado | Se expande a los cuatro `SWIZZLE_R/G/B/A` |
| Texture buffer (`glTexBuffer[Range]`, `samplerBuffer`) | Condicionado | ES 3.2 nativo; ES 3.1 con `EXT/OES_texture_buffer`; ES 3.0 no |
| Framebuffers y renderbuffers | Nativo | Framebuffer 0 = superficie EGL (ver sección 1) |
| `glDrawBuffer(COLOR_ATTACHMENTi)`, `glDrawBuffers` con mapeo no identidad | Emulado | FBO proxy (ver sección 4) |
| `glBlitFramebuffer`, `glInvalidate*`, `glClearBuffer*` | Nativo | |
| `glClearBufferData/SubData` | Emulado | Formatos R/RG/RGB/RGBA de 8/16/32 bits (unorm, snorm, int, uint, float) desde cualquier `format/type` |
| `glReadPixels`, `glGetTexImage` | Emulado en GPU | Motor de transferencia de píxeles (ver sección 4) |
| `glGetCompressedTexImage`, `glGetTexImage` de comprimidas | Emulado en GPU | Copia de bloques en GPU + decodificador compute (ver sección 4) |
| Shaders y programas | Nativo + conversor GLSL | Ver «Conversor GLSL» |
| Uniforms, UBO, `glProgramUniform*` (33) | Nativo | Bloques de uniforms completos |
| Program pipelines, `glCreateShaderProgramv`, program binary, program interface query | Nativo en ES ≥ 3.1 | |
| Compute, `glDispatchCompute*`, SSBO, atomic counters, image load/store | Nativo en ES ≥ 3.1 | Formatos `image*` limitados a los de ES |
| `glShaderStorageBlockBinding` | Emulado (ES 3.1) | El conversor da un `layout(binding=N)` único a los bloques SSBO que no lo traen y antes de cada draw/dispatch se enlaza el buffer que la app puso en ese binding |
| `glMemoryBarrier[ByRegion]`, `glBindImageTexture(s)` | Nativo en ES ≥ 3.1 | |
| `glGetActiveAtomicCounterBufferiv` | Emulado | Sobre la interfaz `ATOMIC_COUNTER_BUFFER` de ES 3.1 |
| Draw instanciado, rangos, `MultiDraw*` | Nativo | |
| `glDrawElements*BaseVertex` | Emulado en ES < 3.2 | Desplaza punteros de atributos; nativo con ES 3.2 / `EXT_draw_elements_base_vertex` |
| Base instance | Emulado | Mueve offsets de buffers; avisa si no es múltiplo del divisor |
| Draws indirectos | Nativo en ES ≥ 3.1 | `MultiDraw*Indirect` emulado con un bucle |
| `glPrimitiveRestartIndex` (cualquier índice) | Emulado en GPU | Compute reescribe los índices |
| `glPolygonMode(LINE/POINT)` | Emulado en GPU | Compute convierte triángulos en líneas/puntos |
| `glProvokingVertex` | Nativo / en GPU | `FIRST` con `EXT/ANGLE_provoking_vertex`; sin ella, cada primitiva se rota en la GPU |
| `glClipControl` | Emulado | `ZERO_TO_ONE` exacto en el VS (`z' = 2z - w`); `UPPER_LEFT` invierte Y y el sentido de las caras |
| `glPointSize`, origen de point sprite | Emulado | Reescritura GLSL con uniforms propios |
| `GL_DEPTH_CLAMP`, `GL_CLIP_DISTANCEi` | Nativo / emulado | Con `EXT_depth_clamp` / `EXT_clip_cull_distance`; sin ellas, reescritura GLSL |
| Tessellation (`GL_PATCHES`, `glPatchParameteri`) | Condicionado | Requiere tessellation en el backend |
| Geometry shaders y primitivas con adyacencia | Condicionado | ES 3.2 o `EXT/OES_geometry_shader` |
| Transform feedback y objetos de TF | Nativo | `glDrawTransformFeedback*` con cuenta propia de vértices capturados |
| Queries de oclusión y `GL_PRIMITIVES_GENERATED` | Nativo / emulado | `ARB_occlusion_query2` ya viene en ES 3.0; las primitivas generadas se cuentan por draw |
| Queries de tiempo (`GL_TIME_ELAPSED`, `glQueryCounter`, `glGetInteger64v(GL_TIMESTAMP)`) | Condicionado | Nativo con `EXT_disjoint_timer_query`; sin ella, timestamps de Vulkan 1.1 tras un fence nativo de GL; sin Vulkan devuelve 0 con aviso |
| Sync objects | Nativo | |
| Blend por buffer (`glBlendFunci`…), `glEnablei`, `glColorMaski` | Condicionado | Con `draw_buffers_indexed`; sin ella se guarda por buffer y se aplica el del primer buffer activo |
| Blend de doble fuente (`GL_SRC1_*`, `layout(index=1)`) | Condicionado | Con `EXT_blend_func_extended`; sin ella `GL_INVALID_OPERATION` |
| `glVertexAttribP*ui` (2_10_10_10) | Emulado | Desempaquetado en CPU hacia `glVertexAttrib4f` |
| Sample shading, `glMinSampleShading`, `glGetMultisamplefv` | Condicionado / emulado | `OES_sample_shading` o ES 3.2; sin ellas se registran y se sombrea por píxel; posiciones estándar de Vulkan para 1/2/4/8/16 muestras |
| `glBindFragDataLocation` | Emulado | Reconvierte el fragment shader con `layout(location=N)` al linkear |
| Subida `GL_BGRA` | Emulado | Kernel de compute con ES 3.1 (con o sin PBO, con `UNPACK_*`); en ES 3.0, intercambio R/B en CPU |
| Texture view, `glCopyImageSubData` | Condicionado / emulado | ES 3.2 o `EXT/OES_texture_view` / `EXT_copy_image`; sin ellas, `glCopyImageSubData` copia en GPU con `glBlitFramebuffer` |
| Multi-bind (`glBindBuffersBase/Range`, `glBindTextures`, `glBindSamplers`, `glBindImageTextures`, `glBindVertexBuffers`) | Nativo | |
| KHR_debug (`glDebugMessage*`, `glPushDebugGroup`, labels) | Condicionado | Requiere `KHR_debug` en el backend |
| DSA (`glCreate*`, `glNamed*`, `glTexture*`, `glVertexArray*`…) | Emulado | Ver «Direct State Access» |

### Conversor GLSL (`gl31_shader_glsl.c`)

- **Entradas aceptadas:** `#version` 110–150, 330 y 400–460; etapas vertex, fragment, geometry, compute,
  tess control y tess evaluation.
- **Salida:** `#version 300 es`, `310 es` o `320 es` según el backend. Se inyectan calificadores de
  precisión y los `#extension` EXT/OES que el backend exija por debajo de ES 3.2.
- **Samplers:** `sampler1D`, `sampler1DArray` y `sampler2DRect` (y variantes `i`/`u`) pasan a `sampler2D` /
  `sampler2DArray`. `texture`, `textureLod`, `texelFetch` y `textureSize` sobre ellos se redirigen a
  funciones `lorica_*` inyectadas (coordenada extra 0.5; en Rect, `p / textureSize`). `glGetActiveUniform`
  informa el tipo de escritorio (`GL_SAMPLER_1D`…).
- **Tipos de doble precisión:** `double`, `dvecN`, `dmatN` pasan a `float`, `vecN`, `matN` (**se pierde
  precisión**).
- **Se conservan** (en ES ≥ 3.1): `binding`, `offset`, `early_fragment_tests` y `location` entre etapas.
- **Se eliminan:** `gl_PerVertex` redeclarado y `precise` (si ES < 3.2).
- **Se rechazan con error claro:** `subroutine`, `image1D`, `sampler1DShadow`, `sampler2DMSArray` (en ES 3.0)
  y los usos no soportados de samplers 1D/Rect (ver sección 5).
- Los uniforms `lorica_vs`, `lorica_vs2` y `lorica_fs` que Lorica inyecta en los shaders se ocultan a la app
  (`glGetActiveUniform`, `glGetUniformIndices`, `GL_ACTIVE_UNIFORMS`).

### Direct State Access (`gl31_dsa.c`)

ES no tiene acceso por nombre. Cada función DSA hace: enlazar el objeto → llamar a la función clásica ya
traducida → restaurar el enlace anterior. El estado enlazado que ve la app no cambia.

- Cubierto: texturas, buffers, framebuffers, renderbuffers, vertex arrays, samplers, queries, program
  pipelines y transform feedback (`glCreate*`, `glNamed*`, `glTexture*`, `glVertexArray*`,
  `glTransformFeedbackBuffer*`, `glGetGraphicsResetStatus`, que siempre devuelve `GL_NO_ERROR`).
- Los objetos creados con `glCreate*` quedan con tipo definido (en ES el tipo lo fija el primer enlace). Una
  textura generada solo con `glGenTextures` da `GL_INVALID_OPERATION` en las funciones `glTexture*`.
- Se publica como extensión `GL_ARB_direct_state_access` cuando la versión anunciada es ≥ 3.3.

### Contextos y GLX

Publicados por el front-end GLX: `glXCreateContext`, `glXCreateContextAttribsARB`, `glXMakeCurrent`,
`glXSwapBuffers`, `glXGetProcAddress`, `glXChooseFBConfig`, `glXGetVisualFromFBConfig` y el resto de la API
GLX que necesitan SDL 2, GLFW y cualquier app que hable con X11 (el script de instalación exige al menos 40
símbolos `glX*` exportados).

### Núcleo legacy (contextos GL 1.x–2.1)

Los contextos legacy (perfil compatibility o sin versión) los atiende el núcleo de `src/gl/`, que
implementa la API de función fija y GLSL de GL 2.1 sobre ES 2.0. Es parte de Lorica igual que GLADIATOR: el
puente decide qué capa atiende cada contexto y ambas se publican a través de `glGetProcAddress`. El núcleo
legacy publica **640 nombres de función distintos** (cada uno además con sus alias `ARB` / `EXT` cuando
existen); algunos se registran solo si el backend ofrece la capacidad (por ejemplo, los de framebuffer
objects). El listado está en el apéndice B y la fuente de verdad es `src/gl/gl_lookup.c` y
`src/gl/glesfuncs.inc`.

| Área | Módulos | Qué cubre |
|---|---|---|
| Pipeline de función fija | `fpe.c`, `fpe_shader.c`, `fpe_cache.c` | Genera y cachea los shaders de ES 2.0 que reproducen la función fija |
| Modo inmediato y listas | `list.c`, `listdraw.c`, `listrl.c`, `render.c` | `glBegin`/`glEnd`, display lists |
| Evaluadores | `eval.c` | `glMap*`, `glEvalCoord*`, `glEvalMesh*` |
| Matrices y pilas | `matrix.c`, `matvec.c`, `stack.c` | Pilas de matrices, `glPushAttrib` / `glPopAttrib` |
| Iluminación, niebla, texenv, texgen | `light.c`, `fog.c`, `texenv.c`, `texgen.c` | Material, luces, niebla, `glTexEnv*`, `glTexGen*` |
| Primitivas clásicas | `line.c`, `pointsprite.c`, `planes.c`, `raster.c` | `glLineStipple`, puntos y point sprites, planos de recorte, `glBitmap`, `glDrawPixels` |
| Programas ARB y shaders | `arbparser.c`, `arbgenerator.c`, `arbconverter.c`, `oldprogram.c`, `shaderconv.c`, `preproc.c` | Programas `ARB_vertex_program` / `ARB_fragment_program`, conversión y preprocesado de GLSL |
| Texturas | `texture*.c`, `decompress.c`, `stb_dxt_104.h` | Subida, compresión y descompresión, parámetros |
| Framebuffers, buffers, estado | `framebuffers.c`, `buffers.c`, `array.c`, `drawing.c`, `getter.c`, `glstate.c` | FBO, VBO, arrays, draws, consultas |

Las limitaciones de este núcleo son las del backend ES 2.0 sobre el que corre y no se repiten en la sección
5, que trata de GLADIATOR.

---

## 4. GPU en lugar de CPU y apoyo de Vulkan

### Principio

GLES es la base. Vulkan 1.1+ (sin extensiones opcionales) es un coprocesador de GPU para lo que GLES no
puede expresar. Si ninguno de los dos puede por sí solo, se combinan; la CPU solo se usa si no existe camino
de GPU. Solo se usan extensiones de **plataforma** imprescindibles para compartir con GL
(`external_memory_fd`/AHB, `external_semaphore_fd`, `external_fence_fd`), detectadas en ejecución.

**Límite estructural**: no hay compilador GLSL→SPIR-V, así que Vulkan no puede ejecutar los shaders de la
aplicación. Vulkan ejecuta solo trabajo propio de Lorica (timestamps, copias, kernels SPIR-V propios) sobre
memoria compartida con GLES. Lo que se puede resolver reescribiendo GLSL se resuelve en GLES.

### Piezas

| Fichero | Función |
|---|---|
| `src/vk/lvk_types.h` | Subconjunto ABI-exacto de Vulkan 1.1 (sin depender de `vulkan.h`) |
| `src/vk/lvk.[ch]` | Carga por `dlopen`, instancia/dispositivo, buffers exportables, `lvk_run`, timestamps |
| `src/gl/GLADIATOR/gl31_vk.[ch]` | Puente: fence nativo de EGL → sync fd, tabla de queries de tiempo |
| `src/gl/GLADIATOR/gl31_idxgen.c` | Kernels de compute que reescriben índices (restart, polygon mode, provoking vertex) |
| `tools/build_linux.sh`, `tests/hybrid/run.sh` | Compilación y pruebas reproducibles |

### Rutas de GPU (requieren ES 3.1 salvo indicación; sin ello quedan las rutas de CPU anteriores)

| Operación | Cómo se resuelve |
|---|---|
| `glReadPixels` de color, cualquier formato/tipo de GL (incl. empaquetados, `10F_11F_11F`, `5_9_9_9`, enteros, half, `SWAP_BYTES`, `CLAMP_READ_COLOR`, `PACK_*`) | Blit a textura temporal + kernel de compute; PBO por copia GPU→GPU |
| `glReadPixels` depth / stencil / depth-stencil | Igual (blit + compute) |
| `glGetTexImage` de profundidad / profundidad-stencil | Usa la lectura depth/stencil en GPU |
| `GL_PACK_IMAGE_HEIGHT` y `GL_PACK_SKIP_IMAGES` | Aplicados en `glGetTexImage` de 3D y arrays |
| Lecturas con un FBO interno | Van por el camino de GPU |
| Subida de texturas `GL_BGRA` | Kernel de compute, con o sin PBO |
| `glGetCompressedTexImage` | Cada nivel comprimido (por cara en cubos) guarda una copia de sus bloques en un buffer de GPU, actualizada en `glCompressedTexImage*/SubImage*`, `glTexStorage*` + SubImage y `glCopyImageSubData` sin pasar por la CPU. La lectura es una copia a un PBO o a la memoria de la app |
| `glGetTexImage` sobre formatos comprimidos | S3TC (DXT1/3/5, sRGB incluido), RGTC (BC4/BC5 con y sin signo), EAC R11/RG11 y ETC2 (RGB8, sRGB8, punchthrough, RGBA8 EAC): se descomprime cada capa en GPU (compute → textura RGBA32F) y se lee con el camino normal de `glReadPixels`. ASTC y BPTC (BC6H/BC7) dan aviso |
| `glCopyImageSubData` sin la capacidad nativa | Copia en GPU con `glBlitFramebuffer` (color y depth/stencil, 2D/array/3D/cubo/renderbuffer). Comprimidos y 1D array: aviso |
| `glGetTexLevelParameter(COMPRESSED_IMAGE_SIZE)` | Se calcula con la tabla de bloques (S3TC, RGTC, BPTC, ETC2/EAC, ASTC) |

El coste de la copia de bloques comprimidos es memoria extra igual al tamaño de las texturas comprimidas,
con tope `LORICA_COMPRESSED_SHADOW_MB` (256 por defecto). Pasado el tope, las texturas nuevas no se copian y
su lectura da `INVALID_OPERATION` con un aviso.

### Draw buffers arbitrarios (FBO proxy)

ES solo permite que la salida `j` del fragment shader vaya al attachment `j`. Para `glDrawBuffer(
GL_COLOR_ATTACHMENTi)` con `i>0` y `glDrawBuffers` con mapeo no identidad, Lorica dibuja en un **FBO proxy**
del backend cuyo attachment `j` es la imagen del attachment `k` del FBO de la app (más depth/stencil). Todo
lo que cambia o consulta attachments se redirige al FBO real y el proxy se reconstruye; `GL_DRAW_BUFFERi`
devuelve el mapeo de la app.

### Vulkan: qué resuelve

| Función | Ruta |
|---|---|
| `GL_TIME_ELAPSED`, `glQueryCounter`, `glGetInteger64v(GL_TIMESTAMP)` sin `EXT_disjoint_timer_query` | GLES + `vkCmdWriteTimestamp` tras fence GL |
| `glBufferStorage` inmutable + `PERSISTENT`/`COHERENT` sin `EXT_buffer_storage` | Vulkan host-coherente por fd → `EXT_memory_object_fd` |
| `glFinish` | Espera además los marcadores de Vulkan pendientes |

### Capacidades del backend que se pueden apagar para probar

`LORICA_DISABLE_CAPS` (ver sección 2) deja ejercitar las rutas de respaldo con un driver capaz; las pruebas
de `tests/hybrid/test_fallback.c` lo usan.

### No verificado en hardware (solo compilado / probado en software)

- Sincronización GL→Vulkan por **native fence** (`eglCreateSyncKHR` + `eglDupNativeFenceFDANDROID` + import
  de semáforo `SYNC_FD`); en las pruebas de software se usa `glFinish`.
- Rutas con AHB, ruta Vulkan de buffers (llvmpipe no importa buffers), y las extensiones
  `EXT_clip_cull_distance` / `EXT_depth_clamp` reales de Mali.

---

## 5. No publicado, stubs y limitaciones

### 5.1 No publicado (la app recibe `NULL`)

GLES no puede expresar estas funciones, así que `glGetProcAddress` devuelve `NULL` y las apps pueden
detectar que no existen.

| Función / característica | Motivo |
|---|---|
| Bindless textures, sparse residency | Fuera del alcance de ES |
| `glUniform1d` y resto de uniforms de doble precisión | ES no tiene fp64 |
| `glUniformSubroutinesuiv` (subroutines) | Sin equivalente; `subroutine` en GLSL se rechaza |
| `glTextureBarrier` | Sin camino actual |
| `glGetTextureSubImage` | No implementado |
| `glGetTransformFeedback*` (getters de objetos de TF) | No implementado |
| `glGetVertexArrayIndexed64iv` | No implementado |
| `glGetnUniform*` | No implementado |
| Funciones de compatibilidad (`glVertexP*ui`, `glColorP*ui`, `glTexCoordP*ui`…) | No existen en perfil core |
| Versiones 4.4 – 4.6 | Faltan `ARB_query_buffer_object`, `ARB_clear_texture`, `ARB_enhanced_layouts`, mirror-clamp |

### 5.2 Stubs, emulaciones parciales y funciones con aviso

Estas funciones existen, pero no hacen exactamente lo mismo que en GL de escritorio. Cada aviso sale una sola
vez por `stderr` (`LORICA GL31: … no soportado sobre GLES (ignorado)`), y si el resultado sería incorrecto se
genera además un error GL.

| Función | Comportamiento |
|---|---|
| `glLogicOp` | Sin efecto (ES no lo tiene) |
| `glPolygonMode` | `LINE`/`POINT` en GPU. Con cull face se dibujan los bordes de todos los triángulos; no aplica a draws indirectos |
| `glClampColor` | `CLAMP_READ_COLOR` aplicado en `glReadPixels` (buffers float); `CLAMP_VERTEX/FRAGMENT_COLOR` se registran (ES no recorta atributos ni color float) |
| `glPointSize` | Wrapper del VS. No aplica a programas con geometry/tessellation ni a pipelines separables |
| `glClipControl` | No remapea viewport, scissor ni `gl_FragCoord` |
| `glPixelStorei(UNPACK_SWAP_BYTES / LSB_FIRST)` | Se registran; `PACK_SWAP_BYTES` se aplica en `glReadPixels`. `LSB_FIRST` solo afecta a bitmaps (no existen en ES) |
| `glPrimitiveRestartIndex` | Cualquier índice en draws normales; en draws indirectos solo el índice fijo |
| `glProvokingVertex(GL_FIRST_VERTEX_CONVENTION)` | Nativo con `EXT/ANGLE_provoking_vertex`; si no, en GPU (tope de 8192 vértices para la paridad de strips con restart). No aplica a draws indirectos |
| `glReadPixels` con ES 3.0 | Ruta de CPU: tipos empaquetados (4_4_4_4, 5_5_5_1, 2_10_10_10_REV…) dan aviso |
| `glFramebufferTexture` sin geometry shaders | Adjunta solo la capa 0 |
| `glBufferStorage(PERSISTENT / COHERENT)` | Solo con `EXT_buffer_storage` o (`EXT_memory_object_fd` + Vulkan 1.1); si no, `INVALID_OPERATION` |
| `glPatchParameterfv` | No aplica niveles por defecto: usar un tessellation control shader |
| `glPatchParameteri` | Aviso si el backend no ofrece tessellation |
| `glColorMaski`, `glEnablei`, `glBlendFunci/EquationSeparatei` | Sin `draw_buffers_indexed`: se guardan por buffer y se aplica el del primer buffer activo (se reaplica al cambiar de FBO/draw buffers). Aviso solo si dos buffers activos difieren |
| `glTextureView` | Aviso sin ES 3.2 / extensión |
| `glCreateShaderProgramv`, `glSampleMaski` | Aviso sin ES 3.1; `glSampleMaski` acepta la máscara que deja pasar todo, una restrictiva avisa |
| `glQueryCounter(GL_TIMESTAMP)` | Sin `EXT_disjoint_timer_query` y sin Vulkan 1.1 con timestamps: resultado 0 |
| `glVertexAttribLFormat` | fp64 no soportado |
| `glCopyTexImage2D(1D_ARRAY)`, `glCompressedTexImage2D(RECTANGLE/1D_ARRAY)` | Casos no soportados |
| `glGetTexImage` | Formatos no renderizables: aviso. Comprimidos ASTC/BPTC: aviso |
| `glGetCompressedTextureImage`, `glCompressedTextureSubImage3D` sobre cube map | `GL_INVALID_OPERATION` |
| `glDrawTransformFeedback*` | Stream 0. Con geometry/tessellation necesita que la app no tenga activa una query `TF_PRIMITIVES_WRITTEN` |
| Dual-source blending sin `EXT_blend_func_extended` | `GL_INVALID_OPERATION`; el `layout(index)` se descarta |
| `glCreateShader(GL_COMPUTE_SHADER / TESS_*)` | Se aceptan si el backend los ofrece |

### 5.3 Limitaciones de lo que sí está implementado

**Límites numéricos y de estado**
- `GL_MAX_VIEWPORTS` = 1: viewport, scissor y depth range con arrays solo usan el índice 0.
- `GL_MAX_DUAL_SOURCE_DRAW_BUFFERS` = 1.
- `glBeginQueryIndexed` / `glEndQueryIndexed` / `glGetQueryIndexediv`: solo índice 0.
- `GL_TEXTURE_CUBE_MAP_SEAMLESS` se guarda como estado «blando» (en ES las cubemaps ya son siempre
  continuas).
- `GL_DEPTH_CLAMP`: nativo con `EXT_depth_clamp`; sin ella se sujeta la `z` de clip en el VS (aproximación
  por vértice: correcta salvo en primitivas que cruzan el plano near/far).
- `GL_CLIP_DISTANCEi` / `gl_ClipDistance`: con `EXT_clip_cull_distance` el wrapper del VS anula los planos
  no habilitados; sin ella se emula con un varying y `discard` en el FS (hasta 8 planos; no con
  geometry/tessellation).
- 64 bits: `double` se degrada a `float`.

**Texturas**
- Las texturas multisample usan almacenamiento inmutable de ES: volver a llamar `glTexImage2DMultisample`
  sobre la misma textura da `GL_INVALID_OPERATION` (en GL escritorio es legal).
- `GL_BGRA` en subidas con ES 3.0 solo con `GL_UNSIGNED_BYTE` / `UNSIGNED_INT_8_8_8_8_REV` y sin PBO de
  desempaquetado.
- Compresión RGTC (BC4/BC5) y S3TC solo si el backend las ofrece; no se transcodifican.
- Cube maps en DSA: `glTextureSubImage3D`, `glCopyTextureSubImage3D` y `glGetTextureImage` reparten en las 6
  caras (sin `GL_UNPACK_IMAGE_HEIGHT`).
- Texture view y copy image nativos requieren ES 3.2 o la extensión EXT/OES correspondiente.

**Dibujo**
- Base vertex emulado (ES < 3.2): modifica y restaura los punteros de atributos en cada draw (costo extra de
  CPU) y solo cubre atributos con buffer y sin divisor.
- Base instance emulada: avisa si no es múltiplo del divisor de un atributo.
- `MultiDraw*Indirect` emulado con bucle; `baseVertex`/`baseInstance` dentro de la estructura indirecta los
  resuelve el backend.
- Formatos `image*` sujetos a ES (`r32f/r32i/r32ui` y `rgba*`).

**GLSL**
- Sobre samplers 1D/Rect no se admiten `textureProj*`, `textureGrad`, `textureOffset`, `textureGather` ni
  sombras 1D/Rect (error de compilación con mensaje claro). `textureLod` sobre Rect no existe en GL.
- Los helpers de sampler solo se aplican cuando el sampler se declara **en el mismo shader**: un sampler 1D
  pasado a una función de otro shader no se detecta.
- Geometry shaders: el conversor solo ajusta versión, extensiones y layouts; las diferencias semánticas
  (límites de salida, bloques de interfaz) las valida el compilador del driver.
- `subroutine`, `image1D`, `sampler1DShadow`, `sampler2DMSArray` (ES 3.0) se rechazan.

**Contextos, hilos y framebuffer**
- El estado se **comparte entre contextos** para shaders, programas, texturas y VAOs, y **no tiene locking**.
- El despacho de símbolos enlazados es global, igual que `glxContext`: con varios hilos activos sobre
  contextos de distinto tipo (core y legacy), gana el último `MakeCurrent`.
- `LIBGL_FB=2` no es compatible con contextos GL 3+: el puente cae a ES 2 y GLADIATOR queda inactivo.
- El despacho por trampolines solo existe en Linux x86_64 y aarch64, y no en builds `-DNOX11`.

---

## 6. Qué exige cada versión de ES

| Función | ES 3.0 | ES 3.1 | ES 3.2 / extensiones |
|---|---|---|---|
| Compute, SSBO, imágenes, draw indirecto, pipelines | No | Sí | Sí |
| Rutas de GPU para píxeles, BGRA, decodificador comprimido, restart/polygon/provoking | No (queda la ruta de CPU) | Sí | Sí |
| Texturas multisample 2D, `glSampleMaski` | No | Sí | Sí |
| Multisample 2D array | No | Solo con `OES_texture_storage_multisample_2d_array` | Sí |
| Texture buffer | No | Con `EXT/OES_texture_buffer` | Sí |
| Geometry shaders, adyacencia, `glFramebufferTexture` | No | Con `EXT/OES_geometry_shader` | Sí |
| Tessellation | No | Con extensión | Sí |
| `gl_ClipDistance` | Emulado (varying + discard) | Con `EXT_clip_cull_distance` | Con la extensión |
| Base vertex nativo | Emulado | Emulado | Nativo |
| Texture view, copy image, cube map array | No | Con extensión | Sí |
| `glProvokingVertex` FIRST | Solo con `EXT/ANGLE_provoking_vertex` | En GPU (compute ES 3.1) | Con `EXT/ANGLE_provoking_vertex` o en GPU |

Sigue sin implementarse, por ser un límite real del backend y no deuda de código: compute/SSBO/atómicos/
draw indirecto sin ES 3.1, tessellation, geometry shaders, dual-source blending, texture buffers, cube map
arrays y texturas multimuestra sin extensión, `glTextureView`, programas separables sin ES 3.1 y fp64.

La versión anunciada por backend está en la sección 1.

---

## 7. EGL y GLX

Lorica usa EGL y GLX en **dos direcciones opuestas**. No hay que confundirlas:

```
Dirección A (libGL.so.1, siempre)          Dirección B (libEGL.so.1, opcional)

 App X11/GLX                                App que pide OpenGL por EGL
     │ glX*                                     │ egl*  (EGL_OPENGL_API)
     ▼                                          ▼
 Lorica: front-end GLX  (src/glx)           Lorica: wrapper EGL  (src/egl)
     │ egl*                                     │ glX*
     ▼                                          ▼
 EGL REAL del dispositivo                   Lorica: front-end GLX (dirección A)
 (Android, Mesa, Scutum…)                       ▼
     ▼                                      EGL real → GLES
 GLES
```

Si las dos están instaladas, una app EGL pasa por B y luego por A: *EGL (Lorica) → GLX (Lorica) → EGL real*.

---

### A. GLX → EGL (`src/glx/glx.c`)

Es la parte que ya usan las apps GLX normales (SDL, GLFW, Red Eclipse…). El EGL que usa es el **real**
del sistema, cargado con `LOAD_EGL` desde `libEGL` (ruta forzable con `LIBGL_EGL`; la librería GLES con
`LIBGL_GLES`).

#### Arranque
1. `init_display`: `eglGetDisplay(EGL_DEFAULT_DISPLAY)`. Con GBM (`LIBGL_FB=4`) el display sale de
   `OpenGBMDisplay`; con HYBRIS, de `eglGetPlatformDisplay(ANDROID)`.
2. `InitEGL`: `eglBindAPI(EGL_OPENGL_ES_API)` + `eglInitialize`. Se hace una sola vez.
3. `init_eglconfig`: pide a EGL **todas** las configs con `EGL_RENDERABLE_TYPE = ES2` y construye la
   tabla de `GLXFBConfig`. Cada `EGLConfig` genera **dos** entradas (sin doble buffer y con doble
   buffer), indexadas por `EGL_CONFIG_ID` y `EGL_CONFIG_ID + max`.

#### Correspondencia de funciones

| GLX | EGL |
|---|---|
| `glXChooseFBConfig`, `glXGetFBConfigs`, `glXGetFBConfigAttrib` | `eglChooseConfig`, `eglGetConfigAttrib` (tamaños R/G/B/A, depth, stencil, samples; `EGL_SURFACE_TYPE` → `GLX_WINDOW/PIXMAP/PBUFFER_BIT`; `EGL_NATIVE_VISUAL_ID` → visual X11) |
| `glXChooseVisual`, `glXGetVisualFromFBConfig` | Visual X11 asociado al `EGLConfig` (si no hay, el `TrueColor` por defecto de la pantalla) |
| `glXCreateContext`, `glXCreateNewContext`, `glXCreateContextAttribsARB` | `eglChooseConfig` + `eglCreateContext`; el contexto compartido (`shareList`) se pasa como `share_context` |
| `glXDestroyContext` | `eglDestroyContext` (+ liberación de la superficie si no se comparte) |
| `glXMakeCurrent` | Crea la `EGLSurface` si no existe y llama a `eglMakeCurrent(dpy, surf, surf, ctx)` |
| `glXSwapBuffers` | `eglSwapBuffers`, o la copia de pbuffer a ventana (ver modos) |
| `glXSwapIntervalEXT/SGI/MESA` | `eglSwapInterval` |
| `glXCreatePbuffer` / `glXDestroyPbuffer` | `eglCreatePbufferSurface` / `eglDestroySurface` |
| `glXCreatePixmap`, `glXCreateGLXPixmap` | Emulados con un pbuffer + `XImage` (`BlitEmulatedPixmap`) |
| `glXQueryDrawable` | `eglQuerySurface` (ancho/alto) |
| `glXCopyContext`, `glXUseXFont`, … | Resueltos en Lorica (sin equivalente EGL) |

El `GLXContext` que recibe la app es un objeto propio (`struct __GLXContextRec`) que guarda el
`EGLContext`, la lista de configs candidatas, el índice de la elegida, el `EGLSurface` actual, la
versión y perfil de GL pedidos y la versión de ES que se logró (`es_major`, `es_minor`).

#### Versión de ES del contexto de backend
- **Contexto GL ≥ 3.1** (`glXCreateContextAttribsARB`): `gl4es_gl31_create_backend_context` prueba ES 3.2,
  luego 3.1, luego 3.0 sobre una config con el bit ES3; si ninguna funciona cae a ES 2 y GLADIATOR queda
  inactivo (con un `LOGE`). Detalle en la sección 1.
- **Contexto legacy** (`glXCreateContext`): `eglCreateContext` con `EGL_CONTEXT_CLIENT_VERSION` = 3 si
  `LIBGL_GL ≥ 31` o existe `LORICA_GL_MAX_VERSION`, y 2 en caso contrario. Se hace así a propósito: si el
  backend naciera en ES 2, `hardext` reportaría capacidades de ES 2 (1 draw buffer) mientras la app ve
  "GL 3.3" y asume los mínimos de GL 3.

#### Modos de superficie (`LIBGL_FB`)

| `LIBGL_FB` | Modo | Cómo se dibuja y se presenta |
|---|---|---|
| sin definir / 0 | Ventana X11 directa | `eglCreateWindowSurface` con la `Window` X como ventana nativa; `eglSwapBuffers` presenta |
| 1 | Framebuffer | Ventana nativa creada por Lorica (`create_native_window`); sin X |
| 2 | Framebuffer + FBO | Se dibuja en un FBO principal que se vuelca en cada swap. **Desactiva GLADIATOR** |
| 3 | **Pbuffer** | `eglCreatePbufferSurface` del tamaño de la ventana; en cada swap se hace `glFinish` y se copia a la ventana X (`glReadPixels` + `XPutImage`, o `eglCopyBuffers` si funciona). En Android se usa presentación cero-copia con `AHardwareBuffer` (`lorica_ahb_present`). Solo con X11 |
| 4 | GBM | Superficie sobre GBM/DRM |

En Android/Termux `lorica_android_defaults()` fija `LIBGL_FB=3` si no está definido: **por eso, en
Termux, la app nunca le entrega su ventana a EGL**; EGL dibuja en un pbuffer y Lorica lo copia a la
ventana.

Detalles del modo pbuffer:
- Al hacer `glXMakeCurrent` se crea (o se reutiliza) un pbuffer con una config con bit `PBUFFER`; si el
  contexto existente no coincide, se destruye y se sustituye por el que nace con el pbuffer.
- En cada swap, si la ventana cambió de tamaño, el pbuffer se recrea con el tamaño nuevo.
- Las superficies se reciclan por drawable (`RecycleGetSurface`) para no recrear `EGLSurface` en cada
  `glXMakeCurrent`; el contador compartido evita destruir una superficie que otro contexto aún usa.

#### Presentación (`glXSwapBuffers`)
1. Vuelca texturas pendientes, display lists y bitmaps de gl4es.
2. Con `LIBGL_FB=2`: desenlaza y vuelca el FBO principal.
3. Con pbuffer: `glFinish` + copia a la ventana. Si no: `eglSwapBuffers(display, surface)`.
4. Si `LIBGL_FPS` está activo, actualiza el contador en pantalla.

---

### B. EGL → GLX (`src/egl/`, opcional)

Es un `libEGL.so.1` propio que implementa la API de EGL llamando a `glX*` de Lorica. Sirve para
programas que piden **OpenGL de escritorio mediante EGL** (`eglBindAPI(EGL_OPENGL_API)`), por ejemplo
toolkits que usan EGL en X11.

#### Compilación y carga
- Se compila solo con `-DEGL_WRAPPER=ON` (target `EGL` en `src/CMakeLists.txt`, enlazado contra `GL`).
  `scripts/termux-install.sh` **no** lo pide: hay que añadir esa opción a `cmake`.
- Genera `lib/libEGL.so.1`. El binario que hay en `lib/` depende de `libGL.so.1` y de X11, como
  corresponde a este wrapper.
- Al primer uso (`pthread_once`) hace `dlopen("libGL.so.1")` y resuelve `glX*` con `dlsym` y
  `glXGetProcAddress`; las funciones de X11 salen de `libGL` o, si no, de `libX11`.
- ⚠ Conviene fijar `LIBGL_EGL` a la ruta absoluta del EGL real: si el cargador resolviera `libEGL` al
  propio wrapper, la dirección A intentaría usarlo como backend.

#### Correspondencia de tipos

| EGL | Es en realidad |
|---|---|
| `EGLDisplay` | `Display*` de X11 (`eglGetDisplay(NULL)` hace `XOpenDisplay(NULL)`) |
| `EGLConfig` | `GLXFBConfig` |
| `EGLContext` | `GLXContext` |
| `EGLSurface` (ventana) | El `XID` de la `Window` |
| `EGLSurface` (pbuffer) | El `XID` del `GLXPbuffer` con el bit 40 activo (`1UL << 40`) para distinguirlo |

#### Correspondencia de funciones

| EGL | Implementación sobre GLX |
|---|---|
| `eglGetDisplay`, `eglGetPlatformDisplay[EXT]` | `XOpenDisplay`; solo plataforma X11 (`EGL_PLATFORM_X11_KHR`) |
| `eglInitialize` / `eglTerminate` | No inicializan nada: informan EGL **1.5** y devuelven éxito |
| `eglQueryString` | Vendor `Lorica`, versión `1.5 Lorica`, clientes `OpenGL` (ver extensiones abajo) |
| `eglGetConfigs`, `eglChooseConfig` | `glXGetFBConfigs` de la pantalla por defecto; descarta configs sin id o sin bits RGB; filtra por los atributos pedidos (tamaños mínimos, `SURFACE_TYPE`, `RENDERABLE_TYPE`…) |
| `eglGetConfigAttrib` | `glXGetFBConfigAttrib` con traducción de atributos (tabla abajo) |
| `eglCreateWindowSurface`, `eglCreatePlatformWindowSurface[EXT]` | No crea nada: la superficie es el propio `XID` de la ventana (en la variante *Platform*, se lee el `Window` a través del puntero) |
| `eglCreatePbufferSurface` | `glXCreatePbuffer` con `GLX_PBUFFER_WIDTH/HEIGHT` |
| `eglDestroySurface` | `glXDestroyPbuffer` si es pbuffer; las ventanas no se destruyen |
| `eglQuerySurface` | Solo `EGL_WIDTH` / `EGL_HEIGHT`: `glXQueryDrawable` (pbuffer) o `XGetGeometry` (ventana) |
| `eglCreateContext` | Con versión mayor: `glXCreateContextAttribsARB` con `MAJOR/MINOR/FLAGS/PROFILE_MASK` traducidos; sin versión: `glXCreateNewContext(GLX_RGBA_TYPE)`. Config `NULL` → primera config válida |
| `eglDestroyContext` | `glXDestroyContext` (+ pbuffer interno del contexto) |
| `eglMakeCurrent` | `glXMakeCurrent(dpy, draw, ctx)`; sin superficie (*surfaceless*), crea un pbuffer 1×1 por contexto |
| `eglSwapBuffers` | `glXSwapBuffers` |
| `eglSwapInterval` | `glXSwapIntervalEXT` sobre la superficie actual, o `glXSwapIntervalSGI` |
| `eglWaitClient`, `eglWaitGL` / `eglWaitNative` | `glXWaitGL` / `glXWaitX` |
| `eglReleaseThread` | `glXMakeCurrent(dpy, 0, NULL)` |
| `eglGetProcAddress` | Funciones EGL propias; el resto de nombres va a `gl4es_GetProcAddress` (mismo resolvedor que `glXGetProcAddress`, con las reglas de GLADIATOR) |

#### Atributos de config (EGL → GLX)

| `EGL_…` | Origen |
|---|---|
| `BUFFER_SIZE`, `RED/GREEN/BLUE/ALPHA_SIZE`, `DEPTH_SIZE`, `STENCIL_SIZE` | Atributo GLX equivalente |
| `SAMPLES`, `SAMPLE_BUFFERS` | `GLX_SAMPLES`, `GLX_SAMPLE_BUFFERS` |
| `CONFIG_ID`, `NATIVE_VISUAL_ID` | `GLX_FBCONFIG_ID`, `GLX_VISUAL_ID` |
| `SURFACE_TYPE` | `GLX_DRAWABLE_TYPE` (`WINDOW`, `PIXMAP`, `PBUFFER`) |
| `RENDERABLE_TYPE`, `CONFORMANT` | Fijos: `EGL_OPENGL_BIT` |
| `MAX_PBUFFER_WIDTH/HEIGHT/PIXELS` | Fijos: 4096 / 4096 / 4096² |
| `MAX_SWAP_INTERVAL` | Fijo: 10 |
| `CONFIG_CAVEAT`, `TRANSPARENT_TYPE` | Fijo: `NONE` |
| `COLOR_BUFFER_TYPE` | Fijo: `RGB_BUFFER` |
| `NATIVE_VISUAL_TYPE`, `NATIVE_RENDERABLE` | Fijos: `TrueColor`, 1 |
| Cualquier otro (`LEVEL`, `BIND_TO_TEXTURE_*`…) | 0 |

#### Extensiones anunciadas
- Sin display: `EGL_KHR_client_get_all_proc_addresses`, `EGL_EXT_platform_base`,
  `EGL_EXT_platform_x11`, `EGL_KHR_platform_x11`.
- Con display: `EGL_KHR_create_context`, `EGL_KHR_create_context_no_error`,
  `EGL_KHR_surfaceless_context`, `EGL_KHR_no_config_context`, `EGL_KHR_get_all_proc_addresses`,
  `EGL_KHR_fence_sync`, `EGL_KHR_wait_sync`, `EGL_KHR_image_base`.

#### Qué no hace
- **Solo `EGL_OPENGL_API`.** `eglBindAPI` con `EGL_OPENGL_ES_API` o `EGL_OPENVG_API` devuelve
  `EGL_BAD_PARAMETER`.
- **Sin superficies de pixmap:** `eglCreatePixmapSurface` y `eglCreatePlatformPixmapSurface` devuelven
  `EGL_BAD_NATIVE_PIXMAP`; `eglCopyBuffers` falla.
- **`eglCreatePbufferFromClientBuffer` no está soportado.**
- **`eglSurfaceAttrib`, `eglBindTexImage`, `eglReleaseTexImage`** devuelven éxito sin hacer nada.
- **Superficie de lectura:** `eglMakeCurrent` guarda la superficie de lectura pero GLX solo recibe la de
  dibujo; leer de una superficie distinta no está soportado.
- **Sincronización simulada:** `eglCreateSync[KHR]` devuelve un objeto ficticio; `eglClientWaitSync` hace
  `glXWaitGL` y responde `CONDITION_SATISFIED`; `eglWaitSync` no hace nada.
- **`eglCreateImage` falla** (`EGL_BAD_PARAMETER`, devuelve `NULL`); `eglDestroyImage` no hace nada.
- **Límites:** hasta 64 contextos vivos a la vez (`B_MAXCTX`); el error de EGL es por hilo.
- **`eglInitialize`/`eglTerminate` no reflejan el estado real** del servidor X ni de GLX.

---

## 8. Compilación, instalación y pruebas

### Biblioteca completa (la que se instala como `libGL.so.1`)

```
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DNOX11=OFF -DNOEGL=OFF -DSTATICLIB=OFF -DGBM=OFF
make -j4
```

La salida queda en `lib/libGL.so.1`. `NOX11=OFF` activa GLX, `NOEGL=OFF` activa EGL, `GBM=OFF` omite DRM.
El wrapper EGL → GLX (`lib/libEGL.so.1`) solo se genera con `-DEGL_WRAPPER=ON`.

Sin CMake, en Linux con X11: `tools/build_linux.sh [directorio_salida] [CFLAGS extra]` replica la lista de
fuentes de `src/CMakeLists.txt` y compila con `gcc` (deja `libGL.so.1` y un `warnings.log`).

### Termux

`bash scripts/termux-install.sh` comprueba el entorno, compila si hace falta, verifica la integridad, instala
Lorica como `libGL` del sistema (arranca `termux-x11` si está instalado) y ofrece compilar y correr
`TEST/LoriTest.c`. Opciones: `--rebuild`, `-y/--tests`, `-n/--no-tests`, `--no-install`, `--no-x11`,
`-h/--help`; también acepta la ruta de un `libGL.so.1` ya compilado.

### Solo la capa GLADIATOR (`src/gl/GLADIATOR/Makefile`)

| Comando | Resultado |
|---|---|
| `make` | `liblorica_gl31.a` |
| `make SHARED=1` | `liblorica_gl31.so` |
| `make SYSTEM_GLES=1` | Usa los headers GLES del sistema en vez de `third_party/khronos` |
| `make PLATFORM_HEADER=mi_gles.h` | Header de plataforma propio (`LORICA_GL31_PLATFORM_HEADER`) |

Uso standalone: `gl31_init(loader)` con una función que devuelva direcciones (`eglGetProcAddress`,
`dlsym`…), `gl31_get_proc_address("glNombre")` para resolver funciones y `gl31_shutdown()` al final. Los
errores se reportan con `gl31_set_error()`. (`make test` está roto, ver sección 9.)

### Pruebas

- **`TEST/LoriTest.c`**: prueba de extremo a extremo. Cubo de Rubik 3D con contexto core por GLX; pide GL 4.3
  y va bajando hasta que uno funcione. Modos `--test` y `--test 3 3`. Con `LIBGL_GL=43` imprime
  `RESULTADO: TODO OK`. `scripts/termux-install.sh` la compila y la corre.
- **`tests/hybrid/`**: pruebas de la capa de GPU y de Vulkan, contra Mesa (EGL surfaceless, llvmpipe).
  `bash tests/hybrid/run.sh` las compila y las corre todas; cada una termina con `RESULT: TODO OK`.
  Variables útiles: `LIBDIR` (carpeta de `libEGL`/`libGLESv2`) y, en contenedores sin GPU,
  `LORICA_VK_LIB=/ruta/libvk_swiftshader.so`.

| Prueba | Qué cubre |
|---|---|
| `test_lvk.c` | Carga de Vulkan e infraestructura (`src/vk`) |
| `test_timer.c` | Timers y timestamps por Vulkan (necesita un driver Vulkan 1.1) |
| `test_bufstore.c` | `glBufferStorage`, mapeo persistente, errores de la especificación |
| `test_lu.c` | Uniforms internos ocultos a la app, índices y nombres de uniforms |
| `test_idx.c` | Restart, polygon mode, primitivas generadas, transform feedback y provoking vertex (`restart`, `poly`, `prims`, `tf`, `prov2`) |
| `test_pixels.c` | Motor de píxeles: `glReadPixels` (color, depth, stencil, PBO), `glGetTexImage`, BGRA, `PACK_*`, `glClampColor`, draw buffers con mapeo arbitrario (FBO proxy) |
| `test_gl4misc.c` | Misceláneo de GL 4.x: viewports indexados, `glClearBufferData`, contadores atómicos, `glShaderStorageBlockBinding` |
| `test_fallback.c` | Rutas de respaldo con `LORICA_DISABLE_CAPS` |
| `test_compressed.c` | `glGetCompressedTexImage` y la copia de bloques |
| `test_decode.c` | Decodificador GPU de comprimidos contra el del driver, con bloques aleatorios |

---

## 9. Problemas conocidos

No hay una lista formal de issues. Estado: **Abierto** (hay que corregirlo), **Diseño** (limitación asumida,
ver sección 5) o **Menor**.

### A. Repositorio y herramientas

| ID | Problema | Estado |
|---|---|---|
| R-01 | **Pruebas referenciadas que no están en el repo.** `make test` en `src/gl/GLADIATOR/Makefile` apunta a `TEST/gladiator/{test_uniform,test_integration,test_gl32,test_gl33,test_gl4,test_dsa}.c`, y el `CMakeLists.txt` raíz referencia `TEST/legacy/test.cmake`. Ninguno existe: `make test` y los tests de CMake fallan. Las pruebas vigentes son `TEST/LoriTest.c` y `tests/hybrid/` | Abierto |
| R-03 | **Nombres de gl4es.** `version.h` conserva el guard `_GL4ES_VERSION_H`, `CMakeLists.txt` declara `project(gl4es)` y `debian/changelog` habla de `gl4es (1.1.4-1)`; el número de versión (1.4.5) sí es el de Lorica | Abierto |
| R-05 | **`scripts/termux-install.sh` es agresivo con Mesa.** `rm -f "$DST"/libGL.so.1.[0-9]*` borra los `libGL.so.1.x` versionados del sistema y `ln -sfn` reemplaza `libGL.so.1` / `libGL.so`. Una actualización de paquetes puede revertirlo. `--no-install` lo evita | Abierto |
| R-07 | **Comentario y código no coinciden** en `src/gl/init.c`: el comentario dice «default 3.2», pero con `LIBGL_GL` sin definir el código fija 2.1 (1.5 en ES 1). En Android el valor real lo pone `lorica_android_defaults()` (4.3) | Abierto |
| R-08 | **El script de instalación no compila `libEGL.so.1`.** El wrapper EGL → GLX solo se genera con `-DEGL_WRAPPER=ON` y `termux-install.sh` no lo pasa | Abierto |
| R-06 | **Artefactos compilados dentro del árbol** (`build/`, `lib/`, `*.o`, `liblorica_gl31.a`, `TEST/loritest/build/`, binarios de `tests/hybrid`). El `.gitignore` los excluye de git; solo ensucian copias y archivos comprimidos | Menor |

### B. Wrapper EGL → GLX (`src/egl/`)

| ID | Problema | Estado |
|---|---|---|
| E-01 | **Extensiones anunciadas sin respaldo.** Anuncia `EGL_KHR_image_base`, pero `eglCreateImage` siempre falla; anuncia `EGL_KHR_create_context_no_error` y no se comprobó que haga algo con ese flag | Abierto |
| E-02 | **Sync ficticio.** `eglCreateSync`/`eglCreateSyncKHR` devuelven un objeto falso y `eglClientWaitSync` hace `glXWaitGL`: no hay fences reales | Diseño |
| E-03 | **Superficie de lectura ignorada** en `eglMakeCurrent` (GLX solo recibe la de dibujo) | Diseño |
| E-04 | **`eglGetConfigAttrib` nunca devuelve `EGL_BAD_ATTRIBUTE`:** los atributos desconocidos devuelven 0 con éxito | Abierto |
| E-05 | **Sin pixmaps ni `eglCopyBuffers`** | Diseño |

### C. Pendientes de verificar

| ID | Tema | Detalle |
|---|---|---|
| V-01 | Trampolines aarch64 | El salto `adrp/ldr/br` en aarch64 figuraba «sin probar». Conviene confirmar que la verificación en Mali-G52 (aarch64) pasó por los trampolines (`lorica_disp`) y no con `-DLORICA_NO_DISPATCH` |
| V-02 | `glTexImage1D` | Se documenta la emulación como 2D; falta confirmarla con una prueba |
| V-03 | Variables `LIBGL_AHB*`, `LIBGL_OCCL*`, `LIBGL_STRINGS`, `LIBGL_GL4ES_COMPAT`, `LIBGL_FBO` | Existen en el código; falta documentar su semántica |

### D. Divergencias de comportamiento respecto a GL de escritorio

Son limitaciones de diseño; el detalle está en la sección 5.3. Las principales: `glTexImage2DMultisample`
repetido sobre la misma textura da `GL_INVALID_OPERATION`; `glFramebufferTexture` sin geometry shaders
adjunta solo la capa 0; base vertex emulado modifica punteros de atributos en cada draw; estado compartido
entre contextos sin locking; `double` pasa a `float` en GLSL; `LIBGL_FB=2` desactiva GLADIATOR.

---

## 10. Novedades de la 1.4.5

`version.h` pasa de 1.1.7 a **1.4.5**. Lo que esta versión incorpora respecto a lo que describía el README
anterior:

**Funciones y comportamiento nuevos**
- `glClipControl` ya se publica (antes devolvía `NULL`): `ZERO_TO_ONE` exacto en el VS y `UPPER_LEFT` con
  inversión de Y y del sentido de las caras. La tabla de despacho pasa de 571 a **572** funciones.
- `glDrawBuffer(GL_COLOR_ATTACHMENTi)` con `i>0` y `glDrawBuffers` con mapeo no identidad, mediante un FBO
  proxy.
- `glShaderStorageBlockBinding`: antes daba `GL_INVALID_OPERATION`.
- `glGetCompressedTexImage`: antes daba siempre `GL_INVALID_OPERATION`; ahora lee de una copia de bloques en
  GPU (presupuesto `LORICA_COMPRESSED_SHADOW_MB`).
- `glGetTexImage` de texturas comprimidas (S3TC, RGTC, EAC, ETC2) y de profundidad / profundidad-stencil;
  `GL_PACK_IMAGE_HEIGHT` y `GL_PACK_SKIP_IMAGES` en 3D y arrays.
- `glReadPixels` de color y depth/stencil en GPU con cualquier formato/tipo de GL (`SWAP_BYTES`,
  `CLAMP_READ_COLOR`, PBO GPU→GPU) y subida `GL_BGRA` por compute.
- `glClearBufferData/SubData` con cualquier `format/type` (antes solo patrón cero o `R32F/R32I/R32UI`).
- `glPolygonMode(LINE/POINT)`, `glPrimitiveRestartIndex` con cualquier índice y `glProvokingVertex(FIRST)`
  sin extensión, en GPU; `glPointSize` y origen de point sprite; `GL_DEPTH_CLAMP` y `GL_CLIP_DISTANCEi` con
  respaldo cuando falta la extensión.
- `glCopyImageSubData` sin la extensión (copia por `glBlitFramebuffer`), `glVertexAttribFormat/Binding` sin
  soporte nativo, `glSamplerParameterIiv/Iuiv`, `glMinSampleShading`, `glGetMultisamplefv`, blend por buffer
  con respaldo, `glGetActiveAtomicCounterBufferiv`.
- `GL_TIME_ELAPSED`, `glQueryCounter` y `GL_TIMESTAMP` sin `EXT_disjoint_timer_query`, con timestamps de
  Vulkan; `GL_PRIMITIVES_GENERATED` y más de 64 ids de query.
- `glBufferStorage` con `PERSISTENT`/`COHERENT` aun sin `EXT_buffer_storage` (memoria de Vulkan por fd).

**Infraestructura**
- Capa de apoyo de Vulkan (`src/vk/`, `gl31_vk.[ch]`).
- Diagnóstico: `LORICA_DISABLE_CAPS`, `LORICA_TRACE_ERR`, `LORICA_VK*`, `LORICA_BUFSTORE`, `LORICA_DEBUG`.
- Suite `tests/hybrid/` y `tools/build_linux.sh`.
- Documentación consolidada en este archivo; se eliminan las notas anteriores.

**Correcciones**
- `glGetBufferSubData` fallaba sobre buffers mapeados de forma persistente.
- `glEnable(GL_CLIP_DISTANCEi)` se ignoraba incluso con `EXT_clip_cull_distance`.
- El kernel de índices leía fuera de rango la última palabra de buffers de índices de tamaño no múltiplo de 4.
- Las queries «falsas» perdían el resultado a partir de 64 ids.
- `gl31_init` imprimía `[caps]` siempre por `stderr`; ahora solo con `LIBGL_DEBUG`/`LORICA_DEBUG`.
- `glCreateShader(GL_COMPUTE_SHADER / TESS_*)` se rechazaba aunque el backend los tuviera.
- Los comentarios del código que citaban las notas eliminadas apuntan ahora a este documento, y el aviso de
  `glGetTexImage` sobre comprimidos ya menciona ETC2.

---

## Apéndice A: funciones de GLADIATOR

La tabla de despacho de GLADIATOR publica **572 funciones**. Están agrupadas automáticamente por nombre; algunas pueden aparecer en un grupo aproximado. La fuente de verdad es `src/gl/GLADIATOR/gl31.c`.

### Shaders, programas y pipelines (44)

`glActiveShaderProgram`, `glAttachShader`, `glBindAttribLocation`, `glBindFragDataLocation`,
`glBindFragDataLocationIndexed`, `glBindProgramPipeline`, `glCompileShader`, `glDeleteProgram`,
`glDeleteProgramPipelines`, `glDeleteShader`, `glDetachShader`, `glGenProgramPipelines`,
`glGetActiveAttrib`, `glGetAttachedShaders`, `glGetAttribLocation`, `glGetFragDataIndex`,
`glGetFragDataLocation`, `glGetProgramBinary`, `glGetProgramInfoLog`, `glGetProgramInterfaceiv`,
`glGetProgramPipelineInfoLog`, `glGetProgramPipelineiv`, `glGetProgramResourceIndex`,
`glGetProgramResourceLocation`, `glGetProgramResourceName`, `glGetProgramResourceiv`,
`glGetProgramiv`, `glGetShaderInfoLog`, `glGetShaderPrecisionFormat`, `glGetShaderSource`,
`glGetShaderiv`, `glIsProgram`, `glIsProgramPipeline`, `glIsShader`, `glLinkProgram`,
`glProgramBinary`, `glProgramParameteri`, `glReleaseShaderCompiler`, `glShaderSource`,
`glShaderStorageBlockBinding`, `glUseProgram`, `glUseProgramStages`, `glValidateProgram`,
`glValidateProgramPipeline`

### Texturas y samplers (61)

`glActiveTexture`, `glBindSampler`, `glBindSamplers`, `glBindTexture`, `glBindTextures`,
`glCompressedTexImage1D`, `glCompressedTexImage2D`, `glCompressedTexImage3D`,
`glCompressedTexSubImage1D`, `glCompressedTexSubImage2D`, `glCompressedTexSubImage3D`,
`glCopyImageSubData`, `glCopyTexImage1D`, `glCopyTexImage2D`, `glCopyTexSubImage1D`,
`glCopyTexSubImage2D`, `glCopyTexSubImage3D`, `glDeleteSamplers`, `glDeleteTextures`,
`glGenSamplers`, `glGenTextures`, `glGenerateMipmap`, `glGetCompressedTexImage`,
`glGetSamplerParameterIiv`, `glGetSamplerParameterIuiv`, `glGetSamplerParameterfv`,
`glGetSamplerParameteriv`, `glGetTexImage`, `glGetTexLevelParameterfv`, `glGetTexLevelParameteriv`,
`glGetTexParameterIiv`, `glGetTexParameterIuiv`, `glGetTexParameterfv`, `glGetTexParameteriv`,
`glIsSampler`, `glIsTexture`, `glSamplerParameterIiv`, `glSamplerParameterIuiv`,
`glSamplerParameterf`, `glSamplerParameterfv`, `glSamplerParameteri`, `glSamplerParameteriv`,
`glTexImage1D`, `glTexImage2D`, `glTexImage2DMultisample`, `glTexImage3D`,
`glTexImage3DMultisample`, `glTexParameterIiv`, `glTexParameterIuiv`, `glTexParameterf`,
`glTexParameterfv`, `glTexParameteri`, `glTexParameteriv`, `glTexStorage1D`, `glTexStorage2D`,
`glTexStorage2DMultisample`, `glTexStorage3D`, `glTexStorage3DMultisample`, `glTexSubImage1D`,
`glTexSubImage2D`, `glTexSubImage3D`

### Estado, rasterizado, blending, viewport y consultas (78)

`glBeginConditionalRender`, `glBlendColor`, `glBlendEquation`, `glBlendEquationSeparate`,
`glBlendEquationSeparatei`, `glBlendEquationi`, `glBlendFunc`, `glBlendFuncSeparate`,
`glBlendFuncSeparatei`, `glBlendFunci`, `glClampColor`, `glClipControl`, `glColorMask`,
`glColorMaski`, `glCullFace`, `glDeleteQueries`, `glDepthFunc`, `glDepthMask`, `glDepthRange`,
`glDepthRangeArrayv`, `glDepthRangeIndexed`, `glDepthRangef`, `glDisable`, `glDisablei`, `glEnable`,
`glEnablei`, `glEndConditionalRender`, `glFinish`, `glFlush`, `glFrontFace`, `glGenQueries`,
`glGetBooleani_v`, `glGetBooleanv`, `glGetDoublei_v`, `glGetDoublev`, `glGetError`, `glGetFloati_v`,
`glGetFloatv`, `glGetInteger64i_v`, `glGetInteger64v`, `glGetIntegeri_v`, `glGetIntegerv`,
`glGetInternalformativ`, `glGetMultisamplefv`, `glGetString`, `glGetStringi`, `glHint`,
`glIsEnabled`, `glIsEnabledi`, `glLineWidth`, `glLogicOp`, `glMinSampleShading`, `glPixelStoref`,
`glPixelStorei`, `glPointParameterf`, `glPointParameterfv`, `glPointParameteri`,
`glPointParameteriv`, `glPointSize`, `glPolygonMode`, `glPolygonOffset`, `glProvokingVertex`,
`glSampleCoverage`, `glSampleMaski`, `glScissor`, `glScissorArrayv`, `glScissorIndexed`,
`glScissorIndexedv`, `glStencilFunc`, `glStencilFuncSeparate`, `glStencilMask`,
`glStencilMaskSeparate`, `glStencilOp`, `glStencilOpSeparate`, `glViewport`, `glViewportArrayv`,
`glViewportIndexedf`, `glViewportIndexedfv`

### Queries y sincronización (18)

`glBeginQuery`, `glBeginQueryIndexed`, `glClientWaitSync`, `glDeleteSync`, `glEndQuery`,
`glEndQueryIndexed`, `glFenceSync`, `glGetQueryIndexediv`, `glGetQueryObjecti64v`,
`glGetQueryObjectiv`, `glGetQueryObjectui64v`, `glGetQueryObjectuiv`, `glGetQueryiv`, `glGetSynciv`,
`glIsQuery`, `glIsSync`, `glQueryCounter`, `glWaitSync`

### Transform feedback (13)

`glBeginTransformFeedback`, `glBindTransformFeedback`, `glDeleteTransformFeedbacks`,
`glDrawTransformFeedback`, `glDrawTransformFeedbackInstanced`, `glDrawTransformFeedbackStream`,
`glEndTransformFeedback`, `glGenTransformFeedbacks`, `glGetTransformFeedbackVarying`,
`glIsTransformFeedback`, `glPauseTransformFeedback`, `glResumeTransformFeedback`,
`glTransformFeedbackVaryings`

### Buffers (23)

`glBindBuffer`, `glBindBufferBase`, `glBindBufferRange`, `glBindBuffersBase`, `glBindBuffersRange`,
`glBufferData`, `glBufferStorage`, `glBufferSubData`, `glCopyBufferSubData`,
`glCopyNamedBufferSubData`, `glDeleteBuffers`, `glFlushMappedBufferRange`, `glGenBuffers`,
`glGetBufferParameteri64v`, `glGetBufferParameteriv`, `glGetBufferPointerv`, `glGetBufferSubData`,
`glIsBuffer`, `glMapBuffer`, `glMapBufferRange`, `glTexBuffer`, `glTexBufferRange`, `glUnmapBuffer`

### Framebuffers, renderbuffers y clears (41)

`glBindFramebuffer`, `glBindRenderbuffer`, `glBlitFramebuffer`, `glCheckFramebufferStatus`,
`glClear`, `glClearBufferData`, `glClearBufferSubData`, `glClearBufferfi`, `glClearBufferfv`,
`glClearBufferiv`, `glClearBufferuiv`, `glClearColor`, `glClearDepth`, `glClearDepthf`,
`glClearStencil`, `glDeleteFramebuffers`, `glDeleteRenderbuffers`, `glFramebufferParameteri`,
`glFramebufferRenderbuffer`, `glFramebufferTexture`, `glFramebufferTexture1D`,
`glFramebufferTexture2D`, `glFramebufferTexture3D`, `glFramebufferTextureLayer`,
`glGenFramebuffers`, `glGenRenderbuffers`, `glGetFramebufferAttachmentParameteriv`,
`glGetFramebufferParameteriv`, `glGetRenderbufferParameteriv`, `glInvalidateBufferData`,
`glInvalidateBufferSubData`, `glInvalidateFramebuffer`, `glInvalidateSubFramebuffer`,
`glInvalidateTexImage`, `glInvalidateTexSubImage`, `glIsFramebuffer`, `glIsRenderbuffer`,
`glReadBuffer`, `glReadPixels`, `glRenderbufferStorage`, `glRenderbufferStorageMultisample`

### Compute, imágenes y barreras (7)

`glBindImageTexture`, `glBindImageTextures`, `glDispatchCompute`, `glDispatchComputeIndirect`,
`glGetActiveAtomicCounterBufferiv`, `glMemoryBarrier`, `glMemoryBarrierByRegion`

### DSA (acceso directo por nombre, GL 4.5 ext) (93)

`glBindTextureUnit`, `glBlitNamedFramebuffer`, `glCheckNamedFramebufferStatus`,
`glClearNamedBufferData`, `glClearNamedBufferSubData`, `glClearNamedFramebufferfi`,
`glClearNamedFramebufferfv`, `glClearNamedFramebufferiv`, `glClearNamedFramebufferuiv`,
`glCompressedTextureSubImage1D`, `glCompressedTextureSubImage2D`, `glCompressedTextureSubImage3D`,
`glCopyTextureSubImage1D`, `glCopyTextureSubImage2D`, `glCopyTextureSubImage3D`, `glCreateBuffers`,
`glCreateFramebuffers`, `glCreateProgram`, `glCreateProgramPipelines`, `glCreateQueries`,
`glCreateRenderbuffers`, `glCreateSamplers`, `glCreateShader`, `glCreateShaderProgramv`,
`glCreateTextures`, `glCreateTransformFeedbacks`, `glCreateVertexArrays`,
`glDisableVertexArrayAttrib`, `glEnableVertexArrayAttrib`, `glFlushMappedNamedBufferRange`,
`glGenerateTextureMipmap`, `glGetCompressedTextureImage`, `glGetGraphicsResetStatus`,
`glGetNamedBufferParameteri64v`, `glGetNamedBufferParameteriv`, `glGetNamedBufferPointerv`,
`glGetNamedBufferSubData`, `glGetNamedFramebufferAttachmentParameteriv`,
`glGetNamedFramebufferParameteriv`, `glGetNamedRenderbufferParameteriv`, `glGetTextureImage`,
`glGetTextureLevelParameterfv`, `glGetTextureLevelParameteriv`, `glGetTextureParameterIiv`,
`glGetTextureParameterIuiv`, `glGetTextureParameterfv`, `glGetTextureParameteriv`,
`glGetVertexArrayIndexediv`, `glGetVertexArrayiv`, `glInvalidateNamedFramebufferData`,
`glInvalidateNamedFramebufferSubData`, `glMapNamedBuffer`, `glMapNamedBufferRange`,
`glNamedBufferData`, `glNamedBufferStorage`, `glNamedBufferSubData`, `glNamedFramebufferDrawBuffer`,
`glNamedFramebufferDrawBuffers`, `glNamedFramebufferParameteri`, `glNamedFramebufferReadBuffer`,
`glNamedFramebufferRenderbuffer`, `glNamedFramebufferTexture`, `glNamedFramebufferTextureLayer`,
`glNamedRenderbufferStorage`, `glNamedRenderbufferStorageMultisample`, `glTextureBuffer`,
`glTextureBufferRange`, `glTextureParameterIiv`, `glTextureParameterIuiv`, `glTextureParameterf`,
`glTextureParameterfv`, `glTextureParameteri`, `glTextureParameteriv`, `glTextureStorage1D`,
`glTextureStorage2D`, `glTextureStorage2DMultisample`, `glTextureStorage3D`,
`glTextureStorage3DMultisample`, `glTextureSubImage1D`, `glTextureSubImage2D`,
`glTextureSubImage3D`, `glTextureView`, `glTransformFeedbackBufferBase`,
`glTransformFeedbackBufferRange`, `glUnmapNamedBuffer`, `glVertexArrayAttribBinding`,
`glVertexArrayAttribFormat`, `glVertexArrayAttribIFormat`, `glVertexArrayAttribLFormat`,
`glVertexArrayBindingDivisor`, `glVertexArrayElementBuffer`, `glVertexArrayVertexBuffer`,
`glVertexArrayVertexBuffers`

### Vertex arrays y atributos (85)

`glBindVertexArray`, `glBindVertexBuffer`, `glBindVertexBuffers`, `glDeleteVertexArrays`,
`glDisableVertexAttribArray`, `glEnableVertexAttribArray`, `glGenVertexArrays`,
`glGetVertexAttribIiv`, `glGetVertexAttribIuiv`, `glGetVertexAttribPointerv`, `glGetVertexAttribdv`,
`glGetVertexAttribfv`, `glGetVertexAttribiv`, `glIsVertexArray`, `glVertexAttrib1d`,
`glVertexAttrib1dv`, `glVertexAttrib1f`, `glVertexAttrib1fv`, `glVertexAttrib1s`,
`glVertexAttrib1sv`, `glVertexAttrib2d`, `glVertexAttrib2dv`, `glVertexAttrib2f`,
`glVertexAttrib2fv`, `glVertexAttrib2s`, `glVertexAttrib2sv`, `glVertexAttrib3d`,
`glVertexAttrib3dv`, `glVertexAttrib3f`, `glVertexAttrib3fv`, `glVertexAttrib3s`,
`glVertexAttrib3sv`, `glVertexAttrib4Nbv`, `glVertexAttrib4Nsv`, `glVertexAttrib4Nub`,
`glVertexAttrib4Nubv`, `glVertexAttrib4Nuiv`, `glVertexAttrib4Nusv`, `glVertexAttrib4bv`,
`glVertexAttrib4d`, `glVertexAttrib4dv`, `glVertexAttrib4f`, `glVertexAttrib4fv`,
`glVertexAttrib4iv`, `glVertexAttrib4s`, `glVertexAttrib4sv`, `glVertexAttrib4ubv`,
`glVertexAttrib4uiv`, `glVertexAttrib4usv`, `glVertexAttribBinding`, `glVertexAttribDivisor`,
`glVertexAttribFormat`, `glVertexAttribI1i`, `glVertexAttribI1iv`, `glVertexAttribI1ui`,
`glVertexAttribI1uiv`, `glVertexAttribI2i`, `glVertexAttribI2iv`, `glVertexAttribI2ui`,
`glVertexAttribI2uiv`, `glVertexAttribI3i`, `glVertexAttribI3iv`, `glVertexAttribI3ui`,
`glVertexAttribI3uiv`, `glVertexAttribI4bv`, `glVertexAttribI4i`, `glVertexAttribI4iv`,
`glVertexAttribI4sv`, `glVertexAttribI4ubv`, `glVertexAttribI4ui`, `glVertexAttribI4uiv`,
`glVertexAttribI4usv`, `glVertexAttribIFormat`, `glVertexAttribIPointer`, `glVertexAttribLFormat`,
`glVertexAttribP1ui`, `glVertexAttribP1uiv`, `glVertexAttribP2ui`, `glVertexAttribP2uiv`,
`glVertexAttribP3ui`, `glVertexAttribP3uiv`, `glVertexAttribP4ui`, `glVertexAttribP4uiv`,
`glVertexAttribPointer`, `glVertexBindingDivisor`

### Debug y etiquetas (8)

`glDebugMessageCallback`, `glDebugMessageControl`, `glDebugMessageInsert`, `glGetDebugMessageLog`,
`glGetObjectLabel`, `glObjectLabel`, `glPopDebugGroup`, `glPushDebugGroup`

### Draw calls (23)

`glDrawArrays`, `glDrawArraysIndirect`, `glDrawArraysInstanced`,
`glDrawArraysInstancedBaseInstance`, `glDrawBuffer`, `glDrawBuffers`, `glDrawElements`,
`glDrawElementsBaseVertex`, `glDrawElementsIndirect`, `glDrawElementsInstanced`,
`glDrawElementsInstancedBaseInstance`, `glDrawElementsInstancedBaseVertex`,
`glDrawElementsInstancedBaseVertexBaseInstance`, `glDrawRangeElements`,
`glDrawRangeElementsBaseVertex`, `glMultiDrawArrays`, `glMultiDrawArraysIndirect`,
`glMultiDrawElements`, `glMultiDrawElementsBaseVertex`, `glMultiDrawElementsIndirect`,
`glPatchParameterfv`, `glPatchParameteri`, `glPrimitiveRestartIndex`

### Uniforms y bloques de uniforms (45)

`glGetActiveUniform`, `glGetActiveUniformBlockName`, `glGetActiveUniformBlockiv`,
`glGetActiveUniformName`, `glGetActiveUniformsiv`, `glGetUniformBlockIndex`, `glGetUniformIndices`,
`glGetUniformLocation`, `glGetUniformfv`, `glGetUniformiv`, `glGetUniformuiv`, `glUniform1f`,
`glUniform1fv`, `glUniform1i`, `glUniform1iv`, `glUniform1ui`, `glUniform1uiv`, `glUniform2f`,
`glUniform2fv`, `glUniform2i`, `glUniform2iv`, `glUniform2ui`, `glUniform2uiv`, `glUniform3f`,
`glUniform3fv`, `glUniform3i`, `glUniform3iv`, `glUniform3ui`, `glUniform3uiv`, `glUniform4f`,
`glUniform4fv`, `glUniform4i`, `glUniform4iv`, `glUniform4ui`, `glUniform4uiv`,
`glUniformBlockBinding`, `glUniformMatrix2fv`, `glUniformMatrix2x3fv`, `glUniformMatrix2x4fv`,
`glUniformMatrix3fv`, `glUniformMatrix3x2fv`, `glUniformMatrix3x4fv`, `glUniformMatrix4fv`,
`glUniformMatrix4x2fv`, `glUniformMatrix4x3fv`

### glProgramUniform* (33)

`glProgramUniform1f`, `glProgramUniform1fv`, `glProgramUniform1i`, `glProgramUniform1iv`,
`glProgramUniform1ui`, `glProgramUniform1uiv`, `glProgramUniform2f`, `glProgramUniform2fv`,
`glProgramUniform2i`, `glProgramUniform2iv`, `glProgramUniform2ui`, `glProgramUniform2uiv`,
`glProgramUniform3f`, `glProgramUniform3fv`, `glProgramUniform3i`, `glProgramUniform3iv`,
`glProgramUniform3ui`, `glProgramUniform3uiv`, `glProgramUniform4f`, `glProgramUniform4fv`,
`glProgramUniform4i`, `glProgramUniform4iv`, `glProgramUniform4ui`, `glProgramUniform4uiv`,
`glProgramUniformMatrix2fv`, `glProgramUniformMatrix2x3fv`, `glProgramUniformMatrix2x4fv`,
`glProgramUniformMatrix3fv`, `glProgramUniformMatrix3x2fv`, `glProgramUniformMatrix3x4fv`,
`glProgramUniformMatrix4fv`, `glProgramUniformMatrix4x2fv`, `glProgramUniformMatrix4x3fv`

## Apéndice B: funciones del núcleo legacy

Nombres base publicados por el núcleo legacy (640), tomados de `src/gl/gl_lookup.c` y `src/gl/glesfuncs.inc`. Cada
uno se resuelve también con sufijo `ARB` o `EXT` cuando el código lo registra así, y algunos dependen de las
capacidades del backend.

`glActiveTexture`, `glAlphaFunc`, `glAlphaFuncx`, `glAreTexturesResident`, `glArrayElement`,
`glAttachObject`, `glAttachShader`, `glBegin`, `glBeginQuery`, `glBindAttribLocation`,
`glBindBuffer`, `glBindFramebuffer`, `glBindMultiTexture`, `glBindProgramARB`, `glBindRenderbuffer`,
`glBindSampler`, `glBindTexture`, `glBindVertexArray`, `glBitmap`, `glBlendColor`,
`glBlendEquation`, `glBlendEquationSeparate`, `glBlendEquationSeparatei`, `glBlendFunc`,
`glBlendFuncSeparate`, `glBlendFuncSeparatei`, `glBlitFramebuffer`, `glBufferData`,
`glBufferSubData`, `glCallList`, `glCallLists`, `glCheckFramebufferStatus`, `glClampColor`,
`glClear`, `glClearBufferfi`, `glClearBufferfv`, `glClearBufferiv`, `glClearBufferuiv`,
`glClearColor`, `glClearColorx`, `glClearDepth`, `glClearDepthf`, `glClearDepthfOES`,
`glClearDepthx`, `glClearNamedFramebufferfi`, `glClearNamedFramebufferfv`,
`glClearNamedFramebufferiv`, `glClearNamedFramebufferuiv`, `glClearStencil`,
`glClientActiveTexture`, `glClientAttribDefault`, `glClipPlane`, `glClipPlanef`, `glClipPlanefOES`,
`glClipPlanex`, `glColor4f`, `glColor4ub`, `glColor4x`, `glColorMask`, `glColorMaskIndexed`,
`glColorMaterial`, `glColorPointer`, `glCompileShader`, `glCompressedMultiTexImage1D`,
`glCompressedMultiTexImage2D`, `glCompressedMultiTexImage3D`, `glCompressedMultiTexSubImage1D`,
`glCompressedMultiTexSubImage2D`, `glCompressedMultiTexSubImage3D`, `glCompressedTexImage1D`,
`glCompressedTexImage2D`, `glCompressedTexImage3D`, `glCompressedTexSubImage1D`,
`glCompressedTexSubImage2D`, `glCompressedTexSubImage3D`, `glCompressedTextureImage1D`,
`glCompressedTextureImage2D`, `glCompressedTextureImage3D`, `glCompressedTextureSubImage1D`,
`glCompressedTextureSubImage2D`, `glCompressedTextureSubImage3D`, `glCopyBufferSubData`,
`glCopyMultiTexImage1D`, `glCopyMultiTexImage2D`, `glCopyMultiTexSubImage1D`,
`glCopyMultiTexSubImage2D`, `glCopyMultiTexSubImage3D`, `glCopyPixels`, `glCopyTexImage1D`,
`glCopyTexImage2D`, `glCopyTexSubImage1D`, `glCopyTexSubImage2D`, `glCopyTexSubImage3D`,
`glCopyTextureImage1D`, `glCopyTextureImage2D`, `glCopyTextureSubImage1D`,
`glCopyTextureSubImage2D`, `glCopyTextureSubImage3D`, `glCreateProgram`, `glCreateProgramObject`,
`glCreateShader`, `glCreateShaderObject`, `glCullFace`, `glDeleteBuffers`, `glDeleteFramebuffers`,
`glDeleteLists`, `glDeleteObject`, `glDeleteProgram`, `glDeleteProgramsARB`, `glDeleteQueries`,
`glDeleteRenderbuffers`, `glDeleteSamplers`, `glDeleteShader`, `glDeleteTextures`,
`glDeleteVertexArrays`, `glDepthFunc`, `glDepthMask`, `glDepthRange`, `glDepthRangef`,
`glDepthRangefOES`, `glDepthRangex`, `glDetachObject`, `glDetachShader`, `glDisable`,
`glDisableClientState`, `glDisableClientStateIndexed`, `glDisableClientStatei`, `glDisableIndexed`,
`glDisableVertexArray`, `glDisableVertexArrayAttrib`, `glDisableVertexAttribArray`, `glDrawArrays`,
`glDrawArraysInstanced`, `glDrawBuffer`, `glDrawBuffers`, `glDrawElements`,
`glDrawElementsBaseVertex`, `glDrawElementsInstanced`, `glDrawElementsInstancedBaseVertex`,
`glDrawPixels`, `glDrawRangeElements`, `glDrawRangeElementsBaseVertex`, `glDrawTexf`, `glDrawTexi`,
`glEdgeFlag`, `glEdgeFlagPointer`, `glEnable`, `glEnableClientState`, `glEnableClientStateIndexed`,
`glEnableClientStatei`, `glEnableIndexed`, `glEnableVertexArray`, `glEnableVertexArrayAttrib`,
`glEnableVertexAttribArray`, `glEnd`, `glEndList`, `glEndQuery`, `glEvalCoord1d`, `glEvalCoord1dv`,
`glEvalCoord1f`, `glEvalCoord1fv`, `glEvalCoord2d`, `glEvalCoord2dv`, `glEvalCoord2f`,
`glEvalCoord2fv`, `glEvalMesh1`, `glEvalMesh2`, `glEvalPoint1`, `glEvalPoint2`, `glFinish`,
`glFlush`, `glFlushMappedBufferRange`, `glFogCoordPointer`, `glFogCoordd`, `glFogCoorddv`,
`glFogCoordf`, `glFogCoordfv`, `glFogf`, `glFogfv`, `glFogi`, `glFogiv`, `glFogx`, `glFogxv`,
`glFramebufferRenderbuffer`, `glFramebufferTexture1D`, `glFramebufferTexture2D`,
`glFramebufferTexture3D`, `glFramebufferTextureLayer`, `glFrontFace`, `glFrustum`, `glFrustumf`,
`glFrustumfOES`, `glFrustumx`, `glGenBuffers`, `glGenFramebuffers`, `glGenLists`,
`glGenProgramsARB`, `glGenQueries`, `glGenRenderbuffers`, `glGenSamplers`, `glGenTextures`,
`glGenVertexArrays`, `glGenerateMipmap`, `glGetActiveAttrib`, `glGetActiveUniform`,
`glGetAttachedObjects`, `glGetAttachedShaders`, `glGetAttribLocation`, `glGetBooleanIndexedv`,
`glGetBooleanv`, `glGetBufferParameteriv`, `glGetBufferPointerv`, `glGetBufferSubData`,
`glGetClipPlane`, `glGetClipPlanef`, `glGetClipPlanefOES`, `glGetClipPlanex`,
`glGetCompressedMultiTexImage`, `glGetCompressedTexImage`, `glGetCompressedTextureImage`,
`glGetDoubleIndexedv`, `glGetDoublev`, `glGetError`, `glGetFixedv`, `glGetFloatIndexedv`,
`glGetFloatv`, `glGetFramebufferAttachmentParameteriv`, `glGetHandle`, `glGetInfoLog`,
`glGetIntegerIndexedv`, `glGetIntegerv`, `glGetLightfv`, `glGetLightiv`, `glGetLightxv`,
`glGetMapdv`, `glGetMapfv`, `glGetMapiv`, `glGetMaterialfv`, `glGetMaterialiv`, `glGetMaterialxv`,
`glGetMultiTexEnvfv`, `glGetMultiTexEnviv`, `glGetMultiTexGendv`, `glGetMultiTexGenfv`,
`glGetMultiTexGeniv`, `glGetMultiTexImage`, `glGetMultiTexLevelParameterfv`,
`glGetMultiTexLevelParameteriv`, `glGetMultiTexParameterfv`, `glGetMultiTexParameteriv`,
`glGetNamedBufferParameteriv`, `glGetNamedBufferPointerv`, `glGetNamedBufferSubData`,
`glGetObjectParameterfv`, `glGetObjectParameteriv`, `glGetPixelMapfv`, `glGetPixelMapuiv`,
`glGetPixelMapusv`, `glGetPointerIndexedv`, `glGetPointerv`, `glGetProgramBinary`,
`glGetProgramEnvParameterdvARB`, `glGetProgramEnvParameterfvARB`, `glGetProgramInfoLog`,
`glGetProgramLocalParameterdvARB`, `glGetProgramLocalParameterfvARB`, `glGetProgramStringARB`,
`glGetProgramiv`, `glGetProgramivARB`, `glGetQueryObjecti64v`, `glGetQueryObjectiv`,
`glGetQueryObjectui64v`, `glGetQueryObjectuiv`, `glGetQueryiv`, `glGetRenderbufferParameteriv`,
`glGetSamplerParameterIiv`, `glGetSamplerParameterIuiv`, `glGetSamplerParameterfv`,
`glGetSamplerParameteriv`, `glGetShaderInfoLog`, `glGetShaderPrecisionFormat`, `glGetShaderSource`,
`glGetShaderiv`, `glGetString`, `glGetStringi`, `glGetTexEnvfv`, `glGetTexEnviv`, `glGetTexEnvxv`,
`glGetTexGendv`, `glGetTexGenfv`, `glGetTexGeniv`, `glGetTexImage`, `glGetTexLevelParameterfv`,
`glGetTexLevelParameteriv`, `glGetTexParameterfv`, `glGetTexParameteriv`, `glGetTexParameterxv`,
`glGetTextureImage`, `glGetTextureLevelParameterfv`, `glGetTextureLevelParameteriv`,
`glGetTextureParameterfv`, `glGetTextureParameteriv`, `glGetUniformLocation`, `glGetUniformfv`,
`glGetUniformiv`, `glGetVertexAttribPointerv`, `glGetVertexAttribdv`, `glGetVertexAttribfv`,
`glGetVertexAttribiv`, `glHint`, `glIndexPointer`, `glInitNames`, `glInterleavedArrays`,
`glIsBuffer`, `glIsEnabled`, `glIsEnabledIndexed`, `glIsFramebuffer`, `glIsList`, `glIsProgram`,
`glIsProgramARB`, `glIsQuery`, `glIsRenderbuffer`, `glIsSampler`, `glIsShader`, `glIsTexture`,
`glIsVertexArray`, `glLightModelf`, `glLightModelfv`, `glLightModeli`, `glLightModeliv`,
`glLightModelx`, `glLightModelxv`, `glLightf`, `glLightfv`, `glLighti`, `glLightiv`, `glLightx`,
`glLightxv`, `glLineStipple`, `glLineWidth`, `glLineWidthx`, `glLinkProgram`, `glListBase`,
`glLoadIdentity`, `glLoadMatrixd`, `glLoadMatrixf`, `glLoadMatrixx`, `glLoadName`,
`glLoadTransposeMatrixd`, `glLoadTransposeMatrixf`, `glLockArrays`, `glLogicOp`, `glMap1d`,
`glMap1f`, `glMap2d`, `glMap2f`, `glMapBuffer`, `glMapBufferRange`, `glMapGrid1d`, `glMapGrid1f`,
`glMapGrid2d`, `glMapGrid2f`, `glMapNamedBuffer`, `glMaterialf`, `glMaterialfv`, `glMateriali`,
`glMaterialiv`, `glMaterialx`, `glMaterialxv`, `glMatrixFrustum`, `glMatrixLoadIdentity`,
`glMatrixLoadTransposed`, `glMatrixLoadTransposef`, `glMatrixLoadd`, `glMatrixLoadf`,
`glMatrixMode`, `glMatrixMultTransposed`, `glMatrixMultTransposef`, `glMatrixMultd`,
`glMatrixMultf`, `glMatrixOrtho`, `glMatrixPop`, `glMatrixPush`, `glMatrixRotated`,
`glMatrixRotatef`, `glMatrixScaled`, `glMatrixScalef`, `glMatrixTranslated`, `glMatrixTranslatef`,
`glMultMatrixd`, `glMultMatrixf`, `glMultMatrixx`, `glMultTransposeMatrixd`,
`glMultTransposeMatrixf`, `glMultiDrawArrays`, `glMultiDrawElements`,
`glMultiDrawElementsBaseVertex`, `glMultiTexCoord4f`, `glMultiTexCoord4x`, `glMultiTexCoordPointer`,
`glMultiTexEnvf`, `glMultiTexEnvfv`, `glMultiTexEnvi`, `glMultiTexEnviv`, `glMultiTexGend`,
`glMultiTexGendv`, `glMultiTexGenf`, `glMultiTexGenfv`, `glMultiTexGeni`, `glMultiTexGeniv`,
`glMultiTexImage1D`, `glMultiTexImage2D`, `glMultiTexImage3D`, `glMultiTexParameterf`,
`glMultiTexParameterfv`, `glMultiTexParameteri`, `glMultiTexParameteriv`, `glMultiTexSubImage1D`,
`glMultiTexSubImage2D`, `glMultiTexSubImage3D`, `glNamedBufferData`, `glNamedBufferSubData`,
`glNewList`, `glNormal3f`, `glNormal3x`, `glNormalPointer`, `glOrtho`, `glOrthof`, `glOrthofOES`,
`glOrthox`, `glPixelMapfv`, `glPixelMapuiv`, `glPixelMapusv`, `glPixelStoref`, `glPixelStorei`,
`glPixelTransferf`, `glPixelTransferi`, `glPixelZoom`, `glPointParameterf`, `glPointParameterfv`,
`glPointParameteri`, `glPointParameteriv`, `glPointParameterx`, `glPointParameterxv`, `glPointSize`,
`glPointSizePointerOES`, `glPointSizex`, `glPolygonMode`, `glPolygonOffset`, `glPolygonOffsetx`,
`glPolygonStipple`, `glPopAttrib`, `glPopClientAttrib`, `glPopMatrix`, `glPopName`,
`glPrioritizeTextures`, `glProgramBinary`, `glProgramEnvParameter4dARB`,
`glProgramEnvParameter4dvARB`, `glProgramEnvParameter4fARB`, `glProgramEnvParameter4fvARB`,
`glProgramEnvParameters4fvEXT`, `glProgramLocalParameter4dARB`, `glProgramLocalParameter4dvARB`,
`glProgramLocalParameter4fARB`, `glProgramLocalParameter4fvARB`, `glProgramLocalParameters4fvEXT`,
`glProgramStringARB`, `glProgramUniform1f`, `glProgramUniform1fv`, `glProgramUniform1i`,
`glProgramUniform1iv`, `glProgramUniform2f`, `glProgramUniform2fv`, `glProgramUniform2i`,
`glProgramUniform2iv`, `glProgramUniform3f`, `glProgramUniform3fv`, `glProgramUniform3i`,
`glProgramUniform3iv`, `glProgramUniform4f`, `glProgramUniform4fv`, `glProgramUniform4i`,
`glProgramUniform4iv`, `glProgramUniformMatrix2fv`, `glProgramUniformMatrix3fv`,
`glProgramUniformMatrix4fv`, `glPushAttrib`, `glPushClientAttrib`, `glPushClientAttribDefault`,
`glPushMatrix`, `glPushName`, `glQueryCounter`, `glRasterPos2i`, `glReadBuffer`, `glReadPixels`,
`glRectd`, `glRectdv`, `glRectf`, `glRectfv`, `glRecti`, `glRectiv`, `glRects`, `glRectsv`,
`glReleaseShaderCompiler`, `glRenderMode`, `glRenderbufferStorage`,
`glRenderbufferStorageMultisample`, `glRotated`, `glRotatef`, `glRotatex`, `glSampleCoverage`,
`glSampleCoveragex`, `glSamplerParameterIiv`, `glSamplerParameterIuiv`, `glSamplerParameterf`,
`glSamplerParameterfv`, `glSamplerParameteri`, `glSamplerParameteriv`, `glScaled`, `glScalef`,
`glScalex`, `glScissor`, `glSecondaryColorPointer`, `glShadeModel`, `glShaderBinary`,
`glShaderSource`, `glStencilFunc`, `glStencilFuncSeparate`, `glStencilMask`,
`glStencilMaskSeparate`, `glStencilOp`, `glStencilOpSeparate`, `glTexCoordPointer`, `glTexEnvf`,
`glTexEnvfv`, `glTexEnvi`, `glTexEnviv`, `glTexEnvx`, `glTexEnvxv`, `glTexGend`, `glTexGendv`,
`glTexGenf`, `glTexGenfv`, `glTexGeni`, `glTexGeniv`, `glTexImage1D`, `glTexImage2D`,
`glTexImage3D`, `glTexParameterf`, `glTexParameterfv`, `glTexParameteri`, `glTexParameteriv`,
`glTexParameterx`, `glTexParameterxv`, `glTexStorage1D`, `glTexStorage2D`, `glTexStorage3D`,
`glTexSubImage1D`, `glTexSubImage2D`, `glTexSubImage3D`, `glTextureImage1D`, `glTextureImage2D`,
`glTextureImage3D`, `glTextureParameterf`, `glTextureParameterfv`, `glTextureParameteri`,
`glTextureParameteriv`, `glTextureSubImage1D`, `glTextureSubImage2D`, `glTextureSubImage3D`,
`glTranslated`, `glTranslatef`, `glTranslatex`, `glUniform1f`, `glUniform1fv`, `glUniform1i`,
`glUniform1iv`, `glUniform2f`, `glUniform2fv`, `glUniform2i`, `glUniform2iv`, `glUniform3f`,
`glUniform3fv`, `glUniform3i`, `glUniform3iv`, `glUniform4f`, `glUniform4fv`, `glUniform4i`,
`glUniform4iv`, `glUniformMatrix2fv`, `glUniformMatrix3fv`, `glUniformMatrix4fv`, `glUnlockArrays`,
`glUnmapBuffer`, `glUnmapNamedBuffer`, `glUseProgram`, `glUseProgramObject`, `glValidateProgram`,
`glVertexAttrib1d`, `glVertexAttrib1dv`, `glVertexAttrib1f`, `glVertexAttrib1fv`,
`glVertexAttrib1s`, `glVertexAttrib1sv`, `glVertexAttrib2d`, `glVertexAttrib2dv`,
`glVertexAttrib2f`, `glVertexAttrib2fv`, `glVertexAttrib2s`, `glVertexAttrib2sv`,
`glVertexAttrib3d`, `glVertexAttrib3dv`, `glVertexAttrib3f`, `glVertexAttrib3fv`,
`glVertexAttrib3s`, `glVertexAttrib3sv`, `glVertexAttrib4Nbv`, `glVertexAttrib4Niv`,
`glVertexAttrib4Nsv`, `glVertexAttrib4Nub`, `glVertexAttrib4Nubv`, `glVertexAttrib4Nuiv`,
`glVertexAttrib4Nusv`, `glVertexAttrib4bv`, `glVertexAttrib4d`, `glVertexAttrib4dv`,
`glVertexAttrib4f`, `glVertexAttrib4fv`, `glVertexAttrib4iv`, `glVertexAttrib4s`,
`glVertexAttrib4sv`, `glVertexAttrib4ubv`, `glVertexAttrib4uiv`, `glVertexAttrib4usv`,
`glVertexAttribDivisor`, `glVertexAttribIPointer`, `glVertexAttribPointer`, `glVertexPointer`,
`glViewport`
