# RENDER

A 3D render engine built from scratch in C++ with OpenGL.

## Status

**Milestone 4 — Optimisation: FPS · Memory · Speed** ✓ done — **Milestone 5 — glTF Geometry Loading** next

## Requirements

- macOS (OpenGL 3.3 Core Profile via system framework)
- CMake ≥ 3.20
- C++17 compiler (Xcode CLT or clang)
- Git (FetchContent pulls dependencies at configure time)

## Build

```bash
cmake --preset dev && cmake --build --preset dev -j
```

Output: `build/dev/renderer`

## Run

Run from the **project root** (shaders load relative to the working directory):

```bash
./build/dev/renderer
```

## Controls

| Key / Input | Action |
|-------------|--------|
| W / A / S / D | Fly forward / left / back / right |
| E | Fly up |
| Q | Fly down |
| Mouse | Look around |
| 1–6 | Switch view mode |
| ESC | Quit |

## View Modes

| Key | Mode |
|-----|------|
| 1 | Diffuse |
| 2 | Wireframe |
| 3 | Depth |
| 4 | Position |
| 5 | Normals |
| 6 | UV |

## Dependencies

Fetched automatically by CMake:

| Library | Version | Purpose |
|---------|---------|---------|
| [GLFW](https://www.glfw.org) | 3.4 | Window + OpenGL context + input |
| [GLM](https://github.com/g-truc/glm) | 1.0.1 | Math (vectors, matrices) |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.91.9 | Debug HUD overlay |

OpenGL and stb_image are provided by the macOS system framework and committed header respectively.

## Architecture

```
src/
├── main.cpp        — render loop, FBO scale, scene setup
├── window.hpp/.cpp — GLFW window + context (Retina-aware)
├── shader.hpp/.cpp — GLSL compile/link, uniform setters
├── camera.hpp/.cpp — filmback/focal-length camera, WASD+QE + mouse look
├── mesh.hpp/.cpp   — VAO/VBO/EBO geometry, cube/plane/sphere factories
├── texture.hpp/.cpp— PNG/JPG/TGA loading via stb_image
├── hud.hpp/.cpp    — Dear ImGui overlay, FrameStats
└── frustum.hpp     — Gribb-Hartmann frustum planes, sphere/AABB culling tests
shaders/
├── basic.vert/frag — MVP transform, 6 view modes
└── blit.vert/frag  — fullscreen upscale blit
```

## Roadmap

| # | Milestone | Status |
|---|-----------|--------|
| 1 | Hello 3D World — window, camera, primitives, diffuse shading | ✓ done |
| 2 | Texture Loading — stb_image, UV coords, Mesh::sphere() | ✓ done |
| 3 | Debug HUD — Dear ImGui overlay, 6 view modes, FBO render scale | ✓ done |
| 4 | Optimisation — smooth FPS, non-stalling GPU timer, GPU-mem tracking, uniform batching, frustum culling | ✓ done |
| 5 | Geometry Loading — glTF 2.0 (cgltf): meshes, materials, textures, scene hierarchy | next |
| 6 | HDRI Skydome — equirectangular HDR sky, HDRI-driven ambient | planned |
| 7 | PBR Shader / Material System — Cook-Torrance BRDF, IBL | planned |
| 8 | Camera & Lens Effects — HDR framebuffer, tone mapping, bloom, DoF | planned |
| 9 | Advanced — OpenEXR I/O, Alembic geometry caches | planned |
