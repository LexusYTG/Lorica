<div align="center">

<img src="media/lorica-banner.svg" alt="Lorica: desktop OpenGL on OpenGL ES" width="100%">

**Run desktop OpenGL applications on GPUs that only speak OpenGL ES.**

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

</div>

---

## What Lorica is

Lorica sits between an application that speaks desktop OpenGL and a GPU
that only speaks OpenGL ES. The application makes normal GL calls;
Lorica translates them on the fly.

Its core translation engine descends from an MIT-licensed project by
Sebastien Chevalier and Ryan Hileman (see [Legal](#legal)). Everything on
top of that core -- the version negotiation, the modern-GL layer, the GLX
bridge, the DSA shim -- was written for Lorica, with one goal: run the
Gladiator stack well, and be easy to reshape.

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
  multi-bind, indirect draws, base instance, DSA, `glClipControl`,
  `glShaderStorageBlockBinding`, arbitrary draw buffer mapping -- all
  forwarded to ES 3.1 / 3.2, with fallbacks where ES cannot express
  them.
- **GPU instead of CPU.** With ES 3.1, `glReadPixels` (color, depth,
  stencil, any format/type, PBOs), `glGetTexImage`, `GL_BGRA` uploads,
  compressed-texture readback, primitive restart, polygon mode and
  provoking vertex run as compute kernels on the GPU. The CPU is the
  last resort.
- **Vulkan assist (optional).** Where GLES has no answer, a small Vulkan
  1.1 layer helps: GPU timestamps for `GL_TIME_ELAPSED` /
  `glQueryCounter`, and shared memory for persistent `glBufferStorage`.
  It is loaded with `dlopen` and switched off with `LORICA_VK=0`.
- **Fallbacks for weaker backends.** Features a driver lacks (copy
  image, indexed blend, sample shading, vertex binding, draw buffer
  remapping...) are emulated, and `LORICA_DISABLE_CAPS` lets you switch
  backend capabilities off to exercise those paths.
- **GLX.** `glXCreateContext`, `glXCreateContextAttribsARB`,
  `glXMakeCurrent`, `glXSwapBuffers`, `glXGetProcAddress`,
  `glXChooseFBConfig`, `glXGetVisualFromFBConfig`. Enough for SDL 2,
  GLFW, and anything else that talks to X11.

## How it is organized

    src/
      gl/              core translator: FPE, lists, textures,
                       framebuffers, shaders, state, queries...
      gl/GLADIATOR/    the modern layer: GL 3.1 -> 4.3, DSA (4.5 ext),
                       compute, tess, transform feedback, sync, VAO,
                       shader link, GLSL conversion, capability probing
      glx/             GLX front-end, dispatch table, X11 plumbing
      egl/             EGL wrapper (EGL on top of GLX), see "EGL and GLX"
      vk/              optional Vulkan 1.1 assist layer (lvk)
    include/           public headers
    TEST/              LoriTest.c, the end-to-end test program
    tests/hybrid/      GPU-path and Vulkan tests (Mesa, no GPU needed)
    tools/             build_linux.sh (plain gcc build, no CMake)
    scripts/           termux-install.sh (build, verify, install, test)
    spec/              scripts that generate the extension tables
    documentación/     the documentation (one file, in Spanish)

The `GLADIATOR/` directory is the entry point for anything the core
does not cover. Files named `gl31_*` there implement the 3.x/4.x
surface. What is implemented, what is not, and the known limits are
documented in
[`documentación/DOCUMENTACION.md`](documentación/DOCUMENTACION.md).

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

The `tests/hybrid` suite (GPU paths, Vulkan assist, fallbacks) runs on
Mesa with no GPU needed:

    bash tests/hybrid/run.sh

The end-to-end test program is `TEST/LoriTest.c`. On Termux the installer builds
and runs it for you:

    bash scripts/termux-install.sh

or, by hand, against the built library:

    gcc TEST/LoriTest.c -o loritest -Iinclude -Llib -l:libGL.so.1 -lX11 -lm
    LD_LIBRARY_PATH=lib LIBGL_GL=43 ./loritest --test

A passing run prints `RESULTADO: TODO OK`.

## What it does not do

GLES cannot express a few desktop-GL features. Those are the honest
limits:

- **No bindless, no sparse residency.**
- **No `glTextureBarrier`** on the current path.
- **64-bit double precision is downgraded to float.**
- **Cube map compressed subimage and getters** return
  `GL_INVALID_OPERATION` in some DSA paths.
- **`glGetTexImage` on ASTC and BPTC (BC6H/BC7)** textures warns and
  fails; S3TC, RGTC, EAC and ETC2 are decoded on the GPU.
- **`glLogicOp`** has no effect.
- **`glClipControl`** does not remap viewport, scissor or `gl_FragCoord`.
- **Viewport / scissor / depth range arrays** only index 0
  (`GL_MAX_VIEWPORTS = 1`).
- **Transform feedback object getters**, `glGetnUniform*`, and
  `glGetVertexArrayIndexed64iv` are not implemented.

Section 5 of [`documentación/DOCUMENTACION.md`](documentación/DOCUMENTACION.md)
lists each of these against the affected function.

## Building

    mkdir -p build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DNOX11=OFF -DNOEGL=OFF -DSTATICLIB=OFF -DGBM=OFF
    make -j4

The resulting shared library lands in `lib/libGL.so.1`. `NOX11=OFF`
enables the GLX front-end; `NOEGL=OFF` enables EGL; `GBM=OFF` skips
DRM. Add `-DEGL_WRAPPER=ON` to also build `lib/libEGL.so.1` (see below).

Without CMake, on Linux with X11:

    tools/build_linux.sh [output_dir]

## EGL and GLX

Lorica touches EGL and GLX in two opposite directions:

- **GLX on top of EGL (`libGL.so.1`).** The GLX front-end that
  applications link against is implemented with the *real* EGL of the
  device: `glXChooseFBConfig` becomes `eglChooseConfig`,
  `glXCreateContext*` becomes `eglCreateContext`, `glXMakeCurrent`
  creates the EGL surface, `glXSwapBuffers` becomes `eglSwapBuffers`
  (or a pbuffer copy to the X window, depending on `LIBGL_FB`).
- **EGL on top of GLX (`libEGL.so.1`, optional).** Built with
  `-DEGL_WRAPPER=ON`, an EGL library that implements the EGL API with
  Lorica's own `glX*` calls, for programs that ask for desktop OpenGL
  through EGL (`EGL_OPENGL_API`). An `EGLDisplay` is an X11 `Display*`,
  an `EGLConfig` is a `GLXFBConfig`, an `EGLContext` is a `GLXContext`.

Both paths are described in detail in
section 7 of
[`documentación/DOCUMENTACION.md`](documentación/DOCUMENTACION.md).

## Environment variables

| Variable | Effect |
|---|---|
| `LIBGL_GL=NN` | Version ceiling (`33`, `43`...). Legacy contexts stay at 2.1 |
| `LORICA_GL_MAX_VERSION=M.m` | Same, `4.3` style; wins over `LIBGL_GL` |
| `LIBGL_FB=N` | Surface mode: 0 window, 1 framebuffer, 2 FBO (disables the modern layer), 3 pbuffer, 4 GBM |
| `LIBGL_EGL`, `LIBGL_GLES` | Paths to the real EGL and GLES libraries |
| `LIBGL_DEBUG`, `LORICA_DEBUG` | Print the detected backend capabilities |
| `LORICA_TRACE_ERR=1` | Dump the stack where a GL error is raised |
| `LORICA_DISABLE_CAPS=list` | Switch backend capabilities off to test fallbacks |
| `LORICA_VK=0` | Disable the Vulkan assist layer |
| `LORICA_VK_LIB`, `LORICA_VK_DEBUG`, `LORICA_VK_TIMER` | Vulkan library path, tracing, force Vulkan timers |
| `LORICA_BUFSTORE=native\|vk` | Force the `glBufferStorage` path |
| `LORICA_COMPRESSED_SHADOW_MB` | Memory budget for compressed-texture readback copies (default 256) |
| `LIBGL_BANNER=0` | Hide the startup banner |

The full list, with the platform-specific variables, is in section 2 of
[`documentación/DOCUMENTACION.md`](documentación/DOCUMENTACION.md).

## What's new in 1.4.5

- `glClipControl` is now published (`ZERO_TO_ONE` exact, `UPPER_LEFT`
  flips Y and face winding). The GLADIATOR dispatch table now has 572
  functions; the legacy core publishes 640 names of its own (see the docs).
- Arbitrary draw buffer mapping (`glDrawBuffer(GL_COLOR_ATTACHMENTi)`,
  `glDrawBuffers`) through a proxy framebuffer.
- `glShaderStorageBlockBinding` and `glGetCompressedTexImage` are
  implemented (both used to return errors).
- GPU pixel-transfer engine: `glReadPixels` (color, depth, stencil, any
  format/type), `glGetTexImage` (including depth and compressed S3TC /
  RGTC / EAC / ETC2), `GL_BGRA` uploads, `GL_PACK_IMAGE_HEIGHT` /
  `GL_PACK_SKIP_IMAGES`.
- Primitive restart with any index, `glPolygonMode`, provoking vertex,
  `glPointSize`, depth clamp and clip distances, with GPU or shader
  fallbacks where the extension is missing.
- Vulkan assist layer: timer queries without `EXT_disjoint_timer_query`,
  persistent `glBufferStorage` without `EXT_buffer_storage`.
- Fallbacks for weaker backends and `LORICA_DISABLE_CAPS` to test them.
- New `tests/hybrid` suite and `tools/build_linux.sh`.
- Documentation consolidated into a single file; the old numbered notes
  were removed.

## Project goals

Lorica aims at **compatibility and correctness**: making desktop OpenGL
applications run, and being honest about what GLES cannot express. It is
not a performance project, and performance is not measured or tracked in
this repository.

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

Portions derived from earlier work remain under their original
**MIT License**; the required copyright and permission notice is in
[NOTICE/LICENSE-MIT.txt](NOTICE/LICENSE-MIT.txt).

*OpenGL and OpenGL ES are trademarks of their respective owners. Lorica
is an independent project and is not affiliated with or endorsed by
Khronos.*
