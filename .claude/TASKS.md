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
| Tests — Catch2 v3 suite, 91 tests / 571 assertions, PBR math, all 16 AOVs, SSAO, CPU math, headless GL | ✓ |
| Performance profiling and optimisation — benchmark infrastructure, GPU query rings, async PBO readback, separable SSAO blur, IBL precomputation, SSAO kernel UBO, uniform location pre-cache: **3.2× FPS** (66→213) | ✓ |
| Physical Camera — ISO/f-stop/shutter exposure triangle, screen-space DoF (thin-lens CoC, Poisson-disc), cinematic letterbox, HDRI EV offset + intensity, per-AOV HDRI response | ✓ | 
| Color Management — OpenEXR I/O linear pipeline, OCIO ACES workflow w/ OCIO Display Transform (sRGB / Rec.709), White Balance & Kelvin-based lighting controls, | planned |
| Rendering — Thin lens model, Physical-based spectral GPU path tracing render engine, Monte Carlo Unbiased Global Illumination (Indirect/Multiple Scattering), Correct exposure, Linear workflow, Adaptive Sampling / Multiple importance sampling (MIS), Recursive tracing, Russian roulette termination, Energy conservation, Spectral light transport, Wavelength sampling, Spectral materials, Spectral dispersion, BSDF / PBR materials, BRDF sampling + Importance Sampling, Next-event estimation, HDR IBL, Tone-mapping, Area lights, Shadows / Soft shadows, BVH acceleration, Fresnel effects, Caustics, Path throughput accumulation, Radiance estimation formulation | planned |
| Post Lens Effects - DoF, chromatic aberration, anamorphic lenses, film grain
| Displacement: Height map, Disp. bounds, OpenSubD | planned |
| Asset Management — geometry, materials (files and presets), hdr, camera presets | planned |
| Test Scenes — teapot, cornell box, three-sphere material test with curved backdrop | planned |
| HUD: Waveform, AOV min/max (Depth), 2D groundplane, overlay - aspect ratio, Rule-of-Thirds grid | planned |
| Future Features — alembic (cam and geo), background, turntable, macbeth ColorChecker, diffusion rendering, cross-platform support (NVIDIA and Apple Silicon) | planned |

--CURRENT TASK:
Here is Claude's plan:
╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌
 Color Management Plan

 Context

 The renderer outputs physically-accurate scene-linear radiance but has no display transform, no tone-mapping,
 and broken HDRI loading: the profile.json HDRI is already a .exr file but texture.cpp only checks for .hdr — EXR
 falls through to the LDR path and loads garbage. This feature fixes that and builds the full color pipeline:

 1. OpenEXR I/O — load EXR HDRI + textures; save multi-layer AOV EXR
 2. OCIO ACES display transform — GPU 3D LUT, ViewLUT selector (RAW / sRGB / Rec.709)
 3. White balance — Kelvin slider with chromatic adaptation
 4. File cleanup — only .png and .exr accepted throughout

 ---
 New Dependencies (FetchContent — ASF official repos, no vendoring)

 ┌────────────────────┬───────────────────────────────────────┬──────────────────────────────────┐
 │      Library       │                Source                 │             Purpose              │
 ├────────────────────┼───────────────────────────────────────┼──────────────────────────────────┤
 │ OpenEXR v3.3.x     │ AcademySoftwareFoundation/openexr     │ EXR load + write; pulls in Imath │
 ├────────────────────┼───────────────────────────────────────┼──────────────────────────────────┤
 │ OpenColorIO v2.4.x │ AcademySoftwareFoundation/OpenColorIO │ ACES built-in config, LUT baking │
 └────────────────────┴───────────────────────────────────────┴──────────────────────────────────┘

 Both are fetched at CMake configure time. First build is slow (~15–30 min); subsequent builds use the cache.

 ---
 Pre-work: Benchmark Reset

 Before any code changes:
 1. Delete all files in benchmarks/ (9 old step-snapshots from the performance task)
 2. Run the renderer, capture a fresh baseline JSON snapshot (same format as the existing benchmarks) — this is
 the new benchmarks/baseline.json
 3. Commit as bench: reset benchmarks — pre-color-management baseline

 ---
 Phase 1 — CMake: Add OpenEXR + OCIO via FetchContent

 File: CMakeLists.txt

 # OpenEXR (Imath is a transitive dep, auto-pulled)
 FetchContent_Declare(openexr
     GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openexr.git
     GIT_TAG        v3.3.3
     GIT_SHALLOW    TRUE
 )
 set(OPENEXR_BUILD_TOOLS    OFF CACHE BOOL "" FORCE)
 set(OPENEXR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
 set(OPENEXR_INSTALL        OFF CACHE BOOL "" FORCE)
 FetchContent_MakeAvailable(openexr)

 # OpenColorIO (auto-downloads yaml-cpp, expat, pystring via MISSING policy)
 FetchContent_Declare(OpenColorIO
     GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/OpenColorIO.git
     GIT_TAG        v2.4.0
     GIT_SHALLOW    TRUE
 )
 set(OCIO_BUILD_APPS              OFF CACHE BOOL   "" FORCE)
 set(OCIO_BUILD_TESTS             OFF CACHE BOOL   "" FORCE)
 set(OCIO_BUILD_PYTHON            OFF CACHE BOOL   "" FORCE)
 set(OCIO_INSTALL_EXT_PACKAGES  MISSING CACHE STRING "" FORCE)
 FetchContent_MakeAvailable(OpenColorIO)

 Link both to kodak_core:
 target_link_libraries(kodak_core PUBLIC ... OpenEXR::OpenEXR OpenColorIO::OpenColorIO)

 Remove src/render/stb_write_impl.cpp from the KODAK executable (PNG screenshot is gone).

 ---
 Phase 2 — File Type Cleanup + EXR Loading

 Goal: .exr HDRI and textures load correctly; unsupported formats rejected.

 2.1 src/render/texture.cpp

 Replace the .hdr branch with an .exr branch using OpenEXR's InputFile API:

 bool isExr = ends_with(path, ".exr");
 bool isPng = ends_with(path, ".png");
 if (!isExr && !isPng)
     throw std::runtime_error("Unsupported image format (only .exr and .png): " + path);

 For .exr (HDRI and float textures):
 // OpenEXR load path — always GL_RGB16F
 Imf::InputFile file(path.c_str());
 Imath::Box2i dw = file.header().dataWindow();
 int w = dw.max.x - dw.min.x + 1;
 int h = dw.max.y - dw.min.y + 1;
 std::vector<float> buf(w * h * 3);
 // Read R,G,B channels via FrameBuffer + readPixels
 // Upload as GL_RGB16F, GL_CLAMP_TO_EDGE, GL_LINEAR (no mip for HDR equirect)

 For .png (LDR textures): existing stbi_load path, unchanged except the extension check is now explicit.

 2.2 Flip convention for EXR HDRI

 EXR (like .hdr) uses top-left origin → no Y flip needed for equirectangular sampling. Add a comment to match the
 existing .hdr note.

 2.3 Tests

 tests/test_texture.cpp — update to use .exr fixtures or mock; confirm .jpg throws.

 ---
 Phase 3 — EXR Multi-Layer Save

 Goal: "Save EXR" captures a multi-layer EXR with AOV channels.

 3.1 src/render/exr_io.hpp/cpp (new)

 struct AovBuffer {
     std::string name;   // e.g. "beauty", "albedo", "normals", "depth", "direct_diff", "direct_spec", "ao"
     std::vector<float> rgb; // w*h*3 floats, top-left origin
 };

 // Writes all AOVs as named layers in a single multi-part EXR.
 // Channels: "beauty.R", "beauty.G", "beauty.B", "albedo.R", etc.
 bool write_exr_multilayer(const std::string& path, int w, int h,
                           const std::vector<AovBuffer>& aovs);

 Uses Imf::OutputFile with channel naming "layerName.R/G/B".

 3.2 AOV capture in src/main.cpp

 On doCaptureEXR:
 1. Read beauty from rt.colorTex directly via glGetTexImage — no re-render needed
 2. Read normals from rt.normalTex directly
 3. Read linearised depth from rt.depthTex
 4. For each additional AOV (albedo, direct_diff, direct_spec, ao): save current viewMode, set viewMode, draw
 scene into a temporary FBO, read pixels, restore viewMode
 5. Apply finalExposure to beauty buffer only (in CPU loop, not the others)
 6. Call write_exr_multilayer()

 AOV set: {"beauty", "albedo", "normals", "depth", "direct_diff", "direct_spec", "ao"} — maps to viewModes 1, 2,
 3 (normals from G-buffer), depth (from G-buffer), 9, 10, 16.

 3.3 UI changes

 - Remove doCapture (PNG) from FrameStats, remove the screenshot button from hud.cpp
 - Remove screenshot from menu_osx.mm (if present)
 - Add doCaptureEXR flag + "Save EXR" button (or keyboard hotkey, e.g. S)
 - Remove stb_image_write include from stb_write_impl.cpp (entire file can be removed if unused)

 3.4 Tests (tests/test_exr_io.cpp)

 - Write a known 4×4 pattern with two AOVs → open with Imf::InputFile, verify channel names and pixel values
 (epsilon 1e-3 for half-float precision)

 ---
 Phase 4 — OCIO Display Transform (ViewLUT)

 Goal: GPU 3D LUT baked from OCIO ACES CG built-in config; RAW / sRGB / Rec.709 selector in HUD.

 4.1 src/render/color_pipeline.hpp/cpp (new)

 enum class ViewLUT { Raw, ACES_sRGB, ACES_Rec709 };

 class ColorPipeline {
 public:
     ColorPipeline();   // loads OCIO built-in CG config
     void bake(ViewLUT mode, int size = 33);  // bakes LUT, uploads to GPU
     GLuint lut_tex()  const;  // GL_TEXTURE_3D, GL_RGB16F
     bool   enabled()  const;  // false when mode == Raw
 private:
     OCIO::ConstConfigRcPtr m_config;
     GLuint m_tex = 0;
     bool   m_enabled = false;
 };

 Built-in config: OCIO::Config::CreateFromBuiltinConfig("cg-config-v1.0.0_aces-v1.3_ocio-v2.1")

 Color space names in the CG config (verify at runtime with config->getNumColorSpaces()):
 - Input: "ACEScg"
 - sRGB output: "Output - sRGB" (or "sRGB - Display - ACES")
 - Rec.709 output: "Output - Rec.709" (or "Rec.709 - Display - ACES")

 LUT bake (log2 shaper for HDR headroom):
 // For each LUT index (r, g, b) in [0, size-1]:
 float s_r = r / (size - 1.0f);  // normalized [0,1]
 // Invert log2 shaper: scene-linear = 2^(LOG2_MIN + s * (LOG2_MAX - LOG2_MIN))
 float L2_MIN = -10.0f, L2_MAX = 10.0f;
 float lr = std::pow(2.0f, L2_MIN + s_r * (L2_MAX - L2_MIN));
 // same for g, b
 float pixel[3] = {lr, lg, lb};
 cpuProc->applyRGB(pixel);  // ACES RRT + ODT
 // store in lut[]
 Upload as GL_RGB16F 3D texture, trilinear filter.

 4.2 shaders/post/blit.frag

 Add after exposure (before channel view / invert / letterbox):

 uniform sampler3D uColorLUT;
 uniform bool      uLutEnabled;
 uniform vec3      uWhiteBalance;  // (from Phase 5)

 // White balance
 color *= uWhiteBalance;

 // Display LUT
 if (uLutEnabled) {
     const float L2_MIN = -10.0, L2_MAX = 10.0;
     vec3 s = (log2(max(color, 1e-10)) - L2_MIN) / (L2_MAX - L2_MIN);
     color = texture(uColorLUT, clamp(s, 0.0, 1.0)).rgb;
 }

 4.3 Config (src/core/config.hpp/cpp)

 struct Color {
     int   viewLut         = 1;       // 0=Raw 1=ACES_sRGB 2=ACES_Rec709
     float whiteBalanceK   = 6504.f;  // D65 default
 } color;
 JSON key "color".

 4.4 src/main.cpp

 - ColorPipeline pipeline; constructed after GL context ready
 - pipeline.bake(cfg.color.viewLut) at startup
 - Pre-cache: blitLocLutEnabled, blitLocColorLUT (bind to texture unit 3)
 - Per-frame: bind LUT texture, set uniforms
 - When stats.viewLutChanged: pipeline.bake(newLut) then clear flag

 4.5 UI (src/ui/hud.cpp)

 New Color panel section (after Exposure):
 - "ViewLUT" combo: Raw | ACES sRGB | ACES Rec.709
 - Sets stats.viewLutChanged when changed

 ---
 Phase 5 — White Balance (Kelvin)

 5.1 src/render/color_math.hpp (inline header)

 // CIE xy chromaticity from correlated color temperature K (1000–15000K)
 // Uses Kim et al. 2-range polynomial
 glm::vec2 kelvin_to_xy(float K);

 // Bradford chromatic adaptation: scene white at K → D65 → linear sRGB RGB gains
 // Normalized: D65 (6504K) returns (1, 1, 1)
 glm::vec3 white_balance_gains(float K);

 5.2 src/main.cpp

 glm::vec3 wbGains = white_balance_gains(cfg.color.whiteBalanceK);
 blitShader.setAt(blitLocWhiteBalance, wbGains);  // pre-cached loc
 Recompute only when K changes (stats.wbChanged flag).

 5.3 UI (src/ui/hud.cpp)

 In Color section:
 - Kelvin slider 1000–12000K
 - Preset row: 2856K Tungsten | 4000K Fluorescent | 5600K Daylight | 6504K D65 | 7500K Shade

 5.4 Tests (tests/test_color_math.cpp)

 - D65 (6504K) → gains within 1e-4 of (1, 1, 1)
 - 3200K → R > 1, B < 1
 - 10000K → R < 1, B > 1

 ---
 Implementation Order

 1. Pre-work: reset benchmarks (delete + new baseline commit)
 2. Phase 1: CMake (build system first, verify compile)
 3. Phase 2: EXR loading + file type cleanup (fixes the broken HDRI immediately)
 4. Phase 3: EXR multi-layer save + remove screenshot
 5. Phase 4: OCIO LUT + ViewLUT UI
 6. Phase 5: White balance
 7. Tests: run ctest --preset default --output-on-failure
 8. clang-tidy -p build on all new .cpp files
 9. Commit + open PR

 ---
 Files Modified

 ┌───────────────────────────────────┬────────────────────────────────────────────────────────────────────────┐
 │               File                │                                 Change                                 │
 ├───────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
 │ CMakeLists.txt                    │ FetchContent for OpenEXR + OCIO; remove stb_write_impl from KODAK      │
 ├───────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
 │ src/render/texture.cpp            │ Replace .hdr path with .exr (OpenEXR); enforce .png/.exr only          │
 ├───────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
 │ src/render/exr_io.hpp/cpp         │ new — multi-layer AOV EXR write                                        │
 ├───────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
 │ src/render/color_pipeline.hpp/cpp │ new — OCIO LUT baking                                                  │
 ├───────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
 │ src/render/color_math.hpp         │ new — Kelvin → white balance gains                                     │
 ├───────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
 │ src/core/config.hpp/cpp           │ Add Color struct (viewLut, whiteBalanceK)                              │
 ├───────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
 │ src/ui/hud.hpp                    │ Replace doCapture with doCaptureEXR; add viewLut/wbK fields            │
 ├───────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
 │ src/ui/hud.cpp                    │ Remove PNG screenshot button; add Color section (ViewLUT + Kelvin)     │
 ├───────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
 │ src/main.cpp                      │ Pipeline init, LUT bind, WB uniform, EXR capture (multi-AOV), remove   │
 │                                   │ PNG save                                                               │
 ├───────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
 │ src/ui/menu_osx.mm                │ Remove screenshot menu item                                            │
 ├───────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
 │ shaders/post/blit.frag            │ Add uColorLUT, uLutEnabled, uWhiteBalance                              │
 ├───────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
 │ benchmarks/                       │ Delete all 9 old files, add new baseline.json                          │
 ├───────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
 │ tests/test_exr_io.cpp             │ new                                                                    │
 ├───────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
 │ tests/test_color_math.cpp         │ new                                                                    │
 ├───────────────────────────────────┼────────────────────────────────────────────────────────────────────────┤
 │ tests/test_texture.cpp            │ Update for EXR fixtures / extension enforcement                        │
 └───────────────────────────────────┴────────────────────────────────────────────────────────────────────────┘

 ---
 Verification

 make                                          # must compile clean
 ctest --preset default --output-on-failure    # all tests pass

 # Manual:
 # 1. Load scene: HDRI (EXR) loads without error
 # 2. ViewLUT combo: Raw=flat linear, sRGB=tonemapped+gamut, Rec709=same with Rec.709 primaries
 # 3. Kelvin to 3200K: warm shift; back to 6504K: neutral
 # 4. Save EXR: multi-layer .exr on Desktop, open in Nuke/DaVinci/Photoshop; verify layer names
 # 5. Confirm PNG screenshot button is gone from HUD and macOS menu
 # 6. Pass a .jpg path in profile.json: renderer throws clear error

 clang-tidy -p build src/render/exr_io.cpp src/render/color_pipeline.cpp src/render/texture.cpp

 ---
 PR

 After all tests pass, open PR:
 - Branch: feat/color-management
 - Title: feat: color management — OpenEXR I/O, OCIO ACES display transform, white balance
 - Update TASKS.md: mark Color Management ✓, expand Current Task section with all steps