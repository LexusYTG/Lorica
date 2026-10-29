# Arquitectura

## Qué es Lorica

Lorica traduce OpenGL de escritorio a OpenGL ES. La app hace llamadas GL normales; Lorica las
traduce al vuelo para una GPU que solo habla GLES. Es un fork de gl4es (Sebastien Chevalier, MIT)
con una capa moderna propia llamada GLADIATOR.

## Capas

| Capa | Ruta | Para qué contextos | Backend |
|---|---|---|---|
| Núcleo gl4es | `src/gl/` | Contextos **legacy** (GL 1.x–2.1, perfil compatibility, sin versión) | ES 2.0 |
| GLADIATOR | `src/gl/GLADIATOR/` | Contextos **core** GL ≥ 3.1 | ES 3.0 / 3.1 / 3.2 |
| Front-end GLX | `src/glx/` | Creación de contextos y superficies sobre el EGL real, `glXSwapBuffers`, despacho | — |
| Wrapper EGL (opcional) | `src/egl/` | EGL implementado sobre GLX (`libEGL.so.1`), ver `05-egl-glx.md` | — |
| Puente | `src/glx/gl31_bridge.c` | Decide qué capa atiende cada contexto | — |

Cada contexto se atiende con una sola capa. Un contexto legacy dentro del mismo proceso sigue
reportando GL 2.1 aunque exista otro contexto core.

## Qué versión de GL se anuncia

`glGetString(GL_VERSION)`, `GL_MAJOR_VERSION` y `GL_MINOR_VERSION` devuelven **lo menor entre lo que
el backend permite y el tope fijado por el usuario**. Los contextos legacy siempre reciben 2.1.

| Versión | Requisitos del backend | GLSL anunciado |
|---|---|---|
| 2.1 | ES 2.0 (contexto legacy) | 1.20 |
| 3.1 | ES 3.0 | 1.40 |
| 3.2 / 3.3 | ES 3.2 o extensión de geometry shader | 1.50 / 3.30 |
| 4.0 – 4.2 | ES 3.1 (compute, imágenes) + tessellation, cube map array, sample shading, blend indexado | 4.00 – 4.20 |
| 4.3 | + texture view, copy image, multisample 2D array | 4.30 |

No se llega a 4.4–4.6: hay funciones (`glClipControl`, bindless, sparse) que GLES no puede expresar.
`GL_ARB_direct_state_access` se publica como extensión cuando la versión anunciada es ≥ 3.3, aunque
Lorica no anuncie 4.5.

### Cómo se fija el tope

1. En la capa GLADIATOR el tope por defecto es **3.3** (`gl31_query.c`).
2. El puente lo sube o baja siguiendo `LIBGL_GL` (p. ej. `LIBGL_GL=43`), salvo que se defina
   `LORICA_GL_MAX_VERSION` (p. ej. `4.3`), que manda.
3. En Android/Termux, `lorica_android_defaults()` fija por defecto `LIBGL_GL=43` y `LIBGL_FB=3`
   (pbuffer, ver `05-egl-glx.md`) y las rutas de `libEGL.so` / `libGLESv2.so` del sistema, si el usuario
   no las definió. Fuera de Android, con `LIBGL_GL` sin definir el tope es 2.1 (`LIBGL_GL_COMPAT`
   controla la versión de los contextos que no piden una).
4. En C también se puede usar `gl31_set_max_version(mayor, menor)`.

## Variables de entorno que lee el código

`LIBGL_GL`, `LORICA_GL_MAX_VERSION`, `LIBGL_EGL`, `LIBGL_GLES`, `LIBGL_FB`, `LIBGL_AHB`,
`LIBGL_AHB_SWIZZLE`, `LIBGL_AHB_SIGNALS`, `LIBGL_OCCL`, `LIBGL_OCCL_FALLBACK`, `LIBGL_STRINGS`,
`LIBGL_GL4ES_COMPAT`. Además, el núcleo gl4es hereda las variables `LIBGL_*` de upstream.
Esta lista sale de buscar `getenv` en el código; la semántica detallada de cada una está pendiente
de documentar (salvo `LIBGL_GL`, `LORICA_GL_MAX_VERSION` y `LIBGL_FB`, descritas aquí y en `05-egl-glx.md`).

## El puente GLX ↔ GLADIATOR (`gl31_bridge.c`)

- **Contexto de backend.** Si la app pide GL ≥ 3.1 con `glXCreateContextAttribsARB` (y no perfil ES),
  se crea un `EGLContext` de ES 3.x: prueba 3.2, luego 3.1, luego 3.0 y se queda con el primero que
  el driver acepte. Hace falta una config con `EGL_OPENGL_ES3_BIT`. Si no la hay, cae a ES 2 con un
  aviso y GLADIATOR queda inactivo.
- **Arranque.** `gl31_init` se llama la primera vez que un contexto ES 3.x queda actual
  (`glXMakeCurrent`), con un cargador que busca primero en la librería GLES y luego en
  `eglGetProcAddress`. `gl31_shutdown` se llama al destruir el último contexto ES 3.x.
- **`glGetProcAddress`.** Primero se consulta GLADIATOR, que devuelve la función solo si el contexto
  actual es de **perfil core**, GL ≥ 3.1, con backend ES 3.x y GLADIATOR iniciado. En cualquier otro
  caso se atiende por gl4es. Los nombres con sufijo `ARB` se resuelven al nombre base si está en la
  tabla (`glBindBufferARB` → `glBindBuffer`). El sufijo `EXT` **no** se traduce, porque en varias
  funciones (DSA) cambia la firma.
- **Funciones enlazadas directamente.** Los símbolos GL exportados (`glEnable`, `glDrawArrays`…) son
  un salto indirecto que `glXMakeCurrent`, `glXMakeContextCurrent` y `glXDestroyContext` repuntan:
  a GLADIATOR con un contexto core ≥ 3.1 y a gl4es en cualquier otro caso. Los slots están en la
  sección `lorica_disp`. Se desactiva con `-DLORICA_NO_DISPATCH`. Solo Linux x86_64 y aarch64; en
  builds `-DNOX11` no se activa.
- **Framebuffer por defecto.** Con `LIBGL_FB` distinto de 2 (modos en `05-egl-glx.md`), el framebuffer 0 es la superficie EGL
  real y GLADIATOR lo trata directamente (`GL_BACK*` → `GL_BACK`; `GL_FRONT*` avisa). Con
  `LIBGL_FB=2` el puente **no** crea contextos ES 3.x (cae a ES 2) y GLADIATOR queda inactivo.
- **Presentación cero-copia (AHB).** `glx_ahb.c` implementa presentación con `AHardwareBuffer` +
  DRI3/Present. Solo existe en Android; en otros sistemas son funciones vacías.

## Módulos de GLADIATOR (`src/gl/GLADIATOR/`)

| Archivo | Responsabilidad |
|---|---|
| `gl31.c`, `gl31.h` | Tabla de despacho nombre → implementación, declaración de funciones del backend |
| `gl31_caps.c` | `gl31_init`/`gl31_shutdown`, detección de capacidades y carga de funciones opcionales |
| `gl31_state.c`, `gl31_render.c` | Estado por hilo, `glEnable`/`glDisable`, caps "blandas", estado de rasterizado |
| `gl31_buffer.c`, `gl31_vao.c`, `gl31_attrib.c` | Buffers, vertex arrays, atributos y binding de vértices |
| `gl31_texture.c` | Texturas, samplers, emulación 1D/Rect/BGRA, `glGetTexImage` |
| `gl31_fbo.c` | Framebuffers y renderbuffers |
| `gl31_shader_glsl.c` | Conversor GLSL de escritorio → GLSL ES (incluye reescritura de samplers 1D/Rect) |
| `gl31_shader_link.c`, `gl31_uniform.c` | Linkeo de programas, uniforms, bloques, `glProgramUniform*` |
| `gl31_draw.c` | Draw calls, base vertex/instance, indirectos |
| `gl31_compute.c` | Compute, imágenes, barreras |
| `gl31_xfb.c`, `gl31_query.c`, `gl31_sync.c` | Transform feedback, queries, sync |
| `gl31_gl4.c` | Funciones de GL 4.x (pipelines, texture view, multi-bind, debug…) |
| `gl31_dsa.c` | Direct State Access |
| `gl31_stubs.c` | Funciones que GLES no puede ofrecer (avisan una sola vez) |

Todas las llamadas al backend pasan por la macro `BE(fn)` (`gl31_be.fn`). El estado vive en
almacenamiento local de hilo (`__thread`).

## Compilación

Biblioteca completa (la que se instala como `libGL.so.1`):

```
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DNOX11=OFF -DNOEGL=OFF -DSTATICLIB=OFF -DGBM=OFF
make -j4
```

La salida queda en `lib/libGL.so.1`. `NOX11=OFF` activa GLX, `NOEGL=OFF` activa EGL, `GBM=OFF` omite DRM.
En Termux: `bash scripts/termux-install.sh` (compila, verifica, instala y puede correr LoriTest).

Solo la capa GLADIATOR (`src/gl/GLADIATOR/Makefile`):

| Comando | Resultado |
|---|---|
| `make` | `liblorica_gl31.a` |
| `make SHARED=1` | `liblorica_gl31.so` |
| `make SYSTEM_GLES=1` | Usa los headers GLES del sistema en vez de `third_party/khronos` |
| `make PLATFORM_HEADER=mi_gles.h` | Header de plataforma propio (`LORICA_GL31_PLATFORM_HEADER`) |
| `make test` | Ver `04-bugs-conocidos.md`: hoy apunta a pruebas que no están en el repo |

Uso standalone de GLADIATOR: `gl31_init(loader)` con una función que devuelva direcciones
(`eglGetProcAddress`, `dlsym`…), `gl31_get_proc_address("glNombre")` para resolver funciones y
`gl31_shutdown()` al final. Los errores se reportan con `gl31_set_error()`.

## Pruebas existentes y verificación declarada

- `TEST/LoriTest.c`: prueba principal usada por `scripts/termux-install.sh` (modos `--test` y
  `--test 3 3`, con `LIBGL_GL=43`). Resultado esperado: `RESULTADO: TODO OK`.
- El README principal declara verificación de extremo a extremo en una Mali-G52 MC2 (backend ES 3.2):
  barrido de contextos core 3.1 → 4.3 con crear/dibujar/swap/destruir y transiciones core → legacy → core,
  y aplicaciones reales (Red Eclipse 1.6, Luanti 5.10, SuperTuxKart) a través de Gladiator/Scutum.

## Terceros y licencias

- **Lorica**: GPL-3.0 (`LICENSE`).
- **gl4es** (Sebastien Chevalier y Ryan Hileman): MIT; conserva su copyright (`NOTICE/LICENSE-MIT.txt`).
- **Headers de Khronos** en `src/gl/GLADIATOR/third_party/khronos/`: copia literal, sin modificar, de
  `GLES3/gl3.h`, `GLES3/gl3platform.h`, `GLES2/gl2.h`, `GLES2/gl2platform.h` (OpenGL-Registry, `api/`)
  y `KHR/khrplatform.h` (EGL-Registry, `api/KHR/`). Licencia MIT, indicada en cada cabecera.
- `debian/` es el empaquetado heredado de gl4es; sus README (`README.Debian`, `README.source`) estaban
  vacíos y se eliminaron.
