# Changelog

All notable changes to this project will be documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [Unreleased] — Milestone 3: Debug HUD + View Modes

### Added
- `HUD` class: Dear ImGui v1.91.9 overlay (GLFW + OpenGL 3.3 backends); pinned top-left, semi-transparent
- `FrameStats`: FPS + 128-sample rolling graph, GPU timer (GL_TIME_ELAPSED), process memory (mach), viewport, scene geometry, per-object triangle counts, camera state, render scale
- 6 render view modes (keys 1–6): Diffuse, Wireframe, Depth, Position, Normals, UV
- FBO render scale (`RENDER_SCALE=2`): scene rendered at half resolution, blit-upscaled to window; HUD shows native + scaled resolution
- `blit.vert` / `blit.frag`: fullscreen-triangle blit shaders; foundation for M6 post-processing
- Cinematic camera model: filmback (35mm) + focal length (50mm) replacing raw FOV angle
- Camera pos/rot shown as x/y/z axes; filmback, focal length, near/far in HUD
- `Camera::resetMouse()`: resets mouse delta tracking; called on window resize to prevent camera jump
- `Window::Window()` now calls `glfwGetFramebufferSize` to get correct initial pixel dimensions on Retina
- Camera vertical keys: **E** = up, **Q** = down (WASD + Q/E)
- Sphere reduced to 16×16 (512 tris); plane UVs span 0→1 across full 10m grid
- Window 2048×1152; camera starts at (0, 1, 10); far plane 100

### Fixed
- FBO blit not filling window on Retina displays (logical vs physical pixel mismatch)
- Window resize causing camera to jump (firstMouse flag now reset on resize)
- Unicode → in HUD replaced with ASCII -> (ImGui default font lacks U+2192)

---

## [M2 — Texture Loading] — 2026-05-30

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
