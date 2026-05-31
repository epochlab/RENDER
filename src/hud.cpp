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
    s.fpsHistory[s.fpsHistoryOffset] = s.fps;
    s.fpsHistoryOffset = (s.fpsHistoryOffset + 1) % 128;

    // Frame-time min/max over the populated history window (fps → ms).
    float tMin = 1e9f, tMax = 0.0f;
    for (float f : s.fpsHistory) {
        if (f <= 0.0f) continue;            // skip unfilled slots
        float ms = 1000.0f / f;
        if (ms < tMin) tMin = ms;
        if (ms > tMax) tMax = ms;
    }
    if (tMax > 0.0f) { s.frameTimeMin = tMin; s.frameTimeMax = tMax; }

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

    // ── View ──────────────────────────────────────────────────
    sectionHeader("View");
    static const char* k_modeNames[] = {
        "beauty", "wireframe", "alpha", "depth", "world_pos",
        "world_normals", "uv", "albedo", "direct_diffuse", "direct_refl",
        "shading_normal", "ao"
    };
    int modeIdx = s.viewMode - 1;
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##channel", &modeIdx, k_modeNames, 12))
        s.viewMode = modeIdx + 1;

    // ── Frame ─────────────────────────────────────────────────
    sectionHeader("Frame");
    ImGui::Text("%.0f FPS  avg %.0f  %.2f ms", s.fps, s.fpsSmooth, s.frameTimeMs);
    ImGui::PlotLines("##fps", s.fpsHistory, 128, s.fpsHistoryOffset,
                     nullptr, 0.0f, 300.0f, {214.0f, 36.0f});
    ImGui::Text("min %.2f   max %.2f ms", s.frameTimeMin, s.frameTimeMax);
    ImGui::Text("GPU   %.3f ms", s.gpuTimeMs);
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
    ImGui::Text("rot  x %.1f   y %.1f   z %.1f", s.camRotX,  s.camRotY,  s.camRotZ);
    ImGui::Spacing();
    ImGui::Text("Filmback   %.1f mm", s.camFilmbackMm);
    ImGui::Text("Focal      %.1f mm", s.camFocalLengthMm);
    ImGui::Text("Near  %.2f   Far  %.1f", s.camNear, s.camFar);

    // ── GPU ───────────────────────────────────────────────────
    sectionHeader("GPU");
    ImGui::TextWrapped("%s", m_sys.renderer);
    ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "GL %s", m_sys.version);

    ImGui::End();
}

void HUD::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
