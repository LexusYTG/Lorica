# EGL y GLX en Lorica

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

## A. GLX → EGL (`src/glx/glx.c`)

Es la parte que ya usan las apps GLX normales (SDL, GLFW, Red Eclipse…). El EGL que usa es el **real**
del sistema, cargado con `LOAD_EGL` desde `libEGL` (ruta forzable con `LIBGL_EGL`; la librería GLES con
`LIBGL_GLES`).

### Arranque
1. `init_display`: `eglGetDisplay(EGL_DEFAULT_DISPLAY)`. Con GBM (`LIBGL_FB=4`) el display sale de
   `OpenGBMDisplay`; con HYBRIS, de `eglGetPlatformDisplay(ANDROID)`.
2. `InitEGL`: `eglBindAPI(EGL_OPENGL_ES_API)` + `eglInitialize`. Se hace una sola vez.
3. `init_eglconfig`: pide a EGL **todas** las configs con `EGL_RENDERABLE_TYPE = ES2` y construye la
   tabla de `GLXFBConfig`. Cada `EGLConfig` genera **dos** entradas (sin doble buffer y con doble
   buffer), indexadas por `EGL_CONFIG_ID` y `EGL_CONFIG_ID + max`.

### Correspondencia de funciones

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

### Versión de ES del contexto de backend
- **Contexto GL ≥ 3.1** (`glXCreateContextAttribsARB`): `gl4es_gl31_create_backend_context` prueba ES 3.2,
  luego 3.1, luego 3.0 sobre una config con el bit ES3; si ninguna funciona cae a ES 2 y GLADIATOR queda
  inactivo (con un `LOGE`). Detalle en `01-arquitectura.md`.
- **Contexto legacy** (`glXCreateContext`): `eglCreateContext` con `EGL_CONTEXT_CLIENT_VERSION` = 3 si
  `LIBGL_GL ≥ 31` o existe `LORICA_GL_MAX_VERSION`, y 2 en caso contrario. Se hace así a propósito: si el
  backend naciera en ES 2, `hardext` reportaría capacidades de ES 2 (1 draw buffer) mientras la app ve
  "GL 3.3" y asume los mínimos de GL 3.

### Modos de superficie (`LIBGL_FB`)

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

### Presentación (`glXSwapBuffers`)
1. Vuelca texturas pendientes, display lists y bitmaps de gl4es.
2. Con `LIBGL_FB=2`: desenlaza y vuelca el FBO principal.
3. Con pbuffer: `glFinish` + copia a la ventana. Si no: `eglSwapBuffers(display, surface)`.
4. Si `LIBGL_FPS` está activo, actualiza el contador en pantalla.

---

## B. EGL → GLX (`src/egl/`, opcional)

Es un `libEGL.so.1` propio que implementa la API de EGL llamando a `glX*` de Lorica. Sirve para
programas que piden **OpenGL de escritorio mediante EGL** (`eglBindAPI(EGL_OPENGL_API)`), por ejemplo
toolkits que usan EGL en X11.

### Compilación y carga
- Se compila solo con `-DEGL_WRAPPER=ON` (target `EGL` en `src/CMakeLists.txt`, enlazado contra `GL`).
  `scripts/termux-install.sh` **no** lo pide: hay que añadir esa opción a `cmake`.
- Genera `lib/libEGL.so.1`. El binario que hay en `lib/` depende de `libGL.so.1` y de X11, como
  corresponde a este wrapper.
- Al primer uso (`pthread_once`) hace `dlopen("libGL.so.1")` y resuelve `glX*` con `dlsym` y
  `glXGetProcAddress`; las funciones de X11 salen de `libGL` o, si no, de `libX11`.
- ⚠ Conviene fijar `LIBGL_EGL` a la ruta absoluta del EGL real: si el cargador resolviera `libEGL` al
  propio wrapper, la dirección A intentaría usarlo como backend.

### Correspondencia de tipos

| EGL | Es en realidad |
|---|---|
| `EGLDisplay` | `Display*` de X11 (`eglGetDisplay(NULL)` hace `XOpenDisplay(NULL)`) |
| `EGLConfig` | `GLXFBConfig` |
| `EGLContext` | `GLXContext` |
| `EGLSurface` (ventana) | El `XID` de la `Window` |
| `EGLSurface` (pbuffer) | El `XID` del `GLXPbuffer` con el bit 40 activo (`1UL << 40`) para distinguirlo |

### Correspondencia de funciones

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

### Atributos de config (EGL → GLX)

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

### Extensiones anunciadas
- Sin display: `EGL_KHR_client_get_all_proc_addresses`, `EGL_EXT_platform_base`,
  `EGL_EXT_platform_x11`, `EGL_KHR_platform_x11`.
- Con display: `EGL_KHR_create_context`, `EGL_KHR_create_context_no_error`,
  `EGL_KHR_surfaceless_context`, `EGL_KHR_no_config_context`, `EGL_KHR_get_all_proc_addresses`,
  `EGL_KHR_fence_sync`, `EGL_KHR_wait_sync`, `EGL_KHR_image_base`.

### Qué no hace
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
