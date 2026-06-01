# Environment
- Environments are conda. Activate before anything: `conda activate <env>`. [EDIT name]
- Install Python packages with `pip`, never `pip3`.
- Install C++ system dependencies with Homebrew: `brew install <pkg>`.
- Do not add a dependency without flagging it. Prefer stdlib, then NumPy/SciPy, then nothing.

# Build & Test
Exact commands, in order. Do not guess build invocations.

```bash
make   # C++ build → ./build/KODAK
ctest --preset default --output-on-failure            # C++ tests
pip install -e . --no-build-isolation                 # rebuild binding if C++ changed
pytest -x -q                                          # Python tests
```

Single test: `ctest --preset default -R '<regex>'` or `pytest path::test_name`.

Before claiming done: `ruff check . && mypy <package> && clang-tidy -p build <changed .cpp>`.
If you touched the C++ core, the Python tests still need to pass.

# Architecture
C++ numerical core in `src/`; Python orchestration/analysis in `<package>/`.
They meet only at the pybind11 binding (`src/bindings.cpp`). A signature change there is an API change: update both sides and their tests in the same change.

# Gotchas
Append a rule here whenever a convention is violated ("Update CLAUDE.md so you don't repeat this").

- Determinism is required. Every stochastic path takes an explicit seed. Never use a global RNG. Results must be bit-reproducible for a fixed seed.
- Do not reorder floating-point reductions or add `-ffast-math`/`-march=native` for tidiness; they change results.
- NumPy arrays at the binding are `float64`, C-contiguous. Mismatch makes pybind11 silently copy — a correctness and performance bug. Assert, don't copy.

# Out of scope unless asked
No reformatting untouched files, no modernising working numerical code, no new abstraction layers. Minimal diff that satisfies the task and its tests.