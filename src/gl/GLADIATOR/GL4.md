# Lorica GL31 - OpenGL 4.0 a 4.3 sobre GLES 3.1 / 3.2

Se extiende la capa hasta **OpenGL 4.3 core**. No se llega a 4.4-4.6: hay funciones (por ejemplo
`glClipControl`, bindless, sparse) que GLES no puede expresar.

## Version anunciada

`glGetString(GL_VERSION)` / `GL_MAJOR_VERSION` / `GL_MINOR_VERSION` devuelven lo menor entre
lo que el backend permite y el tope del usuario.

| Version | Requisitos del backend | GLSL |
|---|---|---|
| 3.1 | ES 3.0 | 1.40 |
| 3.2 / 3.3 | + geometry shader (ver GL33.md) | 1.50 / 3.30 |
| 4.0 - 4.2 | + ES 3.1 (compute, imagenes), tessellation, cube map array, sample shading, blend indexado | 4.00 / 4.10 / 4.20 |
| 4.3 | + texture view, copy image, multisample 2D array | 4.30 |

**Por defecto el tope es 3.3** (para no romper apps que cambian de camino segun la version).
Con geometry shaders se anuncia 3.3 (GLSL 3.30); `gl31_set_max_version(3, 2)` fuerza 3.2 / 1.50.
Para subirlo: `gl31_set_max_version(4, 3)` o la variable de entorno `LORICA_GL_MAX_VERSION=4.3`
(se lee en `gl31_init`). En ES 3.2 completo (el caso Mali moderno) se llega a 4.3.

## Funcionalidad agregada

* Compute shaders, `glDispatchCompute*`, `glMemoryBarrier*`, `glBindImageTexture`, SSBO, atomic counters.
* Draws indirectos (`DrawArraysIndirect`, `DrawElementsIndirect`, `MultiDraw*Indirect` emulado con bucle).
* Base instance (`glDrawArraysInstancedBaseInstance`, etc.), emulada moviendo offsets de buffers.
* Tessellation (`GL_PATCHES`, `glPatchParameteri`), sample shading, blend por buffer (`*i`).
* Modelo de vertex attrib binding (`glVertexAttribFormat/Binding`, `glBindVertexBuffer(s)`, `glVertexBindingDivisor`).
* Program pipelines, `glCreateShaderProgramv`, 33 `glProgramUniform*`, program binary, program interface query.
* Texture view, `glCopyImageSubData`, `glTexStorage2D/3DMultisample`, `glTexBufferRange`, cube map arrays.
* Multi-bind (`glBindBuffersBase/Range`, `glBindTextures`, `glBindSamplers`, `glBindImageTextures`, `glBindVertexBuffers`).
* KHR_debug, transform feedback objects, invalidate, `glGetInternalformativ`, `glClearBufferData/SubData`.

## Conversor GLSL

* Acepta `#version` 110-150, 330 y 400-460; etapas vertex, fragment, geometry, compute, tess control/eval.
* `double`/`dvecN`/`dmatN` pasan a `float`/`vecN`/`matN` (**se pierde precision**; ES no tiene fp64).
* Se eliminan `gl_PerVertex` redeclarado, `precise` (si ES < 3.2), `subroutine` y `image1D` se rechazan con error claro.
* `binding`, `offset`, `early_fragment_tests` se conservan en ES >= 3.1; `location` entre etapas tambien.

## Limitaciones conocidas

* `glShaderStorageBlockBinding` y `glDrawTransformFeedback*`: `GL_INVALID_OPERATION` con aviso.
* Viewport / scissor / depth range con arrays: solo indice 0 (`GL_MAX_VIEWPORTS` = 1).
* `glClearBufferData`: solo patrones cero y R32F/I/UI.
* Indirect draws: `baseVertex`/`baseInstance` dentro de la estructura los resuelve el backend.
* `glUniform1d`, `glClipControl` y `glUniformSubroutinesuiv` **no se publican** (las apps detectan NULL).
* Calificadores de formato en `image*` sujetos a las restricciones de ES (r32f/r32i/r32ui y rgba*).
* Texture view y copy image requieren ES 3.2 o la extension EXT/OES correspondiente.
* **Nada se probo en una GPU Mali real.** Los tests usan un backend simulado y se corrieron con ASan/UBSan
  (gcc) y compilacion limpia con gcc y clang `-Wall -Wextra`.
