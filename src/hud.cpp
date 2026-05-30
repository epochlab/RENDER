#include "hud.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

HUD::HUD(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // don't write imgui.ini

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding   = 4.0f;
    style.FrameRounding    = 3.0f;
    style.WindowBorderSize = 0.0f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
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

void HUD::draw(FrameStats& s) {
    // Update FPS history ring buffer
    s.fpsHistory[s.fpsHistoryOffset] = s.fps;
    s.fpsHistoryOffset = (s.fpsHistoryOffset + 1) % 128;

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration   |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoNav          |
        ImGuiWindowFlags_NoMove         |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::SetNextWindowPos({10.0f, 10.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);

    if (ImGui::Begin("##hud", nullptr, flags)) {

        // ── View mode ─────────────────────────────────────────
        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "View");
        ImGui::Separator();
        ImGui::Text("[%d] %s", s.viewMode, s.viewModeName);

        ImGui::Spacing();

        // ── Frame ─────────────────────────────────────────────
        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Frame");
        ImGui::Separator();
        ImGui::Text("%.0f FPS   %.2f ms", s.fps, s.frameTimeMs);
        ImGui::PlotLines("##fps", s.fpsHistory, 128, s.fpsHistoryOffset,
                         nullptr, 0.0f, 300.0f, {200.0f, 40.0f});

        ImGui::Spacing();

        // ── Viewport ──────────────────────────────────────────
        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Viewport");
        ImGui::Separator();
        ImGui::Text("%d x %d", s.width, s.height);

        ImGui::Spacing();

        // ── Geometry ──────────────────────────────────────────
        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Geometry");
        ImGui::Separator();
        ImGui::Text("Draw calls   %d",   s.drawCalls);
        ImGui::Text("Triangles    %d",   s.totalTriangles);
        ImGui::Text("Vertices     %d",   s.totalVertices);

        ImGui::Spacing();

        // ── Camera ────────────────────────────────────────────
        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Camera");
        ImGui::Separator();
        ImGui::Text("Pos  %.2f  %.2f  %.2f", s.camPos.x, s.camPos.y, s.camPos.z);
        ImGui::Text("Yaw  %.1f deg   Pitch %.1f deg", s.camYaw, s.camPitch);
    }
    ImGui::End();
}

void HUD::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
