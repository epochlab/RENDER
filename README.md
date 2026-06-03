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

## Testing

```bash
# Configure + build (includes tests_kodak binary)
cmake --preset default && cmake --build --preset default -j

# Run all 39 tests via CTest
ctest --preset default --output-on-failure

# Run directly for coloured ✓ / ✗ output per test
./build/tests_kodak
```

Tests run headless — no display or GPU session required. Each test case registers individually in CTest so `ctest -R <pattern>` filters by name (e.g. `ctest -R Camera`).

| Suite | Tests | Coverage |
|-------|-------|---------|
| Config | 7 | JSON load/save, per-key defaults, malformed input, round-trip |
| Camera | 9 | FOV from filmback/focal length, projection depth terms, viewMatrix orthonormality, front direction, pitch clamp |
| Frustum | 8 | Plane extraction, unit-length normals, sphere inside/outside/boundary |
| Mesh | 6 | Bounding radius, AABB, index/triangle counts, move semantics |
| Shader | 4 | Compile success, syntax error throws, missing file throws |
| Texture | 5 | `white()` and `flatNormal()` pixel correctness, bind unit, move semantics |

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
| View | Capture | Save screenshot to Desktop |
| View | Set JSON | Write camera position, focal length, and HDRI rotation to `scene.json` |
| View | Show/Hide HUD | Toggle the HUD stats panel |

## HUD Panel

The overlay panel (top-left) shows frame timing, memory, viewport, scene stats, and camera parameters. When hidden, a **◈** button in the top-right corner restores it.

When channel isolation is active (R / G / B hotkey) a coloured label appears in the top-right corner of the viewport — always visible regardless of panel state.

**AOV** dropdown (bottom of panel) — selects the active output channel:

| Mode | Channel |
|------|---------|
| beauty | PBR: Fresnel-weighted diffuse + specular IBL |
| alpha | Albedo alpha channel |
| bounds | Flat grey geometry + yellow AABB wireframe box (GL_LINES, depth-tested) |
| wireframe | Triangle edges |
| depth | Linearised scene depth |
| albedo | Raw albedo texture |
| hsv | Hue / saturation / value decomposition |
| luminance | Rec. 709 greyscale |
| direct_diffuse | Full irradiance diffuse lobe |
| direct_refl | Fresnel-weighted reflection lobe |
| world_pos | World-space position normalised within scene AABB |
| world_normals | World-space vertex normals |
| uv | UV coordinates |
| shading_normal | TBN-perturbed shading normal |
| fresnel | F term — red (facing) → green (grazing) |
| occlusion | SSAO occlusion |

**Histogram** — RGB or greyscale channel distribution plotted below the AOV selector:
- Full-RGB passes (beauty, albedo, normals, …) show B/G/R filled curves back-to-front with a white overlap zone where all three channels coincide
- 2-channel passes (UV, Fresnel) show only the active channels; overlap is `min(R,G)`
- Greyscale passes (alpha, luminance, wireframe, depth) show a single grey curve; near-binary passes (alpha) use a full-range peak so endpoint spikes are visible
- Scale is sqrt-normalised against the interior peak (bins 1–254) to prevent background/saturation spikes from dominating; a triangle-weighted kernel replaces the box filter so narrow spikes render as peaked humps rather than flat-topped rectangles

**Frame graph** — instantaneous FPS history in white with a red horizontal line marking the smoothed average.

**HDRI** section (bottom of panel):
- **Y rot slider** (1–360°) — spin the skydome; IBL lighting rotates with it
- **Flip V** — vertically flip the equirectangular panorama

## Config files

All fields are optional; missing keys fall back to defaults.

### `profile.json` — renderer settings

Edited manually. Never written at runtime.

```jsonc
{
  "render": {
    "width":      2048,         // render resolution width (pixels)
    "height":     1152,         // render resolution height (pixels)
    "downsample": 2,            // FBO divisor: 2 → render at BASE/2 on screen
    "iblSamples": 16            // IBL hemisphere sample count
  },
  "shading": {
    "ior":            1.5,      // index of refraction (1.5 ≈ plastic/glass)
    "ssaoRadius":     0.5,      // SSAO hemisphere radius in world units
    "ssaoBias":       0.025,    // SSAO depth bias
    "ssaoBlurRadius": 2,        // SSAO blur: 1 = 3×3, 2 = 5×5
    "ssaoHalfRes":    false     // true = half-res SSAO, false = full-res (default)
  }
}
```

### `scene.json` — scene content

Updated by **View → Set JSON** (camera position, focal length, HDRI rotation only).

```jsonc
{
  "camera": {
    "position":    [x, y, z],   // world-space eye position
    "yaw":         -90.0,       // horizontal rotation in degrees
    "pitch":       0.0,         // vertical rotation in degrees
    "near":        0.1,         // near clip plane (metres)
    "far":         100.0,       // far clip plane (metres)
    "filmback":    35.0,        // sensor width in mm (35 = full-frame)
    "focalLength": 70.0         // focal length in mm
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
    "roughness": 0.3,           // GGX roughness: 0 = mirror, 1 = fully diffuse
    "metallic":  0.0            // 0 = dielectric, 1 = metal
  }
}
```

## Dependencies

Fetched automatically by CMake:

| Library | Version | Purpose |
|---------|---------|---------|
| [GLFW](https://www.glfw.org) | 3.4 | Window + OpenGL context + input |
| [GLM](https://github.com/g-truc/glm) | 1.0.1 | Math (vectors, matrices) |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.91.9 | HUD overlay |
| [cgltf](https://github.com/jkuhlmann/cgltf) | 1.14 | glTF 2.0 parsing (header-only) |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | JSON config (header-only) |
| [Catch2](https://github.com/catchorg/Catch2) | 3.7.1 | Test framework (tests only) |

OpenGL, Cocoa, stb_image, and stb_image_write are provided by the macOS system frameworks and committed headers respectively.

## Architecture

```
src/
├── main.cpp                — render loop, G-buffer, SSAO pipeline, config wiring
├── core/
│   ├── config.hpp/.cpp     — JSON profile/scene loader/saver (nlohmann/json)
│   └── log.hpp             — structured logger (DEBUG/INFO/WARN/ERROR → stderr)
├── render/
│   ├── window.hpp/.cpp     — GLFW window + OpenGL 3.3 Core context
│   ├── shader.hpp/.cpp     — GLSL compile/link, uniform cache
│   ├── mesh.hpp/.cpp       — VAO/VBO/EBO geometry, bounding sphere + AABB
│   ├── texture.hpp/.cpp    — PNG/JPG/HDR loading via stb_image (RGBA8 + RGB16F)
│   ├── model.hpp/.cpp      — glTF 2.0 loader (cgltf), Model/SubMesh, node transform walk
│   └── frustum.hpp         — Gribb-Hartmann frustum planes, sphere culling test
├── camera/
│   └── camera.hpp/.cpp     — filmback/focal-length camera, LMB orbit (depth-sampled pivot)
└── ui/
    ├── hud.hpp/.cpp        — Dear ImGui overlay (crosshair, AOV, histogram, stats)
    └── menu_osx.hpp/.mm    — Native macOS menu bar via Cocoa (ObjC++ bridge)
shaders/
├── geometry/
│   └── pbr.vert/.frag      — MVP transform, G-buffer MRT, PBR BSDF, 16 AOV modes
├── post/
│   ├── blit.vert/.frag     — fullscreen composite, SSAO multiply, channel overlay
│   ├── ssao.vert/.frag     — SSAO compute (64-sample kernel, depth reconstruction)
│   └── ssao_blur.frag      — 3×3 / 5×5 box blur on raw SSAO
├── sky/
│   └── sky.vert/.frag      — equirectangular HDRI skydome (rotation, exposure, flip)
└── debug/
    └── bounds.vert/.frag   — flat-colour GL_LINES pass for the AABB wireframe box
tests/
├── gl_context.hpp/.cpp     — headless GLFW singleton (hidden 1×1 window, GL 3.3 Core)
├── reporter.cpp            — Catch2 listener: ✓ PASSED (green) / ✗ FAILED (red)
├── test_camera.cpp         — projection math, viewMatrix, front direction, pitch clamp
├── test_config.cpp         — JSON load/save round-trip, defaults, malformed input
├── test_frustum.cpp        — plane extraction, sphere inside/outside/boundary
├── test_mesh.cpp           — bounding radius, AABB, counts, move semantics
├── test_shader.cpp         — compile success/failure, missing file
└── test_texture.cpp        — pixel correctness, bind unit, move semantics
profile.json                — renderer config (resolution, IBL samples, SSAO, IOR)
scene.json                  — scene content (camera, HDRI, geometry, material overrides)
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
| Directory Structure — domain-based layout: core/, render/, camera/, ui/ | ✓ |
| Tests — Catch2 v3 suite, 39 tests / 118 assertions, headless GL, coloured output, CTest integration | ✓ |
| HUD: Waveform, AOV min/max (Depth), 2D groundplane, overlay - cross, camera square, aspect safe zones, grid (3x3 with sub-lines which are darker)
| Color Management — OpenEXR I/O linear pipeline, OCIO ACES workflow w/ sRGB and Rec709 viewing LUTs | planned |
| Camera & Lens Effects — ISO, f-stop, shutter speed, DoF, focus distance, chromatic aberration, anamorphic lenses, aspect ratio, Kelvin-based lighting controls, film grain | planned |
| Shader update — RGB albedo color parameter (white default), indirect (self-reflection, refraction, SSS) | planned |
| Ray Tracing — shadows, area lights, indirect illumination, brute-force path tracing | planned |
| Sampling — adaptive sampling, multiple importance sampling (MIS) | planned |
| Displacement: Height map, Displacement bounds, SubDivision at Render Time
| Geometry & Shader Library — reusable assets, camera presets, and materials files and presets, scene import/export | planned |
| Test Scenes — teapot, cornell box, three-sphere material test with curved backdrop | planned |
| Future Features — 2d groundplane, alembic (cam and geo), turntable, macbeth ColorChecker, diffusion rendering, cross-platform support (NVIDIA and Apple Silicon) | planned |