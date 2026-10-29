# Documentación de Lorica

Esta carpeta reemplaza a todas las notas sueltas que había repartidas por el proyecto
(`src/gl/GLADIATOR/README.md`, `GL32.md`, `GL33.md`, `GL4.md`, `GL45.md`,
`third_party/khronos/README.md` y los `README.*` de `debian/`). El único README que queda es
el de la raíz.

| Archivo | Contenido |
|---|---|
| `01-arquitectura.md` | Capas, versiones de GL anunciadas, requisitos del backend, variables de entorno, despacho, compilación, terceros |
| `02-funciones-implementadas.md` | Qué está implementado y cómo se resuelve cada área; índice de las 571 funciones publicadas |
| `03-no-implementado-y-limitaciones.md` | Lo que no existe, lo que es stub y las limitaciones de lo que sí existe |
| `04-bugs-conocidos.md` | Bugs, inconsistencias y problemas conocidos del repo, con su estado |
| `05-egl-glx.md` | Cómo se traducen GLX → EGL (`libGL.so.1`) y EGL → GLX (`libEGL.so.1`, opcional) |

## Objetivo del proyecto

Lorica busca **compatibilidad y corrección**: que las aplicaciones de OpenGL de escritorio funcionen y
decir con honestidad lo que GLES no puede expresar. No es un proyecto de rendimiento: aquí no se mide
ni se compara velocidad, y ninguna decisión de diseño se justifica por ella.

## Cómo leer los estados

- **Nativo**: se reenvía al backend GLES sin cambios de comportamiento.
- **Emulado**: Lorica lo reconstruye con otras llamadas de GLES; funciona, con costo o con casos límite.
- **Parcial / con aviso**: funciona solo en algunos casos; en los demás avisa por `stderr` y/o genera un error GL.
- **Stub**: se publica pero no hace nada (avisa una vez por función).
- **No publicado**: `glGetProcAddress` devuelve `NULL` para que la app detecte que no existe.

## De dónde sale la información

- La lista de funciones sale de la tabla de despacho `src/gl/GLADIATOR/gl31.c` (571 entradas).
- Los stubs y avisos salen de las llamadas a `gl31_stub_warn()` en `src/gl/GLADIATOR/*.c`.
- Los requisitos de backend, emulaciones y limitaciones vienen de las notas antiguas, contrastadas
  con el código y con el README principal.
- Todo esto es lectura del código y de las notas: **no se compiló ni se ejecutó nada para
  escribirlo**. Donde una nota antigua contradecía a otra, se indica en `04-bugs-conocidos.md`.

Fecha de esta revisión: 2026-10-28.
