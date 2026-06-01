# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [Build & Run] — 2026-06-01

- **Single build preset** — `CMakePresets.json` consolidated to one `default` preset (Release + LTO); binary at `build/KODAK` with no `dev/` or `release/` subdirectory
- **Makefile** — top-level `make` (configure + build), `make run` (build + launch), `make clean` (wipe `build/`)

---

## [Quick Fixes] — 2026-05-31

- **Sky Background menu fix** — View → Sky Background now correctly toggles the sky; menu-driven changes are detected before the per-frame sync overwrites them (`main.cpp`)
- **world_pos AOV fix** — replaced `fract(vFragPos)` with AABB-normalised position `(vFragPos − boundsMin) / extent`, giving a smooth continuous gradient rather than repeating modulo-1 banding
- **Bounds AOV** — new "bounds" mode (mode 3, inserted after wireframe): geometry drawn as flat grey with a yellow GL_LINES wireframe box showing the world-space AABB min/max; `Mesh` and `Model` track model-space min/max; corners transformed at startup via the (constant) scene matrix; `line.vert`/`line.frag` added for the box pass

---

## [Render Performance] — 2026-05-31

- **Release build preset** — `cmake --preset release` builds with `-O3` + LTO; executable at `build/release/KODAK`
- **Uniform location cache** — `Shader::loc()` now caches `glGetUniformLocation` results; driver round-trip eliminated after first frame
- **Normal matrix on CPU** — `transpose(inverse(uModel))` removed from vertex shader; computed once per frame as `mat3` and uploaded as `uNormalMatrix`
- **HDRI rotation as `mat3`** — `rotateXYZ()` removed from `basic.frag` and `sky.frag`; rotation pre-built on CPU as `uHdriRotMat`, eliminating 6 trig calls per IBL sample per pixel
- **`invProj` caching** — only recomputed when projection matrix changes; skipped on every static frame
- **`invVP` hoisted** — single `glm::inverse(proj * view)` computed once and reused for sky shader
- **Half-resolution SSAO** — SSAO and blur passes run at `BASE_W/2 × BASE_H/2`; blur output uses `GL_LINEAR` for smooth blit upsampling; quarters fragment invocations
- **Syscall throttling** — `task_info` memory query and `glfwGetWindowSize` moved to every 60 frames, eliminating the periodic frame-pacing spikes visible in the HUD frame-time graph

---

## [Rename & Cleanup] — 2026-05-31

- **Project renamed** to KODAK; executable, window title, and screenshot prefix updated
- **File menu removed** — Quit lives in the macOS app menu via `terminate:`; `doQuit` flag and handling path removed
- **Dead code removed** — `Mesh::gpuBytes()` / `m_gpuBytes` (computed but never read)
- **Geometry variable** renamed from `rock` → `geom` in main loop (asset-neutral)
- **HDRI slider** label updated to "Y-axis"

---

## [GUI & Debug Enhancements] — 2026-05-31

- **Native macOS menu bar** — Cocoa/ObjC++ bridge (`menu_osx.mm`); View → Sky Background, Capture, Set JSON, Show Panel (checkmarks stay in sync each frame)
- **Crosshair** — white `+` always drawn at screen centre via ImGui background draw list, marking the camera orbit pivot sample point
- **HDRI Y-rotation slider** — 1–360° live spinner in HUD; sky and IBL lighting rotate together
- **HDRI Flip V** — toggle to flip the equirectangular panorama vertically in both `sky.frag` and `basic.frag`; persisted in `profile.json` as `hdri.flipV`
- **Fresnel AOV** — remapped from raw RGB to a red (facing) → green (grazing) scalar ramp
- **HUD layout** — AOV and HDRI sections moved to bottom of panel; "View" section renamed "AOV"
- **Orbit fix** — `ImGui::WantCaptureMouse` guard prevents camera orbit firing while dragging UI sliders
- **profile.json** — field ordering fixed; `saveConfig` switched to `nlohmann::ordered_json`
- **Hotkeys** — H/K/J/B removed; all actions accessible from native menu or HUD; ESC retained

---

## [PBR BSDF] — 2026-05-31

- **Schlick Fresnel** with F0 from IOR (dielectrics) or albedo (metals); energy-conserving `Ld + Ls` beauty
- **New uniforms**: `uMetallic`, `uIOR`; AOV modes 9/10/13 updated; `profile.json` + config extended

---

## [Orbit Camera] — 2026-05-31

- LMB depth-sampled pivot orbit; WASD/QE always active; viewport resolution and IBL sample count in profile.json; cosine-weighted Fibonacci diffuse IBL fix; HDRI rotation; sky and camera save hotkeys

---

## [Project Quality] — 2026-05-31

- JSON config (profile.json); render scale; Z-up correction; SSAO 64-sample + 5×5 blur; 10 view modes

---

## [HDRI Skydome] — 2026-05-31

- Equirectangular HDR sky; HDRI diffuse irradiance; RGB16F FBO; linear HDR passthrough

---

## [Geometry Loading] — 2026-05-30

- glTF 2.0 via cgltf; Model class with node hierarchy; albedo from baseColorTexture

---

## [Optimisation] — 2026-05-30

- 3-deep GPU query ring; EMA-smoothed FPS; Gribb-Hartmann frustum culling; per-frame matrix caching

---

## [Debug HUD] — 2026-05-30

- Dear ImGui overlay; 6 view modes; FBO + fullscreen blit; cinematic filmback/focal-length camera

---

## [Texture Loading] — 2026-05-30

- stb_image Texture class; UV-sphere factory; UV coords on all geometry

---

## [Hello 3D World] — 2026-05-30

- Window, Shader, Camera, Mesh; spinning cube/sphere/plane; WASD fly-through; GLFW + GLM via FetchContent
