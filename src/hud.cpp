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
    m_sys.vendor   = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
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
    ImGui::Text("[%d] %s", s.viewMode, s.viewModeName);

    // ── Frame ─────────────────────────────────────────────────
    sectionHeader("Frame");
    ImGui::Text("%.0f FPS   %.2f ms", s.fps, s.frameTimeMs);
    ImGui::PlotLines("##fps", s.fpsHistory, 128, s.fpsHistoryOffset,
                     nullptr, 0.0f, 300.0f, {214.0f, 36.0f});
    ImGui::Text("GPU   %.3f ms", s.gpuTimeMs);
    if (s.memMB > 0.0f)
        ImGui::Text("Mem   %.1f MB", s.memMB);

    // ── Viewport ──────────────────────────────────────────────
    sectionHeader("Viewport");
    ImGui::Text("%d x %d", s.width, s.height);

    // ── Scene ─────────────────────────────────────────────────
    sectionHeader("Scene");
    ImGui::Text("Objects      %d", s.numObjects);
    ImGui::Text("Draw calls   %d", s.drawCalls);
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
