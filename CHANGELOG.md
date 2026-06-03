# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

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
