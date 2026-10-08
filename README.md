![logo](gl4es.png "gl4es logo")

# Lorica

**A fork of [gl4es](https://github.com/ptitSeb/gl4es), taken apart and rebuilt for deep modification.**

Lorica lets programs written for classic desktop OpenGL (1.x and 2.x) run on hardware that only supports OpenGL ES. That is the same job gl4es does. What's different is the intent: Lorica exists to be reshaped.

---

> **Lorica is not gl4es.** It started from gl4es and keeps its credit, license and core ideas, but it is a separate project with its own direction. If you want the stable, general-purpose library, use [upstream gl4es](https://github.com/ptitSeb/gl4es).

## How Lorica differs from the original gl4es

| | **gl4es (upstream)** | **Lorica** |
|---|---|---|
| **Purpose** | A general-purpose, portable library, maintained for many platforms. | A fork made to be modified, with room for severe structural changes. |
| **Code structure** | The original layout and internals. | Being reorganized and reassembled. Don't expect a file-by-file match with upstream. |
| **Following upstream** | Evolves on its own. | Not a mirror. Upstream fixes and changes are not merged automatically. |
| **Platforms** | Pandora, ODROID, Raspberry Pi, Android, iOS, Linux x86/x86_64 and more. | Built around the Gladiator stack. Other platforms are not a goal. |
| **Documentation** | Complete upstream docs. | The upstream docs are a starting point, not a guarantee. Where the code has been restructured, the code wins. |

Everything below this line describes **gl4es behaviour that Lorica inherits**. It was written for upstream and has not been re-verified on Lorica unless stated otherwise.

---

## What it does

Many games and apps are written for desktop OpenGL, but phones, single-board computers and other devices only provide OpenGL ES. Lorica sits in between: the program makes normal OpenGL calls, and Lorica translates them into OpenGL ES on the fly.

It supports most of OpenGL up to 1.5 and a large part of 2.x, and can target either OpenGL ES 2.0 hardware or the older ES 1.1. The focus is compatibility and speed across a wide range of software.

## What has worked (upstream gl4es)

gl4es has been tested with a wide range of software, including Minecraft, OpenMW, Serious Sam, Half-Life 1 and 2, Blender 2.68+, SuperTuxKart 0.8.1, OpenRA, GZDoom, TORCS, and many FNA and MonoGame titles (FEZ, Stardew Valley, Towerfall Ascension and others), plus some Unity3D games.

## Known limitations

**General**
- Reading back the depth or stencil buffer doesn't work.
- `GL_FEEDBACK` mode isn't implemented, and there's no accumulation buffer emulation.
- `GL_SELECT` is limited (it ignores the current depth buffer and bound textures, and custom vertex shaders don't work with it).
- Non-power-of-two textures only work properly with `GL_CLAMP`, unless the hardware supports them natively.
- Multiple colour attachments on a framebuffer aren't supported.
- Occlusion queries exist but report with zero bits of precision.

**OpenGL ES 2.0 backend**
- Shader conversion is basic. Simple shaders work; complex ones may not (for example, implicit float-to-int conversion isn't handled).
- Programs that link only a vertex shader or only a fragment shader aren't supported yet.
- 1D, 3D and rectangle textures don't work in shaders yet (they do in the fixed pipeline), and 3D textures are just a single 2D layer.
- `glxgears` works, but flat shading isn't implemented, so it looks slightly different from real hardware.
- ARB programs are supported (converted to GLSL on the fly). Double-sided lighting, separate specular colour, fog coordinates and secondary colour are supported.

**OpenGL ES 1.1 backend**
- Framebuffers require the `FRAMEBUFFER_OES` extension.
- No double-sided lighting or separate specular colour, and no fog coordinates or secondary colour.
- 3D textures are just a single 2D layer.
- Vertex buffers are emulated even when the driver supports them.

## Installation

Put `lib/libGL.so.1` in your `LD_LIBRARY_PATH`.

Lorica is meant to **replace** any other `libGL` on the system (such as Mesa's), so make sure it's the one that gets loaded.

## Build and usage

The build instructions ([COMPILE.md](COMPILE.md)) and the list of runtime options ([USAGE.md](USAGE.md)) come from upstream gl4es. Options can be set through environment variables or at runtime with `glHint(...)`. Because Lorica is being restructured, check the source if something doesn't match.

## Also from upstream

- **GLU** works normally. A compatible version is available [here](https://github.com/ptitSeb/GLU).
- Screenshots and videos of software that works: [MEDIA.md](MEDIA.md)
- Version history: [CHANGELOG.md](CHANGELOG.md) *(upstream's history; Lorica's own changes are tracked separately)*

## Credits and license

Lorica is based on **gl4es** by **Sebastien "ptitSeb" Chevalier**, which itself grew out of [glshim](https://github.com/lunixbochs/glshim) by lunixbochs. Thank you to both.

As gl4es asks, if you use Lorica or gl4es in your project, please mention gl4es in your README or about page.

Released under the **MIT License**, as upstream.
