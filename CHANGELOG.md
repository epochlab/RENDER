# Changelog

All notable changes to this project will be documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [Unreleased] — Milestone 2: Texture Loading

### Added
- `Texture` class: load PNG/JPG/TGA via `stb_image`, upload to GL, mipmaps, repeat wrapping, linear-mip filtering; `Texture::white()` factory for untextured objects
- `Mesh::sphere(stacks, slices)`: UV-sphere geometry factory
- UV coords (`u`, `v`) added to `Vertex` struct; cube, plane, sphere all carry correct UVs
- `Shader::set(name, int)` overload for binding texture samplers
- `shaders/basic.vert`: passes UV through to fragment stage
- `shaders/basic.frag`: samples `uAlbedo` sampler2D; albedo drives base colour
- `assets/textures/checker.png`: 64×64 grey checkerboard test texture
- `external/stb/stb_image.h`: committed single-header stb_image (public domain)
- Scene updated: spinning textured cube + textured sphere, white ground plane

---

## [M1 — Hello 3D World] — 2026-05-30

### Added
- `Window` class: GLFW window + OpenGL 3.3 Core Profile context, resize handling, vsync
- `Shader` class: GLSL vertex/fragment compile-link pipeline, `set()` uniform overloads for `mat4`, `vec3`, `float`
- `Camera` class: perspective projection, yaw/pitch fly-through, WASD + Space/Shift + mouse look
- `Mesh` class: VAO/VBO/EBO geometry owner; `Mesh::cube()` and `Mesh::plane()` static factories
- `shaders/basic.vert` / `shaders/basic.frag`: MVP transform + directional diffuse shading
- `CMakeLists.txt`: FetchContent for GLFW 3.4 and GLM 1.0.1; OpenGL via macOS system framework
- `CMakePresets.json`: `dev` preset (Debug build, compile commands exported)
- Initial scene: spinning cube, static tilted cube, ground plane; depth test enabled
