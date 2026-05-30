# BOUNCE

## Requirements

- macOS (OpenGL 3.3 Core Profile via system framework)
- CMake ≥ 3.20
- C++17 compiler

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
| Space (hold) | Enable mouse look |
| Mouse | Look around (while Space held) |
| 1–9 | Switch view mode |
| H | Toggle HUD overlay |
| K | Save screenshot to `screenshots/` |
| ESC | Quit |

## View Modes

| Key | Mode |
|-----|------|
| 1 | Diffuse (albedo × HDRI irradiance × AO) |
| 2 | Wireframe |
| 3 | Alpha |
| 4 | Depth |
| 5 | Position |
| 6 | Normals |
| 7 | UV |
| 8 | Irradiance |
| 9 | AO (SSAO) |

## Dependencies

Fetched automatically by CMake:

| Library | Version | Purpose |
|---------|---------|---------|
| [GLFW](https://www.glfw.org) | 3.4 | Window + OpenGL context + input |
| [GLM](https://github.com/g-truc/glm) | 1.0.1 | Math (vectors, matrices) |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.91.9 | Debug HUD overlay |
| [cgltf](https://github.com/jkuhlmann/cgltf) | 1.14 | glTF 2.0 parsing (header-only) |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | JSON config parser (header-only) |

OpenGL, stb_image, and stb_image_write are provided by the macOS system framework and committed headers respectively.

## Architecture

```
src/
├── main.cpp        — render loop, G-buffer, SSAO, config wiring
├── config.hpp/.cpp — JSON profile loader (nlohmann/json)
├── window.hpp/.cpp — GLFW window + context (Retina-aware)
├── shader.hpp/.cpp — GLSL compile/link, uniform setters
├── camera.hpp/.cpp — filmback/focal-length camera, Space-hold mouse look
├── mesh.hpp/.cpp   — VAO/VBO/EBO geometry, cube/plane/sphere factories
├── texture.hpp/.cpp— PNG/JPG/HDR loading via stb_image (RGBA8 + RGB16F)
├── hud.hpp/.cpp    — Dear ImGui overlay, FrameStats
├── frustum.hpp     — Gribb-Hartmann frustum planes, sphere/AABB culling tests
├── model.hpp/.cpp  — glTF 2.0 loader (cgltf), Model/SubMesh, node transform walk
└── cgltf_impl.cpp  — cgltf single-header implementation unit
shaders/
├── basic.vert/frag  — MVP, G-buffer MRT output, 9 view modes
├── sky.vert/frag    — equirectangular HDRI skydome (rotation + exposure)
├── ssao.vert/frag   — SSAO compute pass (hemisphere kernel, depth reconstruction)
├── ssao_blur.frag   — 5×5 box blur on SSAO result
└── blit.vert/frag   — fullscreen blit with SSAO composite
profile.json         — runtime scene config (camera, render scale, HDRI, geometry paths)
```

## Roadmap

| # | Milestone | Status |
|---|-----------|--------|
| 1 | Hello 3D World — window, camera, primitives, diffuse shading | ✓ done |
| 2 | Texture Loading — stb_image, UV coords, Mesh::sphere() | ✓ done |
| 3 | Debug HUD — Dear ImGui overlay, 6 view modes, FBO render scale | ✓ done |
| 4 | Optimisation — smooth FPS, non-stalling GPU timer, GPU-mem tracking, uniform batching, frustum culling | ✓ done |
| 5 | Geometry Loading — glTF 2.0 (cgltf): meshes, materials, textures, scene hierarchy | ✓ done |
| 6 | HDRI Skydome — equirectangular HDR sky, HDRI diffuse irradiance, linear pipeline | ✓ done |
| 7 | Project Quality — SSAO, JSON config, render scale fix, Z-up correction, frame capture | in progress |
| 8 | PBR Shader / Material System — Cook-Torrance BRDF, IBL, ORM maps | planned |
| 9 | Camera & Lens Effects — exposure, bloom, DoF | planned |
| 10 | Advanced — OpenEXR I/O, Alembic geometry caches | planned |
