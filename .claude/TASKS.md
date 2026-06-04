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
| Physical Camera — ISO/f-stop/shutter exposure triangle, screen-screen DoF (thin-lens CoC, Poisson-disc), cinematic letterbox, HDRI EV offset + intensity, per-AOV HDRI response | ✓ |
| Color Management — OpenEXR I/O linear pipeline, OCIO ACES workflow w/ OCIO Display Transform (sRGB / Rec.709), White Balance & Kelvin-based lighting controls | in progress |
| Rendering — Thin lens model, Physical-based spectral GPU path tracing render engine, Monte Carlo Unbiased Global Illumination (Indirect/Multiple Scattering), Correct exposure, Linear workflow, Adaptive Sampling / Multiple importance sampling (MIS), Recursive tracing, Russian roulette termination, Energy conservation, Spectral light transport, Wavelength sampling, Spectral materials, Spectral dispersion, BSDF / PBR materials, BRDF sampling + Importance Sampling, Next-event estimation, HDR IBL, Tone-mapping, Area lights, Shadows / Soft shadows, BVH acceleration, Fresnel effects, Caustics, Path throughput accumulation, Radiance estimation formulation | planned |
| Post Lens Effects — DoF, chromatic aberration, anamorphic lenses, film grain | planned |
| Displacement: Height map, Disp. bounds, OpenSubD | planned |
| Asset Management — geometry, materials (files and presets), hdr, camera presets | planned |
| Test Scenes — teapot, cornell box, three-sphere material test with curved backdrop | planned |
| HUD: Waveform, AOV min/max (Depth), 2D groundplane, overlay - aspect ratio, Rule-of-Thirds grid | planned |
| Future Features — alembic (cam and geo), background, turntable, macbeth ColorChecker, diffusion rendering, cross-platform support (NVIDIA and Apple Silicon) | planned |

--CURRENT TASK: Color Management (in progress)

 Phase 0 — Benchmark reset ✓ (cea6abe)
 Phase 1 — CMake: OpenEXR v3.3.3 + OCIO v2.4.0 via FetchContent ✓ (417babf)
 Phase 2 — EXR texture loading via Imf::RgbaInputFile; Y-flip fix ✓ (afb8373, d6055d4)

 ---
 Phase 3 — EXR Multi-Layer Save (next)

 Goal: replace PNG screenshot with a multi-layer EXR capturing all AOV channels.

 3.1 src/render/exr_io.hpp + exr_io.cpp (new)

   struct AovBuffer { std::string name; std::vector<float> rgb; }; // w*h*3, top-left origin

   void write_exr_multilayer(const std::string& path, int w, int h,
                             const std::vector<AovBuffer>& aovs);

   Uses Imf::Header + Imf::OutputFile. Channel naming: "beauty.R/G/B", "albedo.R/G/B", etc.

 3.2 src/main.cpp — replace PNG capture block (lines 1045-1062)

   - Rename doCapture -> doCaptureEXR throughout
   - Read AOVs from existing FBO textures via glGetTexImage (beauty, normals from G-buffer)
   - Re-render remaining AOVs by toggling viewMode into a scratch FBO, then restore
   - AOV set: beauty, albedo, normals, depth, direct_diff, direct_spec, ao
   - Apply finalExposure to beauty only before write
   - Remove #include <stb_image_write.h>

 3.3 src/render/stb_write_impl.cpp — delete; remove from CMakeLists.txt KODAK sources

 3.4 src/ui/hud.hpp, hud.cpp, menu_osx.hpp, menu_osx.mm

   - Replace doCapture -> doCaptureEXR
   - Update menu label "Screenshot" -> "Save EXR"

 3.5 tests/test_exr_io.cpp (new)

   - Write 4x4 two-AOV EXR, reopen with Imf::InputFile, verify channel names + pixel values (epsilon 1e-3)

 Verification:
   make && ctest --preset default --output-on-failure
   ./build/KODAK -> Save EXR -> open in Nuke/Resolve, verify layer names

 ---
 Phase 4 — OCIO Display Transform ✓ (fcd7b1f, 18b4f41)

   src/render/color_pipeline.hpp/cpp -- 33^3 LUT baked from OCIO CG config v1 (ACES v1.3)
   shaders/post/blit.frag -- uColorLUT (sampler3D), uLutEnabled; log2 shaper [-10, +10]
   src/core/config.hpp -- Color { viewLut } (0=Raw 1=ACES_sRGB 2=ACES_Rec709)
   src/ui/hud.cpp -- ViewLUT combo (Raw | ACES sRGB | ACES Rec.709)
   src/main.cpp -- EXR export extended to 7 AOVs: beauty, normals, depth, ao, albedo, direct_diffuse, direct_refl

   Fix: OCIO display/view names corrected to match cg-config-v1 ("sRGB - Display",
        "Rec.1886 Rec.709 - Display", "ACES 1.0 - SDR Video")

 ---
 Phase 5 — White Balance / Kelvin (planned)

   src/render/color_math.hpp -- kelvin_to_xy (Kim et al.), white_balance_gains (Bradford)
   shaders/post/blit.frag -- uWhiteBalance vec3 applied before LUT
   src/ui/hud.cpp -- Kelvin slider 1000-12000 K + presets (Tungsten/Fluorescent/Daylight/D65/Shade)
   tests/test_color_math.cpp -- D65->(1,1,1); 3200K->R>1,B<1; 10000K->R<1,B>1
