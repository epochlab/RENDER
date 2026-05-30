# RENDER

A 3D render engine built from scratch in C++ with OpenGL.

## Status

**Milestone 1 — Hello 3D World** ✓ complete

## Requirements

- macOS (OpenGL 3.3 Core Profile via system framework)
- CMake ≥ 3.20
- C++17 compiler (Xcode CLT or clang)
- Git (FetchContent pulls GLFW and GLM at configure time)

## Build

```bash
cmake --preset dev && cmake --build --preset dev -j
```

Output: `build/dev/renderer`

## Run

Run from the **project root** (shaders are loaded relative to the working directory):

```bash
./build/dev/renderer
```

## Controls

| Key / Input     | Action              |
|-----------------|---------------------|
| W / A / S / D   | Fly forward/left/back/right |
| Space           | Fly up              |
| Left Shift      | Fly down            |
| Mouse           | Look around         |
| ESC             | Quit                |

## Dependencies

Fetched automatically by CMake — no manual installs needed:

| Library | Version | Purpose |
|---------|---------|---------|
| [GLFW](https://www.glfw.org) | 3.4 | Window + OpenGL context + input |
| [GLM](https://github.com/g-truc/glm) | 1.0.1 | Math (vectors, matrices) |

OpenGL is provided by the macOS system framework.

## Architecture

```
src/
├── main.cpp        — render loop, scene setup
├── window.hpp/.cpp — GLFW window + context
├── shader.hpp/.cpp — GLSL compile/link, uniform setters
├── camera.hpp/.cpp — perspective camera, WASD + mouse look
└── mesh.hpp/.cpp   — VAO/VBO/EBO geometry, cube/plane factories
shaders/
├── basic.vert      — MVP transform
└── basic.frag      — directional diffuse shading
```

## Roadmap

| # | Milestone | Status |
|---|-----------|--------|
| 1 | Hello 3D World — window, camera, primitives, diffuse shading | ✓ done |
| 2 | Texture Loading — PNG/JPG/TGA via stb_image, UV coords, mipmaps | planned |
| 3 | Geometry Loading — OBJ (tinyobjloader) + glTF 2.0 (cgltf) | planned |
| 4 | HDRI Skydome — equirectangular HDR sky, HDRI-driven ambient | planned |
| 5 | PBR Shader / Material System — Cook-Torrance BRDF, IBL | planned |
| 6 | Camera & Lens Effects — HDR framebuffer, tone mapping, bloom, DoF | planned |
| 7 | Advanced — OpenEXR I/O, Alembic geometry caches | planned |
