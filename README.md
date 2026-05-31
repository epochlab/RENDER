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
| LMB (click) | Set orbit pivot at screen centre (depth-sampled) |
| LMB (drag) | Tumble camera around pivot |
| 1–9, 0 | Switch view mode |
| H | Toggle HUD overlay |
| B | Toggle sky background (beauty mode) |
| J | Save camera position / rotation to `profile.json` |
| K | Save screenshot to `screenshots/` |
| ESC | Quit |

## View Modes

| Key | Mode |
|-----|------|
| 1 | Beauty (PBR: Fresnel-weighted diffuse + specular IBL) |
| 2 | Wireframe |
| 3 | Alpha |
| 4 | Depth |
| 5 | Position |
| 6 | Normals |
| 7 | UV |
| 8 | Albedo |
| 9 | Diffuse IBL (Fresnel-weighted, no albedo) |
| 0 | Specular IBL (Fresnel-weighted) |
| - | Fresnel (F term — via HUD dropdown) |

## Profile (`profile.json`)

All fields are optional; missing keys fall back to defaults.

```jsonc
{
  "camera": {
    "position":    [x, y, z],   // world-space eye position
    "yaw":         -90.0,       // horizontal rotation in degrees
    "pitch":       0.0,         // vertical rotation in degrees
    "near":        0.1,         // near clip plane (metres)
    "far":         100.0,       // far clip plane (metres)
    "filmback":    35.0,        // sensor width in mm (35 = full-frame)
    "focalLength": 50.0         // focal length in mm
  },
  "render": {
    "scale":      2,            // FBO divisor: 2 → render at BASE/2 resolution
    "width":      2048,         // render resolution width (pixels)
    "height":     1152,         // render resolution height (pixels)
    "iblSamples": 16            // IBL hemisphere sample count
  },
  "shading": {
    "roughness":      0.3,      // GGX roughness: 0 = mirror, 1 = fully diffuse
    "metallic":       0.0,      // 0 = dielectric, 1 = metal
    "ior":            1.5,      // index of refraction (1.5 ≈ plastic/glass)
    "ssaoRadius":     0.5,      // SSAO hemisphere radius in world units
    "ssaoBias":       0.025,    // SSAO depth bias
    "ssaoBlurRadius": 2         // SSAO blur: 1 = 3×3, 2 = 5×5
  },
  "hdri": {
    "path":     "assets/hdr/…", // equirectangular .hdr / .jpg path
    "rotation": [0, 0, 0],      // XYZ Euler degrees applied to sky direction
    "visible":  true,           // draw sky background on startup
    "exposure": 1.0             // linear brightness multiplier
  },
  "scene": {
    "geometry": "assets/geo/…", // glTF 2.0 file path
    "rotation": [0, 0, 0]       // XYZ Euler degrees applied to loaded geometry
  }
}
```

Press **J** at runtime to write the current camera state back to `profile.json`.

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
├── camera.hpp/.cpp — filmback/focal-length camera, LMB orbit (depth-sampled pivot)
├── mesh.hpp/.cpp   — VAO/VBO/EBO geometry, cube/plane/sphere factories
├── texture.hpp/.cpp— PNG/JPG/HDR loading via stb_image (RGBA8 + RGB16F)
├── hud.hpp/.cpp    — Dear ImGui overlay, FrameStats
├── frustum.hpp     — Gribb-Hartmann frustum planes, sphere culling test
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

| Task | Status |
|------|--------|
| Hello 3D World — window, camera, primitives, diffuse shading | ✓ done |
| Texture Loading — stb_image, UV coords, Mesh::sphere() | ✓ done |
| Debug HUD — Dear ImGui overlay, 6 view modes, FBO render scale | ✓ done |
| Optimisation — smooth FPS, non-stalling GPU timer, GPU-mem tracking, frustum culling | ✓ done |
| Geometry Loading — glTF 2.0 (cgltf): meshes, materials, textures, scene hierarchy | ✓ done |
| HDRI Skydome — equirectangular HDR sky, HDRI diffuse irradiance, linear pipeline | ✓ done |
| Project Quality — SSAO, JSON config, render scale fix, Z-up correction, frame capture | ✓ done |
| Orbit Camera — LMB pivot orbit, depth-sampled pivot, diffuse IBL fix, HDRI rotation | ✓ done |
| PBR BSDF — Schlick Fresnel, IOR-derived F0, energy-conserving Ld+Ls, metallic | ✓ done |
| Camera & Lens Effects — exposure, bloom, DoF | planned |
| Advanced — OpenEXR I/O, Alembic geometry caches | planned |
