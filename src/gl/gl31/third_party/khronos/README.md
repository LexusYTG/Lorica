# Headers de Khronos (sin modificar)

Copia literal de los headers oficiales de OpenGL ES, necesarios para compilar Lorica GL31
sin depender de que el sistema tenga `libgles-dev`:

- `GLES3/gl3.h`, `GLES3/gl3platform.h`, `GLES2/gl2.h`, `GLES2/gl2platform.h`
  — https://github.com/KhronosGroup/OpenGL-Registry (`api/`), licencia MIT (SPDX en cada cabecera).
- `KHR/khrplatform.h`
  — https://github.com/KhronosGroup/EGL-Registry (`api/KHR/`), licencia MIT (texto en la cabecera).

Si preferis usar los del sistema: `make SYSTEM_GLES=1`.
