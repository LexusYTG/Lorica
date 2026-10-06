# Lorica GL31 — OpenGL 3.1 core sobre GLES 3.0+

Capa que expone la API de OpenGL 3.1 core y la traduce a un backend GLES 3.0 o superior
cargado con un `loader` (`eglGetProcAddress` / `dlsym`).

## Compilar

    make                  # liblorica_gl31.a
    make SHARED=1         # liblorica_gl31.so
    make test             # pruebas con backend simulado (no necesita GPU)
    make SYSTEM_GLES=1    # usar los headers GLES del sistema en vez de third_party/khronos

Requiere solo un compilador C con soporte GNU (`__thread`, `__typeof__`): gcc o clang.
Los headers de Khronos vienen incluidos en `third_party/khronos`.

## Uso

    #include "gl31.h"
    if (gl31_init(eglGetProcAddress) != 0) { /* falta alguna funcion obligatoria del backend */ }
    void* fn = gl31_get_proc_address("glDrawArrays");   /* NULL si no es de GL 3.1 */
    ...
    gl31_shutdown();

Las funciones de GLES 3.0 base son obligatorias; las posteriores (`glEnablei`, `glColorMaski`,
border clamp, `glGetTexLevelParameteriv`...) se cargan solo si la version/extensiones del
backend las ofrecen (`gl31_caps`, `gl31_get_optional()`).

## Estructura

| Archivo | Contenido |
|---|---|
| `gl31.h` | API interna, tabla de funciones del backend, estado, capacidades |
| `gl31.c` | tabla de despacho `gl31_get_proc_address` |
| `gl31_caps.c` | `gl31_init/shutdown`, deteccion de capacidades, carga de opcionales |
| `gl31_state.c` | estado por hilo, errores, samplers |
| `gl31_render.c` | Enable/Disable (caps de desktop sin equivalente en ES), blend/stencil/depth, clear, puntos |
| `gl31_vao.c` `gl31_buffer.c` `gl31_draw.c` `gl31_attrib.c` | vertices, buffers, draw |
| `gl31_texture.c` | texturas, 1D/RECT/1D_ARRAY emulados, TexImage/SubImage/Copy/Compressed, mipmaps, PixelStore |
| `gl31_fbo.c` | framebuffers, renderbuffers, ReadPixels con conversion de formato |
| `gl31_shader_glsl.c` `gl31_shader_link.c` | conversion GLSL 1.10–1.50/3.30 → ES 3.00; objetos shader/programa |
| `gl31_uniform.c` | uniforms, UBO, consultas de programa |
| `gl31_query.c` | queries, render condicional (emulado), `glGetString`/`glGet*v` |
| `gl31_xfb.c` | transform feedback |
| `gl31_stubs.c` | lo que GLES no puede dar: avisa una vez por stderr y, si el resultado seria incorrecto, genera error GL |

## Limitaciones conocidas

- `sampler1D`, `sampler2DRect`, `samplerBuffer`, `gl_ClipDistance` y built-ins del perfil de
  compatibilidad **se rechazan** al compilar el shader (error claro en el info log). No hay reescritura
  de samplers: los targets 1D/RECTANGLE existen para texturas, pero no se pueden muestrear desde GLSL.
- `glTexImage1D`, `glGetTexImage`, `glTexBuffer`, `glClampColor`: stubs (error / aviso).
- `glPolygonMode` solo admite `GL_FILL`; `glLogicOp`, `GL_CLIP_DISTANCEi`: sin efecto.
- Primitive restart solo con el indice fijo del tipo (0xFF/0xFFFF/0xFFFFFFFF).
- Render condicional: se evalua al hacer Begin (no por draw).
- Compresion de texturas en `GL_TEXTURE_1D_ARRAY` / `GL_TEXTURE_RECTANGLE`: no soportada.
- Persistent/coherent mapping (`glBufferStorage`): no soportado.
