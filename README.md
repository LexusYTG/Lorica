<div align="center">

# Lorica

**Desktop OpenGL on OpenGL ES. A fork of [gl4es](https://github.com/ptitSeb/gl4es), rebuilt to be modified.**

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

</div>

---

## What Lorica is

Lorica sits between an application that speaks desktop OpenGL and a GPU
that only speaks OpenGL ES. The application makes normal GL calls;
Lorica translates them on the fly.

It started from [gl4es](https://github.com/ptitSeb/gl4es), by Sebastien
Chevalier, and keeps its core translation engine, its credit and its
ideas. Everything on top of that core -- the version negotiation, the
modern-GL layer, the GLX bridge, the DSA shim -- was written for Lorica.

This is not a mirror of upstream. It is not a drop-in replacement for
upstream either. It is a fork with a different goal: run the Gladiator
stack well, and be easy to reshape.

If you want the stable, general-purpose library that runs everywhere,
use [upstream gl4es](https://github.com/ptitSeb/gl4es).

## What it does

- **GLSL translation.** Shaders written for desktop GLSL 1.10 through
  4.60 are rewritten to the highest GLSL ES the backend accepts
  (300 es / 310 es / 320 es). Precision qualifiers are injected,
  removed features are stubbed or rejected with a clear error.
- **Dispatch.** Every GL entry point goes through a lookup table.
  Apps that resolve symbols with `glXGetProcAddress` get the same
  functions as apps that link directly.
- **Version negotiation.** `glGetString(GL_VERSION)` and the
  `GL_MAJOR_VERSION` / `GL_MINOR_VERSION` queries report the version
  the context asked for, capped by what the backend can support.
  Legacy contexts get 2.1. Core contexts get whatever they requested,
  up to 4.3.
- **Modern GL layer.** Compute, SSBO, tessellation, transform feedback
  objects, program pipelines, texture views, `glCopyImageSubData`,
  multi-bind, indirect draws, base instance, DSA -- all forwarded to
  ES 3.1 / 3.2, with fallbacks where ES cannot express them.
- **GLX.** `glXCreateContext`, `glXCreateContextAttribsARB`,
  `glXMakeCurrent`, `glXSwapBuffers`, `glXGetProcAddress`,
  `glXChooseFBConfig`, `glXGetVisualFromFBConfig`. Enough for SDL 2,
  GLFW, and anything else that talks to X11.

## How it is organized

    src/
      gl/              core gl4es: translator, FPE, lists, textures,
                       framebuffers, shaders, state, queries...
      gl/GLADIATOR/    the modern layer: GL 3.1 -> 4.3, DSA (4.5 ext),
                       compute, tess, transform feedback, sync, VAO,
                       shader link, GLSL conversion, capability probing
      glx/             GLX front-end, dispatch table, X11 plumbing
    include/           public headers
    tests/             gl33_core_glx (core context sweep), test_version
    spec/              scripts that generate the extension tables

The `GLADIATOR/` directory is the entry point for anything the core
does not cover. Files named `gl31_*` there implement the 3.x/4.x
surface; `GL32.md`, `GL33.md`, `GL4.md` and `GL45.md` document, version
by version, what is implemented and what is not.

## Supported versions

| Version | Backend required | GLSL announced | Notes |
|---|---|---|---|
| 2.1 | ES 2.0 | 1.20 | Legacy default. |
| 3.1 | ES 3.0 | 1.40 | |
| 3.2 / 3.3 | ES 3.2 or geometry shader extension | 1.50 / 3.30 | |
| 4.0 - 4.2 | ES 3.1 (compute, images) + tessellation | 4.00 - 4.20 | |
| 4.3 | + texture view, copy image, multisample 2D array | 4.30 | |

The ceiling is set per process with `LIBGL_GL=<version>` (for example
`LIBGL_GL=33` or `LIBGL_GL=43`), or at runtime through
`gl31_set_max_version()`. Legacy contexts always get 2.1 regardless of
the ceiling.

`ARB_direct_state_access` is exposed when the announced version is 3.3
or higher, so applications that gate DSA on the extension string see it.

## Verified

End-to-end on a Mali-G52 MC2 through the Gladiator stack (bionic
`scutumd` shim, glibc `libEGL.so`), with a backend that reports ES 3.2:

- Core context sweep `3.1, 3.2, 3.3, 4.0, 4.1, 4.2, 4.3` -- all create,
  draw (`glClear` + `glReadPixels`), swap, then transition
  core -> legacy -> core -> destroy -> new core without breaking.
- Legacy context inside the same process still reports 2.1.
- Real applications: Red Eclipse 1.6, Luanti 5.10, SuperTuxKart, running
  through Scutum with Lorica loaded where the app requests modern GL.

The standalone test is `tests/gl33_core_glx.c`:

    gcc gl33_core_glx.c -o gl33_core_glx -I../include -L../lib -l:libGL.so.1 -lX11
    LIBGL_GL=43 ./gl33_core_glx 3 3

## What it does not do

GLES cannot express a few desktop-GL features. Those are the honest
limits:

- **No `glClipControl`.** ES has no equivalent.
- **No bindless, no sparse residency.**
- **No `glTextureBarrier`** on the current path.
- **64-bit double precision is downgraded to float.**
- **Cube map compressed subimage and getters** return
  `GL_INVALID_OPERATION` in some DSA paths.
- **Viewport / scissor / depth range arrays** only index 0
  (`GL_MAX_VIEWPORTS = 1`).
- **Transform feedback object getters**, `glGetnUniform*`, and
  `glGetVertexArrayIndexed64iv` are not implemented.

The per-version notes in `src/gl/GLADIATOR/GL32.md`, `GL33.md`, `GL4.md`
and `GL45.md` list each of these against the affected function.

## Building

    mkdir -p build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DNOX11=OFF -DNOEGL=OFF -DSTATICLIB=OFF -DGBM=OFF
    make -j4

The resulting shared library lands in `lib/libGL.so.1`. `NOX11=OFF`
enables the GLX front-end; `NOEGL=OFF` enables EGL; `GBM=OFF` skips
DRM.

## Gladiator integration

Inside Gladiator, Lorica is installed as `libGL.so.1` in
`/usr/lib/aarch64-linux-gnu`. The GLX front-end dispatches to EGL at
runtime, and EGL is the Scutum shim, which talks to `scutumd` and from
there to the Mali driver.

Nothing is linked against Scutum at build time. The binding is resolved
by the dynamic loader through `LD_LIBRARY_PATH`. `LIBGL_GL` selects the
version ceiling per application.

## Legal

Lorica is distributed under the **GNU General Public License v3.0**
(see [LICENSE](LICENSE)).

It is a fork of **gl4es**, by **Sebastien Chevalier**, distributed
under the **MIT License**. The gl4es code included here keeps its
original copyright and license terms; the combination is distributed
under GPL-3.0. Full attribution is in
[NOTICE/UPSTREAM.md](NOTICE/UPSTREAM.md).
