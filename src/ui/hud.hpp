#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

struct ObjectInfo {
    const char* name      = "";
    int         triangles = 0;
    int         points    = 0;
};

struct SystemInfo {
    const char* renderer = "";   // GL_RENDERER
    const char* version  = "";   // GL_VERSION
};

struct FrameStats {
    // Timing
    float     fps             = 0.0f;   // raw 1/dt (used for the history plot)
    float     fpsSmooth       = 0.0f;   // EMA-smoothed, shown as the headline number
    float     frameTimeMs     = 0.0f;
    float     frameTimeMin    = 0.0f;   // over the 128-frame history window
    float     frameTimeMax    = 0.0f;
    float     gpuGeomMs       = 0.0f;   // geometry pass GPU time
    float     gpuSsaoMs       = 0.0f;   // SSAO pass GPU time
    float     gpuBlurMs       = 0.0f;   // blur pass GPU time
    float     gpuPostMs       = 0.0f;   // composite blit GPU time
    float     triPerSec       = 0.0f;   // Mtri/s
    float     mpixPerSec      = 0.0f;   // Mpix/s
    int       frameCap        = 0;      // 0 = vsync/unlimited
    bool      benchmarkMode  = false;
    float     fpsHistory[128]{};
    int       fpsHistoryOffset = 0;

    // Viewport
    int       width           = 0;   // physical framebuffer (render resolution)
    int       height          = 0;
    int       logicalWidth    = 0;   // logical window size (on-screen pixels)
    int       logicalHeight   = 0;
    int       downsample      = 1;

    // Scene geometry
    int       drawCalls       = 0;   // draw calls actually issued (after culling)
    int       drawCallsTotal  = 0;   // potential draw calls (objects in scene)
    int       drawCallsCulled = 0;   // skipped by frustum cull
    int       totalTriangles  = 0;
    int       totalPoints     = 0;
    ObjectInfo objects[8]{};
    int       numObjects      = 0;

    // Memory
    float     memMB           = 0.0f;   // process phys_footprint (RAM)
    float     gpuAllocMB      = 0.0f;   // tracked GPU allocation (mesh buffers + FBO)

    // Camera
    glm::vec3 camPos          = {};
    float     camRotX         = 0.0f;   // pitch
    float     camRotY         = 0.0f;   // yaw
    float     camFilmbackMm   = 36.0f;
    float     camFocalLengthMm = 31.2f;
    float     camNear         = 0.0f;
    float     camFar          = 0.0f;
    float     camISO          = 100.f;
    float     camFStop        =   8.f;
    float     camShutterSpeed =   0.01f;
    float     camFocusDist    =  10.f;
    bool      camDofEnabled   = false;
    bool      camAspectEnabled = false;
    float     camAspectRatio  =   2.35f;

    // View mode
    int         viewMode      = 1;

    // Histogram (R=0, G=1, B=2; 256 bins each)
    uint32_t    hist[3][256]{};
    int         histValid     = 0;

    // HDRI controls
    float       hdriYawDeg   = 0.0f;
    bool        hdriFlipV    = false;
    float       hdriIntensity = 1.0f;
    float       hdriEvOffset  = 0.0f;  // cfg.hdri.exposure — EV offset in stops

    // Channel / invert mode
    int         channelView   = 0;    // 0=RGB 1=R 2=G 3=B

    // Color management
    int         viewLut        = 1;     // 0=Raw  1=ACES_sRGB  2=ACES_Rec709
    bool        viewLutChanged = false;

    // Menu / panel state
    bool        skyVisible    = false;
    bool        showPanel     = true;
    bool        doCaptureEXR  = false;
    bool        doCapturePNG  = false;
    bool        doSaveJson    = false;
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
