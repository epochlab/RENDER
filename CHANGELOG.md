# Changelog

All notable changes to this project will be documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

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
