# Bugs conocidos e inconsistencias

No hay una lista formal de issues en la copia revisada. Lo que sigue sale de las notas antiguas del
proyecto y de la revisión del repositorio. Estado: **Abierto** (hay que corregirlo), **Diseño**
(limitación asumida, ver `03`) o **Resuelto** (corregido por esta documentación).

## A. Problemas del repositorio

| ID | Problema | Estado |
|---|---|---|
| R-01 | **Pruebas referenciadas que no están en el repo.** `make test` en `src/gl/GLADIATOR/Makefile` apunta a `TEST/gladiator/{test_uniform,test_integration,test_gl32,test_gl33,test_gl4,test_dsa}.c`; las notas antiguas citaban `TEST/bridge/test_gl31_bridge.c` y `tests/`; el `CMakeLists.txt` raíz referencia `TEST/legacy/test.cmake`. En la copia revisada solo existe `TEST/LoriTest.c`. Resultado: `make test` y los tests de CMake fallan. | Abierto |
| R-02 | **README principal desactualizado.** Listaba `tests/` y `TEST/glx/gl33_core_glx.c`, que no existen, y no mencionaba `scripts/termux-install.sh`, `TEST/LoriTest.c` ni el wrapper EGL. | Resuelto (README actualizado) |
| R-03 | **Versión y nombres de gl4es.** `version.h` dice 1.1.7 con guard `_GL4ES_VERSION_H`; `CMakeLists.txt` declara `project(gl4es)` y las opciones hablan de gl4es. | Abierto |
| R-05 | **`scripts/termux-install.sh` es agresivo con Mesa.** `rm -f "$PREFIX/lib"/libGL.so.1.[0-9]*` borra los `libGL.so.1.x` versionados del sistema, y `ln -sfn` reemplaza `libGL.so.1` / `libGL.so`. Una actualización de paquetes puede revertirlo. Existe `--no-install` para evitarlo. | Abierto |
| R-07 | **Comentario y código no coinciden** en `src/gl/init.c`: el comentario dice "default 3.2", pero con `LIBGL_GL` sin definir el código fija 2.1 (1.5 en ES 1). En Android el valor real lo pone `lorica_android_defaults()` (4.3). | Abierto |
| R-08 | **El script de instalación no compila `libEGL.so.1`.** El wrapper EGL → GLX solo se genera con `-DEGL_WRAPPER=ON`, y `termux-install.sh` no lo pasa. | Abierto |
| R-06 | **Artefactos compilados dentro del árbol** (`*.o` en `src/gl/GLADIATOR/`, `liblorica_gl31.a`, `TEST/loritest/build/`, `lib/`). El `.gitignore` los excluye de git; solo ensucian copias y archivos comprimidos. | Menor |

## A2. Wrapper EGL → GLX (`src/egl/`)

| ID | Problema | Estado |
|---|---|---|
| E-01 | **Extensiones anunciadas sin respaldo.** Anuncia `EGL_KHR_image_base`, pero `eglCreateImage` siempre falla; anuncia `EGL_KHR_create_context_no_error`, y no se comprobó que haga algo con ese flag. | Abierto |
| E-02 | **Sync ficticio.** `eglCreateSync`/`eglCreateSyncKHR` devuelven un objeto falso y `eglClientWaitSync` hace `glXWaitGL`: no hay fences reales. | Diseño |
| E-03 | **Superficie de lectura ignorada** en `eglMakeCurrent` (GLX solo recibe la de dibujo). | Diseño |
| E-04 | **`eglGetConfigAttrib` nunca devuelve `EGL_BAD_ATTRIBUTE`:** los atributos desconocidos devuelven 0 con éxito. | Abierto |
| E-05 | **Sin pixmaps ni `eglCopyBuffers`.** | Diseño |

## B. Notas antiguas contradictorias (resueltas aquí)

| ID | Nota antigua | Qué prevalece |
|---|---|---|
| N-01 | `GL32.md`: "ninguna llamada de una app llega a las funciones `gl31_*`"; pasos 3 y 4 del puente "pendientes" | **Obsoleto.** El README de GLADIATOR documentaba el despacho por contexto, los trampolines y el framebuffer por defecto como hechos (ver `01`). |
| N-02 | Varias notas: "nada se probó sobre una GPU Mali real" | **Obsoleto.** El README principal declara verificación en Mali-G52 MC2 (ES 3.2) y apps reales. |
| N-03 | README de GLADIATOR: "`glTexImage1D` genera advertencia y no hace nada" | `GL32.md` (posterior) dice que las texturas 1D se emulan como 2D. Se documenta la emulación; falta confirmar `glTexImage1D` con una prueba. |
| N-04 | README de GLADIATOR: TBO, SSBO, compute, `sampler2DMSArray` "no soportados" | Válido **solo con ES 3.0**. Con ES 3.1/3.2 sí (ver `03`, sección 4). |
| N-05 | README de GLADIATOR: módulos `gl31_shader_rewrite.c` y `gl31_render_enhanced.c` | No existen; la reescritura de samplers vive en `gl31_shader_glsl.c` y el estado de rasterizado en `gl31_render.c`. |
| N-06 | README de GLADIATOR: "Licencia: [Especificar]", "Autor: [Contribuciones de la comunidad]" | Marcadores sin rellenar. La licencia es GPL-3.0 (`LICENSE`). |
| N-07 | Tope de versión: 3.3 por defecto (`GL4.md`, `gl31_query.c`) frente a "hasta 4.3" (README principal) | Ambos son ciertos: el tope de GLADIATOR es 3.3, pero el puente lo sigue a `LIBGL_GL`, y en Android/Termux `LIBGL_GL=43` viene por defecto (ver `01`). |

## C. Pendientes de verificar

| ID | Tema | Detalle |
|---|---|---|
| V-01 | Trampolines aarch64 | Las notas antiguas decían que el salto `adrp/ldr/br` en aarch64 estaba "sin probar". Como la verificación declarada se hizo en Mali-G52 (aarch64), conviene confirmar que esa prueba pasó por los trampolines (`lorica_disp`) y no con `-DLORICA_NO_DISPATCH`. |
| V-02 | `glTexImage1D` | Ver N-03. |
| V-03 | Variables `LIBGL_AHB*`, `LIBGL_OCCL*`, `LIBGL_STRINGS`, `LIBGL_GL4ES_COMPAT` | Existen en el código; falta documentar su semántica. |

## D. Divergencias de comportamiento respecto a GL de escritorio

Son limitaciones de diseño (estado **Diseño**); el detalle de cada una está en
`03-no-implementado-y-limitaciones.md`.

- `glTexImage2DMultisample` repetido sobre la misma textura da `GL_INVALID_OPERATION`.
- `glFramebufferTexture` sin geometry shaders adjunta solo la capa 0.
- `GL_DEPTH_CLAMP`, `GL_TEXTURE_CUBE_MAP_SEAMLESS` y `GL_CLIP_DISTANCEi` no llegan al backend.
- Base vertex emulado modifica punteros de atributos en cada draw.
- Estado compartido entre contextos sin locking; despacho global con varios hilos.
- `double` pasa a `float` en GLSL.
- `LIBGL_FB=2` desactiva GLADIATOR.
