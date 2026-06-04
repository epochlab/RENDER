# Context Files
Read at the start of each session:
- `.claude/MEMORY.md` — architecture, build commands, dependencies, gotchas
- `.claude/TASKS.md` — feature roadmap and completed milestones

# Environment
- Install C++ system dependencies with Homebrew: `brew install <pkg>`.
- Do not add a dependency without flagging it. Prefer what's already in use (see MEMORY.md).

# Build & Test
Exact commands, in order. Do not guess build invocations.

```bash
make                                          # C++ build → ./build/KODAK
ctest --preset default --output-on-failure    # C++ tests
```

Single test: `ctest --preset default -R '<regex>'`.

Before claiming done: `clang-tidy -p build <changed .cpp>`.

# Architecture
C++ rendering engine in `src/`; GLSL shaders in `shaders/`; JSON configs in `config/`.
No Python, no bindings. See MEMORY.md for directory map and dependency list.

# Gotchas
Append a rule here whenever a convention is violated ("Update CLAUDE.md so you don't repeat this").

- Determinism is required. Every stochastic path takes an explicit seed. Never use a global RNG. Results must be bit-reproducible for a fixed seed.
- Do not reorder floating-point reductions or add `-ffast-math`/`-march=native`; they change results.
- `geoSmithIBL` and `sampleHdri` are intentionally duplicated across PBR/bake shaders — do not consolidate.
- No shader hot-reload; GLSL changes require a full rebuild and re-run.

# Out of scope unless asked
No reformatting untouched files, no modernising working code, no new abstraction layers. Minimal diff that satisfies the task and its tests.
