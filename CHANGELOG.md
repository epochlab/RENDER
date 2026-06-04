# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [Optimisation Sprint] — 2026-06-04

7-step render-loop optimisation delivering a **3.2× FPS improvement** (66 → 213 FPS mean) on the benchmark scene (`--benchmark 300`, 2048×1152, ssaoSamples=8). Results in `benchmarks/`.

- **Step 0 — Benchmark infrastructure** — `--benchmark N` CLI flag; per-frame CPU time + 4-pass GPU timing (geometry, SSAO, blur H, blur V, composite) accumulated to JSON; `benchmarks/` directory with per-step regression snapshots
- **Step 1 — GPU query ring fix + per-pass timing** — fixed ring-buffer indexing bug (both queries landing in different slots per frame); split into 5 independent `GL_TIME_ELAPSED` rings; accurate per-pass breakdown revealed SSAO as the bottleneck; reducing `ssaoSamples` 64→16 gave **+49% FPS** (66→99)
- **Step 2 — HDRI rotation matrix cache** — `hdriDirty` flag prevents redundant `rotateXYZ` recomputation when HDRI angles are unchanged each frame
- **Step 3 — Async histogram readback via PBO** — double-buffered PBOs replace synchronous `glReadPixels`; eliminated ~1.8 ms/frame GPU pipeline stall; **+22% FPS** (99→121)
- **Step 4 — Separable SSAO blur** — 2D box kernel replaced with separable H×V 1D passes (`ssao_blur_v.frag` added); **−95% blur GPU time** (0.266→0.013 ms mean); mathematically exact (box filter is separable)
- **Step 5 — IBL precomputation** — irradiance map (128×64 GL_RGB16F), prefiltered specular map (512×256 GL_RGB16F, 5 mips), and BRDF LUT (512×512 GL_RG16F) baked once at startup via `IblBaker`; per-pixel Fibonacci sampling loops in `pbr.frag` eliminated; HDRI yaw rotation kept as per-frame `uHdriRotMat` uniform so the slider stays live; **+123% FPS** (112→249), **−90% GPU geometry time** (5.1→0.5 ms mean)
- **Step 6 — SSAO kernel UBO** — 64-call `glUniform3fv` startup loop replaced with a single `glBufferData` upload into a `std140` UBO; `Shader::bindUniformBlock()` added; `ssao.frag` updated to `layout(std140) uniform KernelBlock`
- **Step 7 — Uniform location pre-cache** — `Shader::uniformLoc()` and `setAt(GLint, T)` overload family (mat4, mat3, vec3, vec2, float, int, bool) added; 23 per-frame `set("name", value)` calls across PBR, sky, SSAO, blit, and line shaders replaced with direct-location `setAt()` calls, eliminating the `unordered_map` string-hash on the hot path

---

## [Test Suite] — 2026-06-03

- **91 tests, 571 assertions** — full regression suite covering config I/O, camera math, frustum culling, mesh bounds, shader compilation, texture correctness, PBR BSDF math, all 16 AOV modes, SSAO math, and CPU-side rendering math
- **PBR BSDF verification** — C++ replicas of Schlick Fresnel, Smith G masking, IOR→F0 and metallic blending, and energy conservation (Ld + Ls = 1.0 with white IBL and white albedo)
- **All 16 AOV modes pixel-verified** — `RenderHarness` renders each view mode to a headless 1×1 FBO with analytically controlled inputs (white HDRI, white albedo, flat normal, NdotV=1) and reads back the pixel via `glReadPixels`; covers beauty, alpha, bounds, wireframe, depth, albedo, HSV, luminance, direct_diffuse, direct_refl, world_pos, world_normals, UV, shading_normal, fresnel, and occlusion
- **SSAO math tests** — depth reconstruction round-trip (`viewPosFromDepth`), smoothstep range-check boundary cases, and kernel property assertions (upper hemisphere, unit sphere, determinism, seed independence)
- **CPU math tests** — EMA FPS smoothing, HDRI Z-Y-X Euler rotation matrix (identity, Rx/Ry correctness, orthonormality), histogram triangle-kernel convolution, sqrt normalisation, grayscale and near-binary detection, frame-time min/max
- **`generateSSAOKernel` extracted** — SSAO kernel generation moved from `main.cpp` into `src/core/ssao_kernel.hpp` (header-only, seed-based, bit-reproducible); noise texture seeded separately with seed 43
- **`kodak_core` static library** — all modules except `main.cpp`, ImGui, and the ObjC++ menu compiled into a shared static lib; both `KODAK` and `tests_kodak` link against it
- **Coloured per-test output** — custom Catch2 `EventListenerBase` prints `✓ PASSED` (green) or `✗ FAILED` (red) per test case; full CTest integration via `catch_discover_tests`
- **`focalLength` default corrected** — `AppConfig::Camera.focalLength` fixed from 50 mm to 70 mm to match the `Camera` constructor and scene.json JSON default

---

## [Config Split] — 2026-06-03

- **`profile.json` split into `profile.json` + `scene.json`** — renderer settings (resolution, IBL sample count, IOR, SSAO) live in `profile.json`; scene content (camera, geometry, HDRI, roughness, metallic) live in `scene.json`; both files load independently with the same missing-file / parse-error fallback to defaults
- **Set JSON writes only runtime state** — View → Set JSON now does a targeted read-modify-write of `scene.json`, persisting only camera position, focal length, and HDRI rotation; all other fields and `profile.json` are left untouched
- **`Model::vertexCount()` renamed to `indexCount()`** — the method returns the sum of index-buffer sizes, not vertex counts; HUD Scene label updated from "Vertices" to "Points"; internal `FrameStats` fields renamed to match

---

## [AOV Reorder] — 2026-06-03

- **AOV dropdown reordered** — modes regrouped by category: beauty → alpha → bounds → wireframe → depth → albedo → hsv → luminance → direct_diffuse → direct_refl → world_pos → world_normals → uv → shading_normal → fresnel → occlusion
- **"ao" renamed to "occlusion"** — consistent lowercase underscore naming across all AOV labels

---

## [Menu — HUD Toggle Fix] — 2026-06-03

- **"Sky Background" menu item removed** — sky visibility is controllable via the HUD HDRI section; the redundant menu toggle is gone
- **"Show Panel" renamed to "Show/Hide HUD" and fixed** — the menu item now correctly toggles the HUD; previously the toggle was overwritten each frame before being read back, so clicks had no effect

---

## [Histogram Triangle Kernel] — 2026-06-03

- **Histogram smooth — box filter replaced with triangle kernel** — `smoothChannel` previously used a uniform (box) weight per bin, which produced flat-topped rectangular shapes for narrow-spike AOVs (wireframe MSAA scatter, bounds grey fill). Replaced with an inverse-distance triangle weight (`radius + 1 − |k − b|`), so a 1–2 bin spike now maps to a symmetric peaked hump rather than a plateau. Continuous distributions (beauty, UV, depth) are visually unchanged since adjacent bins are all occupied and the weighted average converges to the same result.

---

## [SSAO Full-Res] — 2026-06-03

- **SSAO restored to full resolution** — render targets (`ssaoRt`, `blurRt`) and the SSAO/blur viewport now run at `BASE_W × BASE_H` by default, eliminating the half-res downsample and its implicit bilinear upsample artefacts
- **`ssaoHalfRes` profile flag** — set `"ssaoHalfRes": true` in `profile.json` to opt back into half-resolution SSAO (previous behaviour); `false` (default) gives full-res

---

## [Channel Mode Overlay] — 2026-06-03

- **R/G/B viewport indicator** — active channel isolation is shown as a coloured text label (R in red, G in green, B in blue) in the top-right corner of the viewport; nothing rendered when in full RGB mode; always visible regardless of HUD panel state

---

## [Histogram Fixes + FPS Graph] — 2026-06-02

- **Histogram — internal diagonal artefacts fixed** — `AddConcavePolyFilled` applied AA fringe to every edge of each ear-clip sub-triangle, including internal shared edges; doubled fringes appeared as bright diagonal lines through every channel fill. Fixed by disabling `ImDrawListFlags_AntiAliasedFill` for the fill call; `AddPolyline` retains its own AA for the smooth top curve.
- **Histogram — endpoint spike edge artefacts fixed** — bins 0 and 255 were included in the smooth window and clamped to `sqrt(1)=1.0`, inflating adjacent bins with a smaller `cnt` denominator. This produced steep near-vertical transitions visible in all passes (luminance, UV, wireframe). Fixed by excluding endpoints from the window (`k < 1 || k > 254`).
- **Histogram — 2-channel AOV support** — UV and Fresnel (RG, B constant zero) now detect and skip the inactive B channel; overlap is computed as `min(R,G)` so the white zone correctly marks where both channels coincide.
- **Histogram — inactive channel flat-line artefact fixed** — 2-channel AOVs no longer draw a flat B outline along the histogram's bottom border.
- **Histogram — smoothing radius unified** — greyscale non-binary path uses `radius=4` (9-bin) matching the colour path for consistent appearance across all AOV types.
- **FPS graph — average FPS overlay** — a red horizontal line marks the smoothed average FPS on the frame-timing graph alongside the white instantaneous trace.

---

## [AOVs + Histogram] — 2026-06-02

- **AOV reorder** — modes resequenced to group perceptual channels first: beauty → alpha → luminance → hsv → bounds → wireframe → depth → world_pos → world_normals → uv → albedo → direct_diffuse → direct_refl → shading_normal → ao → fresnel (16 total); all mode integer constants updated consistently across `basic.frag`, `blit.frag`, and `main.cpp`
- **HSV AOV (mode 4)** — new display mode converts RGB → HSV in `blit.frag` using a branchless GLSL formulation; H mapped to red, S to green, V to blue
- **Lightroom-style histogram** — RGB histogram widget added to the HUD below the AOV section; smooth filled curves (B→G→R back-to-front) drawn via ImGui path API on a dark background with sqrt-scale normalisation; grayscale AOVs (alpha, luminance, depth) render as a single 80%-white curve; readback throttled to every 4 frames via a fixed 256×144 `GL_RGB8` FBO blit; fixed peak-normalisation bug that caused full-height red artefact bars for binary-valued (0/255) images

---

## [Hotkeys] — 2026-06-02

- **Channel overlay hotkeys** — R/G/B toggle red, green, blue channel views as a post-composite overlay in `blit.frag` via a new `uChannelView` uniform; press the same key again to clear; works on top of any AOV
- **Luminance hotkey (Y)** — toggles new AOV mode 15 (Rec. 709 luminance computed in `blit.frag`); press Y again to restore the previous AOV; also selectable from the AOV dropdown as "luminance"
- **Invert hotkey (I)** — toggles `1.0 − color` in `blit.frag` after channel overlay; stacks with R/G/B and Y
- **HUD toggle (H)** — edge-triggered `H` key hides/shows the stats panel
- **Camera speed** — `moveSpeed` reduced from 2.5 to 1.25 units/sec (0.5×); no modifier key
- **Focal length slider** — new Lens section in the HUD; 8–200 mm range, default 70 mm; live-updates the projection matrix; persisted by Save JSON
- **Enable Background checkbox** — sky toggle added to the HDRI section of the HUD; fixed sync ordering bug that caused HUD changes to be reverted on the next frame; native menu still works bidirectionally
- **GPU section** — moved to the top of the HUD panel
- **HUD labels** — "Flip V" → "Flip Y-axis"; sky checkbox → "Enable Background"

---

## [Logging & Diagnostics + Performance Profiling] — 2026-06-01

- **Structured logger** — new `log.hpp` header-only logger with four levels (DEBUG, INFO, WARN, ERROR); output format `[HH:MM:SS] [LEVEL] message` to stderr; DEBUG suppressed in release builds
- **Startup diagnostics** — on launch logs GL renderer/version, render resolution with downsample factor, loaded geometry (path, triangle and vertex counts), and HDRI path
- **Replaced ad-hoc output** — all `std::cout`/`std::cerr` calls in `main.cpp` and `config.cpp` replaced with typed log calls; screenshot path and profile-save confirmation now emit `[INFO]` lines; parse and write errors emit `[WARN]`/`[ERROR]`
- **Per-pass GPU timing** — second triple-buffered `GL_TIME_ELAPSED` query ring brackets the SSAO + blit passes; HUD Frame panel now shows `geom` and `post` GPU times separately instead of a single combined value
- **Throughput metrics** — `triPerSec` (Mtri/s) and `mpixPerSec` (Mpix/s) computed each frame from triangle count × smooth FPS and render resolution × smooth FPS; displayed in the HUD Frame panel below the GPU timing row

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
