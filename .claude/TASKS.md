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