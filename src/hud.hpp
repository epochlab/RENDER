#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

struct ObjectInfo {
    const char* name      = "";
    int         triangles = 0;
    int         vertices  = 0;
};

struct SystemInfo {
    const char* renderer = "";   // GL_RENDERER
    const char* vendor   = "";   // GL_VENDOR
    const char* version  = "";   // GL_VERSION
};

struct FrameStats {
    // Timing
    float     fps             = 0.0f;
    float     frameTimeMs     = 0.0f;
    float     gpuTimeMs       = 0.0f;
    float     fpsHistory[128]{};
    int       fpsHistoryOffset = 0;

    // Viewport
    int       width           = 0;
    int       height          = 0;

    // Scene geometry
    int       drawCalls       = 0;
    int       totalTriangles  = 0;
    int       totalVertices   = 0;
    ObjectInfo objects[8]{};
    int       numObjects      = 0;

    // Memory
    float     memMB           = 0.0f;

    // Camera
    glm::vec3 camPos          = {};
    float     camRotX         = 0.0f;   // pitch
    float     camRotY         = 0.0f;   // yaw
    float     camRotZ         = 0.0f;   // roll (unused, always 0)
    float     camFilmbackMm   = 36.0f;
    float     camFocalLengthMm = 31.2f;
    float     camNear         = 0.0f;
    float     camFar          = 0.0f;

    // View mode
    int         viewMode      = 1;
    const char* viewModeName  = "Diffuse";
};

class HUD {
public:
    explicit HUD(GLFWwindow* window);
    ~HUD();

    HUD(const HUD&) = delete;
    HUD& operator=(const HUD&) = delete;

    void beginFrame();
    void draw(FrameStats& stats);
    void endFrame();

private:
    SystemInfo m_sys;
};
