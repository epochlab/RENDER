# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [PBR BSDF] — 2026-05-31

- **Schlick Fresnel** (`schlickFresnel`) replaces unphysical `mix(specular, diffuse, roughness)` blend; F0 derived from IOR for dielectrics (`((IOR-1)/(IOR+1))²`), from albedo for metals
- **Energy-conserving BSDF**: `Ld = albedo×(1-F)×(1-metallic)×irradianceIBL`; `Ls = F×reflectionIBL`; beauty = `Ld+Ls`
- **New uniforms**: `uMetallic` (0=dielectric, 1=metal), `uIOR` (default 1.5 ≈ plastic/glass)
- **AOV updates**: mode 9 (`direct_diffuse`) and mode 10 (`direct_refl`) now show Fresnel-weighted lobes; mode 13 (`fresnel`) added — raw F term visualised as colour
- **Config**: `shading.metallic` and `shading.ior` in `profile.json`, `AppConfig`, and `config.cpp`

---

## [Orbit Camera] — 2026-05-31

- LMB depth-sampled pivot orbit; WASD/QE always active; viewport resolution and IBL sample count in profile.json; cosine-weighted Fibonacci diffuse IBL fix; HDRI rotation; B key sky toggle; J key camera save

---

## [Project Quality] — 2026-05-31

- Renamed BOUNCE; JSON config (profile.json) for all runtime parameters; render scale fix; Z-up correction; SSAO 64-sample + 5×5 blur; 10 view modes; H/K keys for HUD/screenshot

---

## [HDRI Skydome] — 2026-05-31

- Equirectangular HDR sky; HDRI diffuse irradiance; FBO upgraded to RGB16F; linear HDR passthrough

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
