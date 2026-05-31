# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [M8 — Orbit Camera] — 2026-05-31

- **LMB orbit**: left-click depth-samples the G-buffer at screen centre to set the orbit pivot; drag tumbles the camera around that point; release frees the cursor
- **Pivot depth**: world-space position reconstructed from the depth buffer via `inverse(proj × view)`; falls back to 3 units ahead of camera when no geometry is hit
- **No-jump orbit**: yaw/pitch synced after every orbit move so WASD re-entry is seamless; `m_firstMouse` reset on click prevents first-frame delta spike
- **WASD / QE always active**: free-fly never disabled during orbit
- **Y-axis inverted** for natural tumble feel
- **Viewport resolution in profile**: `render.width` / `render.height` configurable without recompile (defaults 2048×1152)
- **IBL sample count in profile**: `render.iblSamples` configurable without recompile
- **Diffuse IBL fix**: 64-sample cosine-weighted Fibonacci hemisphere integration; `albedo × avg(L)` correctly computed
- **HDRI lighting rotation**: `uHdriRot` passed to `basic.frag`; sky and diffuse irradiance rotate together
- **Skydome Y-flip**: V-coordinate flipped in equirectangular UV mapping; workaround `rotation.z = 180` removed from `profile.json`
- **B key**: toggle sky background at runtime (beauty mode only)
- **J key**: write current camera `position`, `yaw`, `pitch` back to `profile.json`

---

## [M7 — Project Quality & Bug Fixes] — 2026-05-31

---

## [M7 — Project Quality & Bug Fixes] — 2026-05-31

- **Project renamed BOUNCE**; window title updated
- **Space-hold mouse look**: cursor is free by default; hold Space to capture and rotate camera
- **Mouse sensitivity halved** (0.1 → 0.05)
- **JSON config** (`profile.json`): runtime config for camera, render scale, HDRI path + rotation + exposure, geometry path, light exposure/intensity — no recompile needed
- **HDRI + geometry paths** in config: swap assets without touching source
- **Render scale fix**: FBO always renders at `BASE_W × BASE_H` (2048×1152); no longer inflated by Retina display scale
- **Z-up → Y-up correction**: model loader applies −90° X rotation to imported geometry
- **Model name**: `Model::name()` derived from filename; HUD now shows the actual asset name instead of hardcoded "rock"
- **10 view modes** (keys 1–9 + 0): Beauty | Wireframe | Alpha | Depth | Position | Normals | UV | Albedo | Direct Diffuse | AO
- HDRI sky background only visible in Beauty mode; all other modes have black background
- **Alpha channel** (mode 3): samples albedo texture `.a`; textures now loaded as RGBA8
- **Albedo channel** (mode 8): raw texture, no lighting
- **Direct Diffuse** (mode 9, renamed from Irradiance): HDRI irradiance without albedo
- **SSAO** (mode 9): G-buffer with view-space normals + depth texture; 64-sample hemisphere kernel (deterministic seed 42); 5×5 blur pass; applied as AO multiplier in mode 1
- **HDRI rotation** (`profile.json` → `hdri.rotation`): XYZ Euler rotation applied to sky direction each frame
- **HDRI exposure** (`profile.json` → `hdri.exposure`): per-frame sky brightness multiplier
- **HDRI visibility toggle** (`profile.json` → `hdri.visible`): skip sky draw entirely when false
- **Irradiance scale** (`light.exposure × light.intensity`): scales HDRI diffuse contribution on geometry
- **H key**: toggle HUD overlay on/off
- **K key**: save PNG screenshot to `screenshots/BOUNCE_<timestamp>.png`
- **nlohmann/json v3.11.3** added via FetchContent; `stb_image_write.h` added for screenshot output

---

## [M6 — HDRI Equirectangular Skydome] — 2026-05-31

- Equirectangular HDR sky rendered as fullscreen triangle before geometry (depth test + mask off)
- World-space direction reconstructed per-pixel via `inverse(proj * view)` uniform; sky is world-space fixed (rotating the camera reveals different parts of the panorama)
- HDR texture loading in `Texture`: detects `.hdr` extension → `stbi_loadf` + `GL_RGB16F`, no flip, no mipmaps
- FBO colour buffer upgraded `GL_RGB8` → `GL_RGB16F` to preserve HDR range through the render pass
- HDRI diffuse irradiance: geometry lit by sampling the equirectangular map along the surface normal (direct radiance sample; M8 PBR replaces with preconvolved irradiance map)
- Directional light removed from `basic.frag`
- View mode 7 — Irradiance: shows raw HDRI contribution, no albedo
- Linear HDR passthrough in `blit.frag` (OCIO hook reserved for a later milestone)

---

## [M5 — Geometry Loading] — 2026-05-30

- Load glTF 2.0 files via cgltf v1.14 (header-only, MIT): parse + buffer load + validate
- New `Model` class: walks scene node hierarchy, accumulates transforms, builds `Mesh` per primitive
- Loads albedo from `baseColorTexture`; normal + ORM paths stored for M7 PBR (not yet bound)
- Replaces toy cube + sphere with Megascans rock asset (~13 886 tris, 4K albedo texture)
- Background cleared to black; ground plane removed
- `assets/geo/` added to `.gitignore` (large binary assets, ~47 MB)

---

## [M4 — Optimisation: FPS · Memory · Speed] — 2026-05-30

- FPS: raw headline + EMA smooth "avg" secondary line; frame-time min/max over the 128-frame window
- GPU timer: 3-deep query ring read via `GL_QUERY_RESULT_AVAILABLE` — no CPU stall on the GPU
- Memory: HUD splits RAM (`phys_footprint`) from tracked GPU allocation (mesh buffers + FBO)
- Speed: view/projection matrices cached per frame (eliminates double-computation), frame-constant uniforms set once
- Frustum culling (`src/frustum.hpp`, Gribb-Hartmann): per-object bounding-sphere test; HUD shows `drawn / total (culled)`
- `Mesh` gains bounding radius + GPU byte footprint; optional `FRAME_CAP` testing aid

---

## [M3 — Debug HUD + View Modes] — 2026-05-30

- Dear ImGui v1.91.9 overlay: FPS graph, GPU time, process memory, draw calls, per-object triangle counts, camera state
- 6 render view modes (keys 1–6): Diffuse, Wireframe, Depth, Position, Normals, UV
- Cinematic camera: 35mm filmback / 50mm focal length replacing raw FOV; E=up Q=down
- FBO render pipeline + fullscreen blit shaders (foundation for M6 post-processing)
- Window = BASE/SCALE logical pixels (1024×576); physical framebuffer = render resolution (1:1)
- Retina fix: `glfwGetFramebufferSize` called at startup; resize no longer jumps camera
- Plane UVs span 0→1 across full grid; sphere reduced to 16×16 (512 tris)

---

## [M2 — Texture Loading] — 2026-05-30

- `Texture` class via stb_image; `Texture::white()` for untextured objects
- `Mesh::sphere(stacks, slices)` UV-sphere factory
- UV coords on all geometry (cube, plane, sphere)
- `assets/textures/checker.png` 64×64 checkerboard test texture

---

## [M1 — Hello 3D World] — 2026-05-30

- `Window`, `Shader`, `Camera`, `Mesh` classes; OpenGL 3.3 Core on macOS
- Spinning cube + sphere + ground plane with directional diffuse shading
- WASD + mouse fly-through; FetchContent for GLFW 3.4 + GLM 1.0.1
