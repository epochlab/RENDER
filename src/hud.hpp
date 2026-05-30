#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

struct FrameStats {
    float     fps            = 0.0f;
    float     frameTimeMs    = 0.0f;
    float     fpsHistory[128]{};
    int       fpsHistoryOffset = 0;
    int       drawCalls      = 0;
    int       totalTriangles = 0;
    int       totalVertices  = 0;
    int       width          = 0;
    int       height         = 0;
    glm::vec3   camPos         = {};
    float       camYaw         = 0.0f;
    float       camPitch       = 0.0f;
    int         viewMode       = 1;
    const char* viewModeName   = "Diffuse";
};

class HUD {
public:
    explicit HUD(GLFWwindow* window);
    ~HUD();

    HUD(const HUD&) = delete;
    HUD& operator=(const HUD&) = delete;

    void beginFrame();
    void draw(FrameStats& stats);   // non-const: updates fpsHistory ring buffer
    void endFrame();
};
