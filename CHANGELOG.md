# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [Unreleased] — Milestone 6: HDRI Equirectangular Skydome

- Equirectangular HDR sky rendered as fullscreen triangle before geometry (depth mask off)
- World-space direction reconstructed per-pixel via `inverse(proj * view)` uniform
- HDR texture loading in `Texture`: detects `.hdr` extension → `stbi_loadf` + `GL_RGB16F`, no flip, no mipmaps
- FBO colour buffer upgraded `GL_RGB8` → `GL_RGB16F` to preserve HDR range through the render pass
- Reinhard tonemapping added to `blit.frag` (replaces raw passthrough); M8 will extend this

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
