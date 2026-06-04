# Camera — ISO, F-Stop, Shutter Speed, Focus Distance, Aspect Ratio

Branch: `feat/camera-exposure-dof`

---

## Step 1 — Camera class: exposure parameters [ ]

**Files:** `src/camera/camera.hpp`, `src/camera/camera.cpp`

Add private members `m_iso`, `m_fStop`, `m_shutterSpeed`, `m_focusDist` with clamped setters/getters.

Add `exposureValue()` — returns linear multiplier normalised to 1.0 at ISO 100 / f8 / 1/100s:
```
exposure = (shutterSpeed * iso) / (fStop² * 100)
```

Add `cocScale(float imageWidthPx)` — pixel scale factor for per-pixel CoC computation in DoF shader:
```
cocScale = imageWidthPx * focalLength_mm / (fStop * filmback_mm)
```

---

## Step 2 — Config: persist new camera params [ ]

**Files:** `src/core/config.hpp`, `src/core/config.cpp`

Extend `AppConfig::Camera` with `iso`, `fStop`, `shutterSpeed`, `focusDist` (defaults match normalised exposure reference point).

`loadConfig()`: read four new keys with `contains()` guard.
`saveConfig()`: write four new keys alongside existing camera fields.

---

## Step 3 — blit.frag: exposure + aspect letterbox [ ]

**File:** `shaders/post/blit.frag`

Add `uniform float uExposure` — multiply `color *= uExposure` in beauty path before channel view / invert.

Add `uniform int uAspectMode` (0=off, 1=2.39:1, 2=1.85:1) — after all colour ops, draw black bars when `vUV.y` falls outside the letterbox window. Letterbox height computed from `textureSize` so it works at any render resolution.

Pre-cache both uniform locations in `main.cpp` alongside existing blit locations.

---

## Step 4 — DoF pass: new shader [ ]

**File:** `shaders/post/dof.frag` (new — reuses `blit.vert`)

Pipeline position: after SSAO blur, before blit composite.

Per pixel:
1. Linearise depth from G-buffer.
2. Skip sky (linDepth > far * 0.99) → passthrough.
3. `CoC_px = cocScale * abs(linDepth - focusDist) / linDepth`, clamped to 32 px.
4. Skip if CoC < 0.5 → passthrough.
5. 16-tap Poisson disc sample over `CoC_px * texelSize` radius → averaged colour.

Fast path: `if (uCocScale < 0.5) { FragColor = texture(uFrame, vUV); return; }` — pinhole camera costs one texture fetch.

Uniforms: `uFrame`, `uDepth`, `uNear`, `uFar`, `uFocusDist`, `uCocScale`.

---

## Step 5 — main.cpp: wire DoF pass + exposure [ ]

**File:** `src/main.cpp`

**DoF FBO**: `dofTex` (GL_RGB16F, BASE_W × BASE_H) + `dofFbo` — created after `rt`.

**DoF shader**: loaded alongside SSAO blur shaders; all six uniform locations pre-cached.

**Render loop** (after SSAO blur V-pass):
- Bind `dofFbo`, run `dofShader` reading from `rt.colorTex` + `rt.depthTex`.
- Blit composite reads `dofTex` instead of `rt.colorTex` for `uFrame`.

**Exposure**:
```cpp
float physExposure  = camera.exposureValue();
float finalExposure = physExposure * std::pow(2.0f, cfg.hdri.exposure); // hdri.exposure = EV offset
blitShader.setAt(blitLocExposure, finalExposure);
```
IBL baking: pass `uHdriExposure = 1.0f` (neutral bake; exposure applied in blit).

**Camera init from config**: `setISO`, `setFStop`, `setShutterSpeed`, `setFocusDist` after existing `setFocalLength`.

**FrameStats sync**:
- End-of-frame: read four new fields from `camera` into `stats`.
- Post-HUD: write four fields from `stats` back to `camera` and `cfg.camera`.

**Aspect mode**: pass `stats.camAspectMode` to `blitShader`.

---

## Step 6 — HUD: camera panel sliders [ ]

**Files:** `src/ui/hud.hpp`, `src/ui/hud.cpp`

`FrameStats` additions: `camISO`, `camFStop`, `camShutterSpeed`, `camFocusDist`, `camAspectMode`.

Camera panel (after filmback / focal length):
- ISO: `SliderInt` over index 0–7 → `100 * 2^idx`; display "ISO N" below.
- f-stop: `SliderFloat` 1.0–22.0 logarithmic, format `"f/%.1f"`.
- Shutter: `SliderFloat` 1/8000–1/4 s logarithmic; display as `"1/N s"` when < 0.1 s.
- Focus dist: `SliderFloat` 0.1–100 m logarithmic.
- EV₁₀₀ readout (display only): `log2(N²/t) - log2(ISO/100)`.
- Aspect guides: `ImGui::Combo` — Off / 2.39:1 Scope / 1.85:1 Flat.

---

## Step 7 — Tests [ ]

**File:** `tests/test_camera.cpp`

Five new tests (target: 92 → 97 total):

| Test | Assertion |
|------|-----------|
| Reference point | ISO 100, f8, 1/100s → `exposureValue() == 1.0` |
| Double ISO | exposure doubles |
| Double f-stop | exposure quarters |
| Double shutter | exposure doubles |
| Pinhole CoC | `cocScale(2048) < 0.5` at f/1000 |

---

## Verification

```bash
make                                          # clean build
ctest --preset default --output-on-failure   # 92 → 97 tests pass
ctest --preset default -R Camera             # camera suite specifically
./build/KODAK                                # visual check
```

Visual checks:
1. Default renders identically to `main` (exposure=1, fStop=8, ISO=100, shutter=1/100s)
2. Raise ISO → scene brightens
3. Raise f-stop → scene darkens
4. Open shutter → scene brightens
5. Low f-stop + focus near → background blurs
6. Enable 2.39:1 → correct black bars
7. Save JSON → reload → values preserved
