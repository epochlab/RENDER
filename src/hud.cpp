#include "hud.hpp"
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

    // ── Floating restore button (visible only when panel is hidden) ──
    if (!s.showPanel) {
        const ImGuiWindowFlags btnFlags =
            ImGuiWindowFlags_NoDecoration    |
            ImGuiWindowFlags_NoNav           |
            ImGuiWindowFlags_NoMove          |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize;
        float dispW = ImGui::GetIO().DisplaySize.x;
        ImGui::SetNextWindowPos({dispW - 46.0f, 10.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.55f);
        ImGui::Begin("##restore", nullptr, btnFlags);
        if (ImGui::Button(" \xE2\x97\x88 "))   // UTF-8 ◈
            s.showPanel = true;
        ImGui::End();
        return;
    }

    // ── Stats panel ───────────────────────────────────────────
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration    |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoNav           |
        ImGuiWindowFlags_NoMove          |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::SetNextWindowPos({10.0f, 10.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::SetNextWindowSize({230.0f, 0.0f});

    if (!ImGui::Begin("##hud", nullptr, flags)) { ImGui::End(); return; }

    // ── GPU ───────────────────────────────────────────────────
    sectionHeader("GPU");
    ImGui::TextWrapped("%s", m_sys.renderer);
    ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "GL %s", m_sys.version);

    // ── Frame ─────────────────────────────────────────────────
    sectionHeader("Frame");
    ImGui::Text("%.0f FPS  avg %.0f  %.2f ms", s.fps, s.fpsSmooth, s.frameTimeMs);
    ImGui::PlotLines("##fps", s.fpsHistory, 128, s.fpsHistoryOffset,
                     nullptr, 0.0f, 300.0f, {214.0f, 36.0f});
    ImGui::Text("min %.2f   max %.2f ms", s.frameTimeMin, s.frameTimeMax);
    ImGui::Text("GPU  geom %.2f  post %.2f ms", s.gpuGeomMs, s.gpuPostMs);
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
    ImGui::Text("Vertices     %d", s.totalVertices);
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

    // ── AOV ───────────────────────────────────────────────────
    sectionHeader("AOV");
    static const char* k_modeNames[] = {
        "beauty", "alpha", "luminance", "hsv",
        "bounds", "wireframe",
        "depth", "world_pos", "world_normals", "uv", "albedo",
        "direct_diffuse", "direct_refl", "shading_normal", "ao", "fresnel"
    };
    int modeIdx = s.viewMode - 1;
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##channel", &modeIdx, k_modeNames, 16))
        s.viewMode = modeIdx + 1;

    // ── Histogram ─────────────────────────────────────────────
    if (s.histValid) {
        sectionHeader("Histogram");

        uint32_t peak = 1;
        for (int c = 0; c < 3; ++c)
            for (int b = 0; b < 256; ++b)
                if (s.hist[c][b] > peak) peak = s.hist[c][b];

        bool isGray = true;
        for (int b = 0; b < 256 && isGray; ++b)
            isGray = (s.hist[0][b] == s.hist[1][b] && s.hist[1][b] == s.hist[2][b]);

        const float  W   = ImGui::GetContentRegionAvail().x;
        const float  H   = 72.0f;
        ImVec2       pos = ImGui::GetCursorScreenPos();
        ImDrawList*  dl  = ImGui::GetWindowDrawList();
        const float  bw  = W / 256.0f;

        dl->AddRectFilled(pos, {pos.x + W, pos.y + H}, IM_COL32(18, 18, 18, 255));

        auto drawCurve = [&](int c, ImU32 col) {
            dl->PathClear();
            dl->PathLineTo({pos.x, pos.y + H});
            for (int b = 0; b < 256; ++b) {
                float norm = sqrtf((float)s.hist[c][b] / (float)peak);
                dl->PathLineTo({pos.x + (b + 0.5f) * bw, pos.y + H - norm * H});
            }
            dl->PathLineTo({pos.x + W, pos.y + H});
            dl->PathFillConvex(col);
        };

        if (isGray) {
            drawCurve(0, IM_COL32(204, 204, 204, 204));
        } else {
            drawCurve(2, IM_COL32( 60, 100, 220, 180));   // B
            drawCurve(1, IM_COL32( 60, 200,  80, 180));   // G
            drawCurve(0, IM_COL32(220,  60,  60, 180));   // R on top
        }

        dl->AddRect(pos, {pos.x + W, pos.y + H}, IM_COL32(60, 60, 60, 180));
        ImGui::Dummy({W, H});
    }

    // ── HDRI ──────────────────────────────────────────────────
    sectionHeader("HDRI");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##hdriYaw", &s.hdriYawDeg, 1.0f, 360.0f, "Y-axis  %.0f deg");
    ImGui::Checkbox("Flip Y-axis", &s.hdriFlipV);
    ImGui::Checkbox("Enable Background", &s.skyVisible);

    ImGui::End();
}

void HUD::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
