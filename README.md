# KODAK

## Requirements

- macOS (OpenGL 3.3 Core Profile via system framework)
- CMake ≥ 3.20
- C++17 compiler

## Build & Run

Run from the **project root** (shaders and assets load relative to the working directory):

```bash
make        # build → ./build/KODAK
make run    # build and run
make clean  # wipe build/
```

## Controls

| Input | Action |
|-------|--------|
| W / A / S / D | Fly forward / left / back / right |
| E / Q | Fly up / down |
| LMB click | Set orbit pivot at screen centre (depth-sampled) |
| LMB drag | Tumble camera around pivot |
| R / G / B | Toggle red / green / blue channel isolation (press again to clear) |
| Y | Toggle luminance AOV (Rec. 709); press again to restore previous AOV |
| I | Toggle invert (`1 − colour`) |
| H | Show / hide HUD stats panel |
| ESC | Quit |

Other actions are in the macOS menu bar:

| Menu | Item | Action |
|------|------|--------|
| File | Close | Quit (Cmd+W) |
| View | Sky Background | Toggle HDRI sky in beauty mode |
| View | Capture | Save screenshot to Desktop |
| View | Set JSON | Write camera + HDRI state to `profile.json` |
| View | Show Panel | Toggle the HUD stats panel |

## HUD Panel

The overlay panel (top-left) shows frame timing, memory, viewport, scene stats, and camera parameters. When hidden, a **◈** button in the top-right corner restores it.

When channel isolation is active (R / G / B hotkey) a coloured label appears in the top-right corner of the viewport — always visible regardless of panel state.

**AOV** dropdown (bottom of panel) — selects the active output channel:

| Mode | Channel |
|------|---------|
| beauty | PBR: Fresnel-weighted diffuse + specular IBL |
| wireframe | Triangle edges |
| bounds | Flat grey geometry + yellow AABB wireframe box (GL_LINES, depth-tested) |
| alpha | Albedo alpha channel |
| depth | Linearised scene depth |
| world_pos | World-space position normalised within scene AABB |
| world_normals | World-space vertex normals |
| uv | UV coordinates |
| albedo | Raw albedo texture |
| direct_diffuse | Full irradiance diffuse lobe |
| direct_refl | Fresnel-weighted reflection lobe |
| shading_normal | TBN-perturbed shading normal |
| ao | SSAO occlusion |
| fresnel | F term — red (facing) → green (grazing) |

**Histogram** — RGB or greyscale channel distribution plotted below the AOV selector:
- Full-RGB passes (beauty, albedo, normals, …) show B/G/R filled curves back-to-front with a white overlap zone where all three channels coincide
- 2-channel passes (UV, Fresnel) show only the active channels; overlap is `min(R,G)`
- Greyscale passes (alpha, luminance, wireframe, depth) show a single grey curve; near-binary passes (alpha) use a full-range peak so endpoint spikes are visible
- Scale is sqrt-normalised against the interior peak (bins 1–254) to prevent background/saturation spikes from dominating; a triangle-weighted kernel replaces the box filter so narrow spikes render as peaked humps rather than flat-topped rectangles

**Frame graph** — instantaneous FPS history in white with a red horizontal line marking the smoothed average.

**HDRI** section (bottom of panel):
- **Y rot slider** (1–360°) — spin the skydome; IBL lighting rotates with it
- **Flip V** — vertically flip the equirectangular panorama

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
    "width":      2048,         // render resolution width (pixels)
    "height":     1152,         // render resolution height (pixels)
    "downsample": 2,            // FBO divisor: 2 → render at BASE/2 on screen
    "iblSamples": 16            // IBL hemisphere sample count
  },
  "hdri": {
    "path":     "assets/hdr/…", // equirectangular .hdr / .jpg path
    "exposure": 1.0,            // linear brightness multiplier
    "rotation": [0, 0, 0],      // XYZ Euler degrees applied to sky direction
    "visible":  true,           // draw sky background on startup
    "flipV":    false           // flip panorama vertically
  },
  "scene": {
    "geometry": "assets/geo/…", // glTF 2.0 file path
    "rotation": [0, 0, 0]       // XYZ Euler degrees applied to loaded geometry
  },
  "shading": {
    "roughness":      0.3,      // GGX roughness: 0 = mirror, 1 = fully diffuse
    "metallic":       0.0,      // 0 = dielectric, 1 = metal
    "ior":            1.5,      // index of refraction (1.5 ≈ plastic/glass)
    "ssaoRadius":     0.5,      // SSAO hemisphere radius in world units
    "ssaoBias":       0.025,    // SSAO depth bias
    "ssaoBlurRadius": 2,        // SSAO blur: 1 = 3×3, 2 = 5×5
    "ssaoHalfRes":    false     // true = half-res SSAO, false = full-res (default)
  }
}
```

Use **View → Set JSON** to write the current camera and HDRI state back to `profile.json`.

## Dependencies

Fetched automatically by CMake:

| Library | Version | Purpose |
|---------|---------|---------|
| [GLFW](https://www.glfw.org) | 3.4 | Window + OpenGL context + input |
| [GLM](https://github.com/g-truc/glm) | 1.0.1 | Math (vectors, matrices) |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.91.9 | HUD overlay |
| [cgltf](https://github.com/jkuhlmann/cgltf) | 1.14 | glTF 2.0 parsing (header-only) |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | JSON config (header-only) |

OpenGL, Cocoa, stb_image, and stb_image_write are provided by the macOS system frameworks and committed headers respectively.

## Architecture

```
src/
├── main.cpp          — render loop, G-buffer, SSAO, config wiring
├── config.hpp/.cpp   — JSON profile loader/saver (nlohmann/json, ordered)
├── window.hpp/.cpp   — GLFW window + context (Retina-aware)
├── shader.hpp/.cpp   — GLSL compile/link, uniform setters
├── camera.hpp/.cpp   — filmback/focal-length camera, LMB orbit (depth-sampled pivot)
├── mesh.hpp/.cpp     — VAO/VBO/EBO geometry, cube/plane/sphere factories
├── texture.hpp/.cpp  — PNG/JPG/HDR loading via stb_image (RGBA8 + RGB16F)
├── hud.hpp/.cpp      — Dear ImGui overlay (crosshair, AOV, HDRI controls, stats)
├── menu_osx.hpp/.mm  — Native macOS menu bar via Cocoa (ObjC++ bridge)
├── frustum.hpp       — Gribb-Hartmann frustum planes, sphere culling test
├── model.hpp/.cpp    — glTF 2.0 loader (cgltf), Model/SubMesh, node transform walk
└── cgltf_impl.cpp    — cgltf single-header implementation unit
shaders/
├── basic.vert/frag   — MVP transform, G-buffer MRT, PBR BSDF, 14 AOV modes
├── line.vert/frag    — flat-colour GL_LINES pass for the bounds AOV bounding box
├── sky.vert/frag     — equirectangular HDRI skydome (rotation, exposure, flip)
├── ssao.vert/frag    — SSAO compute pass (64-sample kernel, depth reconstruction)
├── ssao_blur.frag    — 5×5 box blur on raw SSAO
└── blit.vert/frag    — fullscreen composite with SSAO multiply
profile.json          — runtime scene config (camera, render, HDRI, scene, shading)
```

## Roadmap

| Task | Status |
|-----------|--------|
| Hello 3D World — window, camera, primitives, diffuse shading | ✓ |
| Texture Loading — stb_image, UV coords | ✓ |
| Debug HUD — Dear ImGui overlay, view modes, FBO render scale | ✓ |
| Optimisation — smooth FPS, GPU timer, frustum culling | ✓ |
| Geometry Loading — glTF 2.0 via cgltf | ✓ |
| HDRI Skydome — equirectangular sky, diffuse irradiance, linear pipeline | ✓ |
| Project Quality — SSAO, JSON config, render scale, Z-up | ✓ |
| Orbit Camera — LMB depth-sampled pivot, diffuse IBL fix | ✓ |
| PBR BSDF — Schlick Fresnel, IOR-derived F0, energy-conserving Ld+Ls | ✓ |
| GUI & Debug — native macOS menu, crosshair, HDRI controls, AOV remap | ✓ |
| Render Performance — uniform cache, CPU normal matrix, half-res SSAO, release preset | ✓ |
| Quick fixes — world_pos AABB normalisation, bounds AOV, Sky Background menu toggle | ✓ |
| Build & Run — single-step build and run workflow (./build/KODAK no 'dev' or 'release') | ✓ |
| Logging & Diagnostics — debug logging, warnings, errors, renderer statistics, screenshot metadata | ✓ |
| Performance Profiling (GUI) — render time, rays/sec, samples/sec, memory usage | ✓ |
| Hotkeys — RGBA channel overlay, luminance (Y), invert (I), HUD toggle (H), focal length slider | ✓ |
| AOVs — Reorder, add HSV AOV, RGB histogram in HUD. 2-channel AOV support (UV/Fresnel), histogram artefact fixes (diagonal fringe, endpoint spikes), FPS graph avg overlay. | ✓ |
| HUD - Panel Tabs (1: Viewport, Scene 2: Camera, Lens, HDRI 3: AOV, Histogram 4: GPU, Frame, Memory) | planned |
| Directory Structure — designed for future expansion, procedural development, maintainability, and clean code organization | planned |
| Tests - A full detailed suite of tests and professional engineering to resolve bugs errors overloads and security | planned |
| Color Management — OpenEXR I/O linear pipeline, OCIO ACES workflow w/ sRGB and Rec709 viewing LUTs | planned |
| Camera & Lens Effects — ISO, f-stop, shutter speed, DoF, focus distance, chromatic aberration, anamorphic lenses, aspect ratio, Kelvin-based lighting controls, film grain | planned |
| Shader update — RGB albedo color parameter (white default), indirect (self-reflection, refraction, SSS) | planned |
| Ray Tracing — shadows, area lights, indirect illumination, brute-force path tracing | planned |
| Sampling — adaptive sampling, multiple importance sampling (MIS) | planned |
| Geometry & Shader Library — reusable assets, camera presets, and materials files and presets, scene import/export | planned |
| Test Scenes — teapot, cornell box, three-sphere material test with curved backdrop | planned |
| Future Features — 2d groundplane, alembic (cam and geo), turntable, macbeth ColorChecker, diffusion rendering, cross-platform support (NVIDIA and Apple Silicon) | planned |