# Lorica GL31 - OpenGL 3.1 Core Layer

Lorica es un traductor de OpenGL 3.1 core a GLES 3.0+ que permite utilizar API de OpenGL 3.1 en sistemas que solo tienen soporte para GLES 3.0.

## Características

### Soporte Completo
- **Vertex Array Objects (VAO)** - Gestión de estado de vertex
- **Buffer Objects** - Crear, modificar, mapear buffers
- **Texture Management** - Todas las operaciones de texturas
- **Framebuffer Objects (FBO)** - Renderizado offscreen
- **Shader Programs** - GLSL compilation y linking
- **Uniforms** - Bloques de uniformes (UBO) y uniforms escalares
- **Transform Feedback** - Captura de datos de geometría
- **Queries** - Occlusion queries y time elapsed
- **Samplers** - Objetos de muestreo independientes

### Características Avanzadas
- **Sampler Rewriting** - Traducción automática de `sampler1D`/`sampler2DRect` a `sampler2D`
- **Capability Detection** - Detección automática de extensiones del backend
- **Optional Functions** - Carga dinámica de funciones según capacidades
- **State Management** - Gestión coherente de estado entre API y backend
- **Error Handling** - Manejo completo de errores GL con GL_INVALID_* apropiados

## Integración con GL4ES (estado)

`src/glx/gl31_bridge.c` hace de puente en el lado GLX:

* **Contexto de backend.** Si la app pide GL >= 3.1 con `glXCreateContextAttribsARB` (y no perfil ES), el
  `EGLContext` se crea de ES 3.x: prueba 3.2, 3.1 y 3.0 y se queda con el primero que el driver acepte. Hace falta una
  config con el bit `EGL_OPENGL_ES3_BIT`. Si no hay config o el driver no da ES 3.x, cae a ES 2 con un aviso y
  GLADIATOR queda inactivo. Los contextos legacy (sin versión, o < 3.1) siguen siendo ES 2, igual que antes.
* **Arranque.** `gl31_init` se llama la primera vez que un contexto ES 3.x queda actual (`glXMakeCurrent`), con un
  cargador que busca primero en la librería GLES y luego en `eglGetProcAddress`. El tope de versión sigue a
  `LIBGL_GL`, salvo que se fije `LORICA_GL_MAX_VERSION`. `gl31_shutdown` se llama al destruir el último contexto ES 3.x.
* **Despacho (`glGetProcAddress`).** `gl4es_GetProcAddress` (`src/gl/gl_lookup.c`) pregunta primero a
  `gl4es_gl31_proc_lookup`, que devuelve la función `gl31_*` de la tabla de `gl31.c` solo si el contexto GLX actual es
  de **perfil core**, GL >= 3.1, con backend ES 3.x y GLADIATOR iniciado. En cualquier otro caso (contexto legacy,
  perfil compatibility, backend ES 2) todo sigue por GL4ES como antes. Los nombres con sufijo `ARB` se resuelven al nombre
  base si está en la tabla (`glBindBufferARB` -> `glBindBuffer`); el sufijo `EXT` no se traduce porque en varias
  funciones (DSA) cambia la firma. `spec/gen.py` no cambia: GLADIATOR tiene su propia tabla.
  Funciones enlazadas directamente (paso 3b): los simbolos GL exportados (`glEnable`, `glDrawArrays`, ...) son un
  salto indirecto (`endbr64; jmp *slot` en x86_64; `adrp/ldr/br` en aarch64, sin probar) que `glXMakeCurrent`,
  `glXMakeContextCurrent` y `glXDestroyContext` repuntan (`gl4es_gl31_dispatch_update`): a GLADIATOR con un contexto
  core GL >= 3.1 y a GL4ES en cualquier otro caso. Los slots se registran en la seccion `lorica_disp`. Se desactiva con
  `-DLORICA_NO_DISPATCH`. El repunte es global, igual que `glxContext`: con varios hilos activos sobre contextos de
  distinto tipo gana el ultimo `MakeCurrent`. Solo Linux x86_64/aarch64; en el resto el export sigue siendo un alias a GL4ES.
  En builds `-DNOX11` el despacho no se activa.
* **Framebuffer por defecto.** Sin `LIBGL_FB=2` el framebuffer 0 es la superficie EGL real y GLADIATOR ya lo trata
  (`GL_BACK*` -> `GL_BACK`, sin attach, `GL_FRONT*` avisa); GL4ES no interviene: `saveCurrentFBO`/`restoreCurrentFBO`
  y el volcado de `glXSwapBuffers` son no-ops porque no hay main FBO. Con `LIBGL_FB=2` (`usefbo`) el framebuffer 0 de
  GL4ES es un FBO propio que se vuelca con el pipeline de GL4ES, lo que pisaría el estado de GLADIATOR; en ese modo el
  puente no crea contextos ES 3.x (cae a ES 2 con un aviso) y GLADIATOR queda inactivo. Soportar `usefbo` con GL3+
  exigiría que GLADIATOR trate el main FBO como "framebuffer 0" y que el volcado no toque su estado: no está hecho.

Prueba del puente (EGL falso, sin GPU): `src/glx/tests/test_gl31_bridge.c`.

## Compilación

### Requisitos
- Compilador C (GCC, Clang)
- Headers de OpenGL ES 3.0+ (GLES3/gl3.h o superior)

### Compilar Biblioteca Estática
```bash
make
```

### Compilar Biblioteca Compartida
```bash
make SHARED=1
```

### Instalar
```bash
make install
```

## Uso

### Inicialización
```c
#include "gl31.h"

// Loader que devuelve direcciones de funciones (eglGetProcAddress, dlsym, etc.)
typedef void* (*gl31_loader_fn)(const char* name);

// Inicializar Lorica
if (gl31_init(my_loader_function) != 0) {
    fprintf(stderr, "Error: no se pudo inicializar GL31\n");
    return -1;
}
```

### Obtener Direcciones de Funciones
```c
void* glCreateShader_ptr = gl31_get_proc_address("glCreateShader");
// Usar con dlsym/eglGetProcAddress...
```

### Usar API GL 3.1
```c
// Ahora puedes usar funciones GL 3.1 normales:
GLuint vao;
glGenVertexArrays(1, &vao);
glBindVertexArray(vao);

// Sampler rewriting automático:
// glUniform1i(loc, 0); // con sampler1D -> se traduce a sampler2D internamente
```

### Limpiar
```c
gl31_shutdown();
```

## Arquitectura

### Módulos Principales

- **gl31.c/gl31.h** - Dispatch table y inicialización
- **gl31_state.c** - Gestión de estado thread-local
- **gl31_caps.c** - Detección de capacidades e inyección de funciones opcionales
- **gl31_buffer.c** - Buffer objects y pixel buffers
- **gl31_vao.c** - Vertex array objects
- **gl31_texture.c** - Texturas y samplers
- **gl31_fbo.c** - Framebuffer objects
- **gl31_shader_glsl.c** - Compilación de shaders
- **gl31_shader_link.c** - Linking de programas
- **gl31_shader_rewrite.c** - Reescritura de samplers 1D/RECT
- **gl31_uniform.c** - Uniforms y uniform blocks
- **gl31_render_enhanced.c** - Estado de rendering y capabilities
- **gl31_xfb.c** - Transform feedback
- **gl31_query.c** - Occlusion queries
- **gl31_stubs.c** - Funciones no soportadas en GLES

### Mapeo a Backend (GLES)

Todas las llamadas al backend pasan por la macro `BE()`:
```c
#define BE(fn) gl31_be.fn  // Acceso a función del backend
```

## Limitaciones Conocidas

### No Soportado en GLES 3.0
- ✗ Texture Buffer Objects (`glTexBuffer`) - Requiere GLES 3.1 o extensión
- ✗ Transform Feedback (sin buffers) - Requiere backend compatible
- ✗ Shader Storage Buffer Objects (SSBO) - No disponible en GLES 3.0
- ✗ Compute Shaders - No disponible en GLES 3.0
- ✗ Double precision floats - Solo en desktop GL

### Traducción Automática
- ✓ `sampler1D` → `sampler2D` (adaptación de coordenadas)
- ✓ `sampler1DArray` → `sampler2DArray`
- ✓ `sampler2DRect` → `sampler2D` (coordinadas normalizadas)
- ✗ `sampler1DShadow` - No soportado (ambiguo)
- ✗ `sampler2DMSArray` - No existe en GLES 3.0

### Funciones Stub (Aviso pero funcionan)
- `glTexImage1D()` - Genera advertencia, no hace nada
- `glGetTexImage()` - Genera advertencia, requeriría FBO temporal
- `glPolygonMode()` - Solo GL_FILL disponible en GLES
- `glDrawBuffer()` - GLES solo tiene GL_BACK

## Detección de Extensiones

Lorica automáticamente detecta y carga extensiones disponibles:

```c
gl31_caps_t caps = gl31_caps;  // Estructura global
printf("GLES %d.%d\n", caps.es_major, caps.es_minor);
printf("Texture Buffer: %s\n", caps.tex_buffer ? "sí" : "no");
printf("Draw Instanced: %s\n", caps.draw_instanced ? "sí" : "no");
printf("Border Clamp: %s\n", caps.border_clamp ? "sí" : "no");
```

## Thread Safety

Lorica usa `__thread` para almacenamiento local de threads. Cada thread tiene su propio estado GL.

## Compilación Condicional

Definir `LORICA_GL31_PLATFORM_HEADER` para usar un header custom:
```bash
gcc -DLORICA_GL31_PLATFORM_HEADER=\"my_platform.h\" -c gl31.c
```

## Debugging

Para más información de debug, compilar con:
```bash
CFLAGS="-g -DDEBUG" make clean all
```

Los errores de GL se reportan automáticamente mediante `gl31_set_error()`.

## Licencia

[Especificar según corresponda]

## Autor

[Contribuciones de la comunidad]
