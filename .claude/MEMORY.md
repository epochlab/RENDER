# Project Memory Index
<!-- Last updated: auto -->

## Build & Run
- Build command: `make` (CMake preset → `./build/KODAK`)
- Run command: `make run`
- Test command: `ctest --preset default --output-on-failure`
- Single test: `ctest --preset default -R '<regex>'`
- Entry point: `src/main.cpp`

## Architecture
- Language/stack: C++17, OpenGL 3.3+, macOS (Cocoa/GLFW)
- Key directories:
  - `src/` — C++ source (main loop, window, camera, mesh, shader, texture, model, config)
  - `shaders/` — GLSL vertex/fragment/compute shaders
  - `external/` — vendored stb_image headers
  - `config/` — JSON scene/renderer configs
  - `tests/` — Catch2 test suite (91 tests / 571 assertions)
  - `benchmarks/` — performance snapshots
  - `build/` — CMake output (gitignored)
- External dependencies (all via CMake FetchContent):
  - GLFW 3.4 — window/input
  - GLM 1.0.1 — math (vectors, matrices)
  - Dear ImGui v1.91.9 — UI overlay (GLFW + OpenGL3 backends)
  - cgltf v1.14 — glTF 2.0 geometry loading
  - nlohmann_json v3.11.3 — JSON config parsing
  - Catch2 v3.7.1 — test framework
  - stb_image / stb_image_write — bundled in `external/`

## Coding Conventions
- C++17; no `-ffast-math`, no `-march=native` (changes float results)
- Naming: `snake_case` for functions/variables, `PascalCase` for classes/types
- Determinism: every stochastic path takes an explicit seed; no global RNG
- No auto-formatting of untouched files unless explicitly asked
- Minimal diffs — do not refactor or modernise working code unless asked

## Workflow Rules
- Run tests before committing: `ctest --preset default --output-on-failure`
- Do not push directly to main; open a PR and let the user merge
- Do not add a dependency without flagging it first

## Known Gotchas
<!-- Claude appends here as patterns emerge -->
- `geoSmithIBL` and `sampleHdri` are intentionally duplicated across PBR/bake shaders — do not consolidate
- Do not reorder floating-point reductions; results must be bit-reproducible for a fixed seed
- No shader hot-reload; GLSL changes require a full rebuild and re-run

## Topic Files
- [TASKS.md](TASKS.md) — full feature roadmap and completed milestones
