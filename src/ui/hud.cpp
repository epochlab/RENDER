#include "hud.hpp"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

HUD::HUD(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding   = 4.0f;
    style.FrameRounding    = 3.0f;
    style.WindowBorderSize = 0.0f;
    style.ItemSpacing      = {8.0f, 4.0f};

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // Query GPU info once — GL context is current here
    m_sys.renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    m_sys.version  = reinterpret_cast<const char*>(glGetString(GL_VERSION));
}

HUD::~HUD() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void HUD::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

static void sectionHeader(const char* label) {
    ImGui::Spacing();
    ImGui::TextColored({0.4f, 0.85f, 1.0f, 1.0f}, "%s", label);
    ImGui::Separator();
}

void HUD::draw(FrameStats& s) {
    // Crosshair (+) at screen centre — always visible.
    {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        ImVec2 c(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
        const float  arm = 8.0f;
        const ImU32  col = IM_COL32(255, 255, 255, 200);
        dl->AddLine({c.x - arm, c.y}, {c.x + arm, c.y}, col, 1.0f);
        dl->AddLine({c.x, c.y - arm}, {c.x, c.y + arm}, col, 1.0f);
    }

    s.fpsHistory[s.fpsHistoryOffset] = s.fps;
    s.fpsHistoryOffset = (s.fpsHistoryOffset + 1) % 128;

    // Frame-time min/max over the populated history window (fps → ms).
    float tMin = 1e9f, tMax = 0.0f;
    for (float f : s.fpsHistory) {
        if (f <= 0.0f) continue;
        float ms = 1000.0f / f;
        if (ms < tMin) tMin = ms;
        if (ms > tMax) tMax = ms;
    }
    if (tMax > 0.0f) { s.frameTimeMin = tMin; s.frameTimeMax = tMax; }

    // ── Channel / invert mode label — top-right, always visible when active ──
    {
        ImDrawList*  dl   = ImGui::GetForegroundDrawList();
        ImVec2       disp = ImGui::GetIO().DisplaySize;
        ImFont*      font = ImGui::GetFont();
        const float  fs   = ImGui::GetFontSize() + 5.0f;
        const float  pad  = 15.0f;
        float        y    = pad;

        auto drawLabel = [&](const char* text, ImU32 col) {
            ImVec2 sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, text);
            dl->AddText(font, fs, {disp.x - sz.x - pad, y}, col, text);
            y += fs + 2.0f;
        };

        if      (s.channelView == 1) drawLabel("R", IM_COL32(255,  80,  80, 230));
        else if (s.channelView == 2) drawLabel("G", IM_COL32( 80, 220,  80, 230));
        else if (s.channelView == 3) drawLabel("B", IM_COL32( 80, 140, 255, 230));
    }

    if (!s.showPanel) return;

    // ── Stats panel ───────────────────────────────────────────
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration    |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoNav           |
        ImGuiWindowFlags_NoMove          |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::SetNextWindowPos({10.0f, 10.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::SetNextWindowSize({260.0f, 0.0f});

    if (!ImGui::Begin("##hud", nullptr, flags)) { ImGui::End(); return; }

    // ── GPU ───────────────────────────────────────────────────
    sectionHeader("GPU");
    ImGui::TextWrapped("%s", m_sys.renderer);
    ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "GL %s", m_sys.version);

    // ── Frame ─────────────────────────────────────────────────
    sectionHeader("Frame");
    ImGui::Text("%.0f FPS  avg %.0f  %.2f ms", s.fps, s.fpsSmooth, s.frameTimeMs);
    {
        constexpr float kFpsMax = 300.0f;
        constexpr ImVec2 kGraphSz{214.0f, 36.0f};
        ImVec2 gp = ImGui::GetCursorScreenPos();
        ImGui::PlotLines("##fps", s.fpsHistory, 128, s.fpsHistoryOffset,
                         nullptr, 0.0f, kFpsMax, kGraphSz);
        if (s.fpsSmooth > 0.0f) {
            float y = gp.y + kGraphSz.y * (1.0f - std::min(s.fpsSmooth, kFpsMax) / kFpsMax);
            ImGui::GetWindowDrawList()->AddLine(
                {gp.x, y}, {gp.x + kGraphSz.x, y}, IM_COL32(220, 60, 60, 200), 1.0f);
        }
    }
    ImGui::Text("min %.2f   max %.2f ms", s.frameTimeMin, s.frameTimeMax);
    ImGui::Text("GPU  geom %.2f  ssao %.2f  blur %.2f  comp %.2f ms",
                s.gpuGeomMs, s.gpuSsaoMs, s.gpuBlurMs, s.gpuPostMs);
    ImGui::Text("     %.1f Mtri/s  %.1f Mpix/s", s.triPerSec, s.mpixPerSec);
    if (s.frameCap > 0)
        ImGui::Text("Cap   %d fps", s.frameCap);
    else
        ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "Cap   vsync");

    // ── Memory ────────────────────────────────────────────────
    sectionHeader("Memory");
    if (s.memMB > 0.0f)
        ImGui::Text("RAM        %.1f MB", s.memMB);
    ImGui::Text("GPU alloc  %.1f MB", s.gpuAllocMB);
    ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "(meshes + FBO, tracked)");

    // ── Viewport ──────────────────────────────────────────────
    sectionHeader("Viewport");
    if (s.downsample > 1)
        ImGui::Text("%d x %d  (x%d -> %d x %d)",
                    s.width, s.height, s.downsample,
                    s.logicalWidth, s.logicalHeight);
    else
        ImGui::Text("%d x %d", s.width, s.height);

    // ── Scene ─────────────────────────────────────────────────
    sectionHeader("Scene");
    ImGui::Text("Objects      %d", s.numObjects);
    ImGui::Text("Draw calls   %d / %d  (%d culled)",
                s.drawCalls, s.drawCallsTotal, s.drawCallsCulled);
    ImGui::Text("Triangles    %d", s.totalTriangles);
    ImGui::Text("Points       %d", s.totalPoints);
    if (s.numObjects > 0) {
        ImGui::Spacing();
        for (int i = 0; i < s.numObjects; ++i)
            ImGui::Text("  %-10s  %4d tris", s.objects[i].name, s.objects[i].triangles);
    }

    // ── Camera ────────────────────────────────────────────────
    sectionHeader("Camera");
    ImGui::Text("pos  x %.2f   y %.2f   z %.2f", s.camPos.x, s.camPos.y, s.camPos.z);
    ImGui::Text("rot  x %.1f   y %.1f", s.camRotX, s.camRotY);
    ImGui::Spacing();
    ImGui::Text("Filmback   %.1f mm", s.camFilmbackMm);
    ImGui::Text("Focal      %.1f mm", s.camFocalLengthMm);
    ImGui::Text("Near  %.2f   Far  %.1f", s.camNear, s.camFar);

    // ── Lens ──────────────────────────────────────────────────
    sectionHeader("Lens");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##focalLength", &s.camFocalLengthMm, 8.0f, 200.0f, "Focal Length  %.0f mm");

    // ── Exposure ──────────────────────────────────────────────
    sectionHeader("Exposure");
    {
        // ISO — display value in format string (avoids SameLine overflow)
        int isoIdx = static_cast<int>(std::log2(std::max(s.camISO, 1.f) / 100.f));
        isoIdx = std::clamp(isoIdx, 0, 7);
        const char* isoLabels[] = {"ISO 100","ISO 200","ISO 400","ISO 800",
                                   "ISO 1600","ISO 3200","ISO 6400","ISO 12800"};
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderInt("##iso", &isoIdx, 0, 7, isoLabels[isoIdx]))
            s.camISO = 100.f * std::pow(2.f, static_cast<float>(isoIdx));
    }
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##fstop", &s.camFStop, 1.0f, 22.0f, "f/%.1f", ImGuiSliderFlags_Logarithmic);
    {
        // Shutter speed: pre-compute fraction string so it displays inside the slider
        char shutterFmt[32];
        if (s.camShutterSpeed < 0.1f)
            snprintf(shutterFmt, sizeof(shutterFmt), "1/%.0f s", 1.0f / s.camShutterSpeed);
        else
            snprintf(shutterFmt, sizeof(shutterFmt), "%.2f s", s.camShutterSpeed);
        float shutterDisp = s.camShutterSpeed;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderFloat("##shutter", &shutterDisp, 1.f/8000.f, 30.f, shutterFmt, ImGuiSliderFlags_Logarithmic))
            s.camShutterSpeed = shutterDisp;
    }
    {
        // EV₁₀₀ read-out
        float ev100 = std::log2((s.camFStop * s.camFStop) / s.camShutterSpeed)
                    - std::log2(s.camISO / 100.f);
        ImGui::Text("EV100  %.2f", ev100);
    }

    // ── Color ─────────────────────────────────────────────────
    sectionHeader("ViewerLUT");
    {
        static const char* k_lutNames[] = { "RAW", "ACES sRGB", "ACES Rec.709" };
        int idx = s.viewLut;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##viewlut", &idx, k_lutNames, 3)) {
            s.viewLut        = idx;
            s.viewLutChanged = true;
        }
    }

    // ── Depth of Field ────────────────────────────────────────
    sectionHeader("Depth of Field");
    ImGui::Checkbox("Enable DoF", &s.camDofEnabled);
    if (s.camDofEnabled) {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat("##focusDist", &s.camFocusDist, 0.1f, 100.f, "%.2f m", ImGuiSliderFlags_Logarithmic);
    }

    // ── Aspect Ratio ──────────────────────────────────────────
    sectionHeader("Aspect Ratio");
    ImGui::Checkbox("Letterbox", &s.camAspectEnabled);
    if (s.camAspectEnabled) {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat("##ratio", &s.camAspectRatio, 1.0f, 4.0f, "%.2f:1");
    }

    // ── AOV ───────────────────────────────────────────────────
    sectionHeader("AOV");
    static const char* k_modeNames[] = {
        "beauty", "alpha", "bounds", "wireframe", "depth",
        "albedo", "hsv", "luminance",
        "direct_diffuse", "direct_refl", "world_pos", "world_normals", "uv",
        "shading_normal", "fresnel", "occlusion"
    };
    int modeIdx = s.viewMode - 1;
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##channel", &modeIdx, k_modeNames, 16))
        s.viewMode = modeIdx + 1;

    // ── Histogram ─────────────────────────────────────────────
    if (s.histValid) {
        sectionHeader("Histogram");

        bool isGray = true;
        for (int b = 0; b < 256 && isGray; ++b)
            isGray = (s.hist[0][b] == s.hist[1][b] && s.hist[1][b] == s.hist[2][b]);

        const float  W   = ImGui::GetContentRegionAvail().x;
        const float  H   = 72.0f;
        ImVec2       pos = ImGui::GetCursorScreenPos();
        ImDrawList*  dl  = ImGui::GetWindowDrawList();
        const float  bw  = W / 256.0f;

        dl->AddRectFilled(pos, {pos.x + W, pos.y + H}, IM_COL32(18, 18, 18, 255));

        // Pre-compute normalised heights.
        // Grayscale: full 256-bin peak, no smoothing — preserves spikes at 0/255 (alpha, depth).
        // Colour: interior-only peak + 9-bin smooth — avoids clipping bins dominating scale.
        // Grayscale non-binary uses ±2 (5-bin) to halve the spread vs colour.
        float smooth[3][256];
        auto smoothChannel = [&](int c, uint32_t peak, int radius = 4) {
            for (int b = 0; b < 256; ++b) {
                float sum = 0.0f, wsum = 0.0f;
                for (int k = b - radius; k <= b + radius; ++k) {
                    if (k < 1 || k > 254) continue;
                    float w = float(radius + 1 - std::abs(k - b));  // triangle kernel
                    sum  += w * sqrtf(float(std::min(s.hist[c][k], peak)) / float(peak));
                    wsum += w;
                }
                smooth[c][b] = wsum > 0.0f ? sum / wsum : 0.0f;
            }
        };

        // Detect channels that always output 0.0 (all pixel counts in bin 0).
        // Happens for 2-channel AOVs like UV and Fresnel where B is constant zero.
        bool channelEmpty[3] = {false, false, false};
        if (!isGray) {
            for (int c = 0; c < 3; ++c) {
                bool empty = true;
                for (int b = 1; b < 256; ++b)
                    if (s.hist[c][b] > 0) { empty = false; break; }
                channelEmpty[c] = empty;
            }
        }

        if (isGray) {
            // Near-binary (e.g. alpha mask): <1% of pixels in interior bins.
            // Use full-range peak + no smoothing so the end-spikes are visible.
            uint32_t interiorTotal = 0;
            for (int b = 1; b < 255; ++b) interiorTotal += s.hist[0][b];
            const bool nearBinary = interiorTotal < static_cast<uint32_t>(256 * 144 / 100);
            if (nearBinary) {
                uint32_t pk = 1;
                for (int b = 0; b < 256; ++b) pk = std::max(pk, s.hist[0][b]);
                for (int b = 0; b < 256; ++b)
                    smooth[0][b] = sqrtf(float(s.hist[0][b]) / float(pk));
            } else {
                uint32_t pk = 1;
                for (int b = 1; b < 255; ++b) pk = std::max(pk, s.hist[0][b]);
                smoothChannel(0, pk);
            }
        } else {
            uint32_t peak = 1;
            for (int c = 0; c < 3; ++c)
                if (!channelEmpty[c])
                    for (int b = 1; b < 255; ++b)
                        peak = std::max(peak, s.hist[c][b]);
            for (int c = 0; c < 3; ++c) smoothChannel(c, peak);
        }

        auto drawSmooth = [&](const float* vals, ImU32 fill, ImU32 line) {
            ImVec2 edge[256];
            for (int b = 0; b < 256; ++b)
                edge[b] = {pos.x + (b + 0.5f) * bw, pos.y + H * (1.0f - vals[b])};
            ImVec2 pts[258];
            pts[0] = {pos.x, pos.y + H};
            for (int b = 0; b < 256; ++b) pts[b + 1] = edge[b];
            pts[257] = {pos.x + W, pos.y + H};
            // Disable fill AA: prevents per-sub-triangle fringes from AddConcavePolyFilled
            // creating visible diagonal lines on internal triangulation edges.
            // The outline (AddPolyline below) retains its own AA for the smooth top curve.
            const ImDrawListFlags savedFlags = dl->Flags;
            dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
            dl->AddConcavePolyFilled(pts, 258, fill);
            dl->Flags = savedFlags;
            dl->AddPolyline(edge, 256, line, 0, 1.0f);
        };

        if (isGray) {
            drawSmooth(smooth[0], IM_COL32(180, 180, 180, 130), IM_COL32(220, 220, 220, 220));
        } else {
            if (!channelEmpty[2]) drawSmooth(smooth[2], IM_COL32( 40,  80, 200, 120), IM_COL32( 80, 140, 255, 220));  // B
            if (!channelEmpty[1]) drawSmooth(smooth[1], IM_COL32( 40, 180,  60, 120), IM_COL32( 80, 220, 100, 220));  // G
            if (!channelEmpty[0]) drawSmooth(smooth[0], IM_COL32(200,  40,  40, 120), IM_COL32(255, 100,  80, 220));  // R

            // Overlap over active channels only (2-ch UV/Fresnel: min(R,G); full RGB: min(R,G,B)).
            int activeCh = (!channelEmpty[0] ? 1 : 0) + (!channelEmpty[1] ? 1 : 0) + (!channelEmpty[2] ? 1 : 0);
            if (activeCh >= 2) {
                float overlap[256]; float overlapMax = 0.0f;
                for (int b = 0; b < 256; ++b) {
                    float ov = 1.0f;
                    for (int c = 0; c < 3; ++c)
                        if (!channelEmpty[c]) ov = std::min(ov, smooth[c][b]);
                    overlap[b] = ov;
                    overlapMax = std::max(overlapMax, ov);
                }
                if (overlapMax > 0.02f)
                    drawSmooth(overlap, IM_COL32(180, 180, 180, 160), IM_COL32(255, 255, 255, 220));
            }
        }

        dl->AddRect(pos, {pos.x + W, pos.y + H}, IM_COL32(60, 60, 60, 180));
        ImGui::Dummy({W, H});
    }

    // ── HDRI ──────────────────────────────────────────────────
    sectionHeader("HDRI");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##hdriYaw", &s.hdriYawDeg, 1.0f, 360.0f, "Y-axis  %.0f deg");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##hdriEv", &s.hdriEvOffset, -4.0f, 4.0f, "EV  %.2f");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##hdriIntensity", &s.hdriIntensity, 0.0f, 5.0f, "Intensity  %.2f");
    ImGui::Checkbox("Invert Y-axis", &s.hdriFlipV);
    ImGui::Checkbox("Enable Background", &s.skyVisible);

    ImGui::End();
}

void HUD::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
