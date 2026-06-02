#include <GLFW/glfw3.h>
#include <imgui.h>
#include "menu_osx.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <mach/mach.h>
#include <stb_image_write.h>
#include <stdexcept>
#include <random>
#include <string>
#include <vector>
#include <cfloat>
#include <cstring>
#include <ctime>
#include "window.hpp"
#include "shader.hpp"
#include "camera.hpp"
#include "texture.hpp"
#include "hud.hpp"
#include "frustum.hpp"
#include "model.hpp"
#include "config.hpp"
#include "log.hpp"

static Camera* g_camera = nullptr;

static void onMouseMove(GLFWwindow*, double xpos, double ypos) {
    if (g_camera) g_camera->processMouseMove(xpos, ypos);
}

static float queryMemoryMB() {
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
        return 0.0f;
    return static_cast<float>(info.phys_footprint) / (1024.0f * 1024.0f);
}

// ── G-buffer render target (colour + view-space normals + depth texture) ─────
struct RenderTarget {
    GLuint fbo = 0, colorTex = 0, normalTex = 0, depthTex = 0;
    int    w   = 0, h = 0;

    void create(int width, int height) {
        w = width; h = height;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        auto mkTex = [&](GLuint& id, GLenum internalFmt, GLenum fmt, GLenum type, GLenum filter) {
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, type, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        };

        mkTex(colorTex,  GL_RGB16F,           GL_RGB,            GL_FLOAT, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

        mkTex(normalTex, GL_RGB16F,           GL_RGB,            GL_FLOAT, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normalTex, 0);

        mkTex(depthTex,  GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);

        GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, bufs);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void destroy() {
        if (fbo)       { glDeleteFramebuffers(1, &fbo);    fbo       = 0; }
        if (colorTex)  { glDeleteTextures(1, &colorTex);   colorTex  = 0; }
        if (normalTex) { glDeleteTextures(1, &normalTex);  normalTex = 0; }
        if (depthTex)  { glDeleteTextures(1, &depthTex);   depthTex  = 0; }
        w = h = 0;
    }

    // RGB16F × 2 + DEPTH32F: (6+6+4) bytes/pixel.
    size_t bytes() const { return static_cast<size_t>(w) * h * (6 + 6 + 4); }
};

// ── Single-channel float render target (SSAO raw + blur) ─────────────────────
struct SsaoTarget {
    GLuint fbo = 0, tex = 0;

    void create(int w, int h) {
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, w, h, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void destroy() {
        if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
        if (tex) { glDeleteTextures(1, &tex);     tex = 0; }
    }
};

int main() {
    try {
        constexpr int FRAME_CAP = 0;  // 0 = vsync/unlimited

        AppConfig cfg = loadConfig("profile.json");
        const int BASE_W      = cfg.render.width  > 0 ? cfg.render.width  : 2048;
        const int BASE_H      = cfg.render.height > 0 ? cfg.render.height : 1152;
        const int downsample  = cfg.render.downsample > 0 ? cfg.render.downsample : 2;
        const int AO_W        = BASE_W / 2;
        const int AO_H        = BASE_H / 2;

        Window win(BASE_W / downsample, BASE_H / downsample, "KODAK");
        glfwSetCursorPosCallback(win.handle(), onMouseMove);
        glfwSetWindowSizeCallback(win.handle(), [](GLFWwindow*, int, int) {
            if (g_camera) g_camera->resetMouse();
        });

        OsxMenuFlags menuFlags;
        menuFlags.skyVisible = cfg.hdri.visible;
        initOsxMenuBar(&menuFlags);

        // ── Shaders ────────────────────────────────────────────────
        Shader shader("shaders/basic.vert", "shaders/basic.frag");
        shader.use();
        shader.set("uAlbedo",     0);
        shader.set("uSkyHDR",     1);
        shader.set("uNormalMap",  2);
        shader.set("uIblSamples", cfg.render.iblSamples);

        Shader blitShader("shaders/blit.vert", "shaders/blit.frag");
        blitShader.use();
        blitShader.set("uFrame", 0);
        blitShader.set("uAO",    1);
        blitShader.set("uDepth", 2);

        Shader skyShader("shaders/sky.vert", "shaders/sky.frag");
        skyShader.use();
        skyShader.set("uSkyHDR", 0);

        Shader ssaoShader("shaders/ssao.vert", "shaders/ssao.frag");
        ssaoShader.use();
        ssaoShader.set("gNormal",   0);
        ssaoShader.set("gDepth",    1);
        ssaoShader.set("uNoiseTex", 2);

        Shader blurShader("shaders/ssao.vert", "shaders/ssao_blur.frag");
        blurShader.use();
        blurShader.set("uSSAO",        0);
        blurShader.set("uBlurRadius",  cfg.shading.ssaoBlurRadius);

        Shader lineShader("shaders/line.vert", "shaders/line.frag");

        // ── Camera ─────────────────────────────────────────────────
        Camera camera(cfg.camera.position, win.aspectRatio(),
                      cfg.camera.filmback, cfg.camera.focalLength,
                      cfg.camera.near,     cfg.camera.far);
        camera.setYaw(cfg.camera.yaw);
        camera.setPitch(cfg.camera.pitch);
        g_camera = &camera;

        HUD hud(win.handle());

        Texture skyTex(cfg.hdri.path);
        Model   geom = Model::loadGLTF(cfg.scene.geometry);

        // Startup diagnostics
        LOG_I(std::string("Renderer: ") +
              reinterpret_cast<const char*>(glGetString(GL_RENDERER)) + "  (" +
              reinterpret_cast<const char*>(glGetString(GL_VERSION)) + ")");
        LOG_I("Render: " + std::to_string(BASE_W / downsample) + "x" +
              std::to_string(BASE_H / downsample) + "  (downsample=" + std::to_string(downsample) + ")");
        LOG_I("Geometry: " + cfg.scene.geometry + "  — " +
              std::to_string(geom.triangleCount()) + " tris, " +
              std::to_string(geom.vertexCount()) + " verts");
        LOG_I("HDRI: " + cfg.hdri.path);

        // ── SSAO kernel (deterministic, seed 42) ───────────────────
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        std::vector<glm::vec3> ssaoKernel(64);
        for (int i = 0; i < 64; ++i) {
            glm::vec3 s(dist(rng)*2.0f-1.0f, dist(rng)*2.0f-1.0f, dist(rng));
            s = glm::normalize(s) * dist(rng);
            float scale = float(i) / 64.0f;
            s *= 0.1f + scale * scale * 0.9f;
            ssaoKernel[i] = s;
        }

        // ── SSAO noise texture (4×4, GL_REPEAT) ───────────────────
        std::vector<glm::vec3> noiseData(16);
        for (auto& n : noiseData)
            n = glm::vec3(dist(rng)*2.0f-1.0f, dist(rng)*2.0f-1.0f, 0.0f);

        GLuint noiseTex;
        glGenTextures(1, &noiseTex);
        glBindTexture(GL_TEXTURE_2D, noiseTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noiseData.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Pass kernel to SSAO shader once at startup.
        ssaoShader.use();
        for (int i = 0; i < 64; ++i)
            ssaoShader.set("uKernel[" + std::to_string(i) + "]", ssaoKernel[i]);
        ssaoShader.set("uNoiseScale", glm::vec2(AO_W / 4.0f, AO_H / 4.0f));

        // ── Empty VAO for fullscreen draws (gl_VertexID-driven) ────
        GLuint blitVAO = 0;
        glGenVertexArrays(1, &blitVAO);

        RenderTarget rt{};
        SsaoTarget   ssaoRt{}, blurRt{};

        rt.create(BASE_W, BASE_H);
        ssaoRt.create(AO_W, AO_H);
        blurRt.create(AO_W, AO_H);

        // ── Histogram readback target (256×144 RGB8, fixed size) ──────
        GLuint histFBO = 0, histTex = 0;
        glGenFramebuffers(1, &histFBO);
        glGenTextures(1, &histTex);
        glBindTexture(GL_TEXTURE_2D, histTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 256, 144, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, histFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, histTex, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // Blur output is upsampled by the blit pass — use linear filter for smooth result.
        glBindTexture(GL_TEXTURE_2D, blurRt.tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        // ── GPU timer rings (geometry pass + SSAO/blit pass) ──────
        constexpr int GPU_QUERY_FRAMES = 3;
        GLuint gpuQueries[GPU_QUERY_FRAMES],     gpuPostQueries[GPU_QUERY_FRAMES];
        bool   queryStarted[GPU_QUERY_FRAMES]{}, postQueryStarted[GPU_QUERY_FRAMES]{};
        int    queryWrite = 0;
        glGenQueries(GPU_QUERY_FRAMES, gpuQueries);
        glGenQueries(GPU_QUERY_FRAMES, gpuPostQueries);

        FrameStats stats{};
        stats.hdriYawDeg = cfg.hdri.rotation.y;
        stats.hdriFlipV  = cfg.hdri.flipV;
        stats.skyVisible = cfg.hdri.visible;
        stats.showPanel  = true;
        int    viewMode    = 1;
        int    channelView = 0;   // 0=off 1=R 2=G 3=B
        int    preLumMode  = 1;   // viewMode saved before entering luminance (mode 15)
        bool   invertColors = false;
        struct { bool r, g, b, y, h, i; } prevKeys{};
        bool   prevLMB    = false;
        float  smoothFps  = 0.0f;
        double lastTime   = glfwGetTime();

        bool skyVisible = cfg.hdri.visible;

        glm::mat4 lastView(1.f), lastProj(1.f);
        glm::mat4 cachedInvProj(1.f);
        int       memQueryCounter = 0;
        int       lw = win.width(), lh = win.height();

        glm::mat4 sceneRot(1.0f);
        sceneRot = glm::rotate(sceneRot, glm::radians(cfg.scene.rotation.x), glm::vec3(1,0,0));
        sceneRot = glm::rotate(sceneRot, glm::radians(cfg.scene.rotation.y), glm::vec3(0,1,0));
        sceneRot = glm::rotate(sceneRot, glm::radians(cfg.scene.rotation.z), glm::vec3(0,0,1));

        // geomMat, AABB, and box VAO are constant — geomMat doesn't change at runtime.
        const glm::mat4 geomMat = sceneRot * geom.transform();
        glm::vec3 wMin(FLT_MAX), wMax(-FLT_MAX);
        {
            glm::vec3 lo = geom.boundsMin(), hi = geom.boundsMax();
            for (int cx = 0; cx <= 1; ++cx)
            for (int cy = 0; cy <= 1; ++cy)
            for (int cz = 0; cz <= 1; ++cz) {
                glm::vec3 c(cx ? hi.x : lo.x, cy ? hi.y : lo.y, cz ? hi.z : lo.z);
                glm::vec3 w = glm::vec3(geomMat * glm::vec4(c, 1.0f));
                wMin = glm::min(wMin, w);
                wMax = glm::max(wMax, w);
            }
        }
        shader.use();
        shader.set("uBoundsMin", wMin);
        shader.set("uBoundsMax", wMax);

        // ── Bounding box line geometry (12 edges × 2 vertices = 24) ───────────
        const glm::vec3 bCorners[8] = {
            {wMin.x, wMin.y, wMin.z}, {wMax.x, wMin.y, wMin.z},
            {wMax.x, wMax.y, wMin.z}, {wMin.x, wMax.y, wMin.z},
            {wMin.x, wMin.y, wMax.z}, {wMax.x, wMin.y, wMax.z},
            {wMax.x, wMax.y, wMax.z}, {wMin.x, wMax.y, wMax.z},
        };
        const int bEdges[24] = {
            0,1, 1,2, 2,3, 3,0,   // bottom face
            4,5, 5,6, 6,7, 7,4,   // top face
            0,4, 1,5, 2,6, 3,7    // vertical pillars
        };
        float boxVerts[72];
        for (int i = 0; i < 24; ++i) {
            boxVerts[i*3+0] = bCorners[bEdges[i]].x;
            boxVerts[i*3+1] = bCorners[bEdges[i]].y;
            boxVerts[i*3+2] = bCorners[bEdges[i]].z;
        }
        GLuint boxVAO = 0, boxVBO = 0;
        glGenVertexArrays(1, &boxVAO);
        glGenBuffers(1, &boxVBO);
        glBindVertexArray(boxVAO);
        glBindBuffer(GL_ARRAY_BUFFER, boxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(boxVerts), boxVerts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);

        while (!win.shouldClose()) {
            double now = glfwGetTime();
            float  dt  = static_cast<float>(now - lastTime);
            lastTime   = now;

            const glm::vec3 hdriRotRad(
                glm::radians(cfg.hdri.rotation.x),
                glm::radians(cfg.hdri.rotation.y),
                glm::radians(cfg.hdri.rotation.z));
            const glm::mat3 hdriRotMat = glm::mat3(
                glm::rotate(glm::mat4(1.f), hdriRotRad.z, glm::vec3(0,0,1)) *
                glm::rotate(glm::mat4(1.f), hdriRotRad.y, glm::vec3(0,1,0)) *
                glm::rotate(glm::mat4(1.f), hdriRotRad.x, glm::vec3(1,0,0)));

            if (glfwGetKey(win.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(win.handle(), GLFW_TRUE);

            // ── Hotkeys (edge-triggered) ───────────────────────────────
            {
                auto edge = [&](int key, bool& prev) {
                    bool now = glfwGetKey(win.handle(), key) == GLFW_PRESS;
                    bool fired = now && !prev; prev = now; return fired;
                };
                if (edge(GLFW_KEY_R, prevKeys.r)) channelView = (channelView == 1) ? 0 : 1;
                if (edge(GLFW_KEY_G, prevKeys.g)) channelView = (channelView == 2) ? 0 : 2;
                if (edge(GLFW_KEY_B, prevKeys.b)) channelView = (channelView == 3) ? 0 : 3;
                if (edge(GLFW_KEY_Y, prevKeys.y)) {
                    if (viewMode == 3) { viewMode = preLumMode; }
                    else { preLumMode = viewMode; viewMode = 3; }
                }
                if (edge(GLFW_KEY_H, prevKeys.h)) stats.showPanel = !stats.showPanel;
                if (edge(GLFW_KEY_I, prevKeys.i)) invertColors = !invertColors;
            }

            // ── LMB: sample pivot at screen centre, then orbit ────────
            // Ignore LMB when ImGui is capturing it (e.g. dragging a slider).
            bool lmbNow = !ImGui::GetIO().WantCaptureMouse &&
                          glfwGetMouseButton(win.handle(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            if (lmbNow && !prevLMB) {
                int px = BASE_W / 2;
                int py = BASE_H / 2;

                glBindFramebuffer(GL_READ_FRAMEBUFFER, rt.fbo);
                float depth = 1.0f;
                glReadPixels(px, py, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

                glm::vec3 newPivot;
                if (depth < 1.0f) {
                    float ndcX = (float(px) / BASE_W) * 2.f - 1.f;
                    float ndcY = (float(py) / BASE_H) * 2.f - 1.f;
                    float ndcZ = depth * 2.f - 1.f;
                    glm::vec4 worldPos = glm::inverse(lastProj * lastView) * glm::vec4(ndcX, ndcY, ndcZ, 1.f);
                    newPivot = glm::vec3(worldPos) / worldPos.w;
                } else {
                    newPivot = camera.position() + 3.f * camera.front();
                }
                camera.setPivot(newPivot);
                camera.beginOrbit();
                glfwSetInputMode(win.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            if (!lmbNow && prevLMB) {
                camera.endOrbit();
                glfwSetInputMode(win.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            prevLMB = lmbNow;

            camera.setAspect(win.aspectRatio());
            camera.processInput(win.handle(), dt);

            // ── GPU query read ─────────────────────────────────────
            if (queryStarted[queryWrite]) {
                GLint available = 0;
                glGetQueryObjectiv(gpuQueries[queryWrite], GL_QUERY_RESULT_AVAILABLE, &available);
                if (available) {
                    GLuint ns = 0;
                    glGetQueryObjectuiv(gpuQueries[queryWrite], GL_QUERY_RESULT, &ns);
                    stats.gpuGeomMs = static_cast<float>(ns) / 1e6f;
                }
            }
            if (postQueryStarted[queryWrite]) {
                GLint available = 0;
                glGetQueryObjectiv(gpuPostQueries[queryWrite], GL_QUERY_RESULT_AVAILABLE, &available);
                if (available) {
                    GLuint ns = 0;
                    glGetQueryObjectuiv(gpuPostQueries[queryWrite], GL_QUERY_RESULT, &ns);
                    stats.gpuPostMs = static_cast<float>(ns) / 1e6f;
                }
            }

            const glm::mat4 view = camera.viewMatrix();
            const glm::mat4 proj = camera.projectionMatrix();
            if (proj != lastProj) cachedInvProj = glm::inverse(proj);
            const glm::mat4& invProj = cachedInvProj;
            const glm::mat4  invVP   = glm::inverse(proj * view);
            lastView = view;
            lastProj = proj;

            // ── Geometry pass → G-buffer FBO ──────────────────────
            glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
            glViewport(0, 0, BASE_W, BASE_H);
            glEnable(GL_DEPTH_TEST);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glPolygonMode(GL_FRONT_AND_BACK, viewMode == 6 ? GL_LINE : GL_FILL);
            if (viewMode == 6) {
                glEnable(GL_POLYGON_OFFSET_LINE);
                glPolygonOffset(-1.0f, -1.0f);
            }

            shader.use();
            shader.set("uView",            view);
            shader.set("uProjection",      proj);
            shader.set("uViewMode",        viewMode);
            shader.set("uNear",            camera.nearPlane());
            shader.set("uFar",             camera.farPlane());
            shader.set("uHdriExposure",    cfg.hdri.exposure);
            shader.set("uHdriRotMat",      hdriRotMat);
            shader.set("uHdriFlipV",       cfg.hdri.flipV);
            shader.set("uCamPos",          camera.position());
            shader.set("uRoughness",       cfg.shading.roughness);
            shader.set("uMetallic",        cfg.shading.metallic);
            shader.set("uIOR",             cfg.shading.ior);

            const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(geomMat)));
            shader.set("uNormalMatrix", normalMatrix);

            Frustum frustum;
            frustum.update(proj * view);

            glBeginQuery(GL_TIME_ELAPSED, gpuQueries[queryWrite]);

            // Sky draw (depth test + write off — drawn at far plane, never occludes).
            if (skyVisible && viewMode == 1) {
                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                skyShader.use();
                skyShader.set("uInvVP",        invVP);
                skyShader.set("uHdriRotMat",   hdriRotMat);
                skyShader.set("uHdriExposure", cfg.hdri.exposure);
                skyShader.set("uHdriFlipV",    cfg.hdri.flipV);
                skyTex.bind(0);
                glBindVertexArray(blitVAO);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                glBindVertexArray(0);
                glDepthMask(GL_TRUE);
                glEnable(GL_DEPTH_TEST);
                shader.use();
            }

            skyTex.bind(1);  // HDRI on unit 1 for diffuse irradiance

            int drawn = 0, total = 1;
            glm::vec3 geomCentre = glm::vec3(geomMat * glm::vec4(geom.centre(), 1.0f));
            if (frustum.testSphere(geomCentre, geom.boundingRadius())) {
                geom.draw(shader, geomMat);
                ++drawn;
            }

            // Bounds AOV: draw AABB wireframe box after geometry.
            if (viewMode == 5) {
                glDepthMask(GL_FALSE);
                lineShader.use();
                lineShader.set("uVP", proj * view);
                glBindVertexArray(boxVAO);
                glDrawArrays(GL_LINES, 0, 24);
                glBindVertexArray(0);
                glDepthMask(GL_TRUE);
                shader.use();
            }

            glEndQuery(GL_TIME_ELAPSED);
            queryStarted[queryWrite] = true;
            queryWrite = (queryWrite + 1) % GPU_QUERY_FRAMES;

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDisable(GL_POLYGON_OFFSET_LINE);

            // ── SSAO pass ─────────────────────────────────────────
            glBeginQuery(GL_TIME_ELAPSED, gpuPostQueries[queryWrite]);
            glBindFramebuffer(GL_FRAMEBUFFER, ssaoRt.fbo);
            glViewport(0, 0, AO_W, AO_H);
            glDisable(GL_DEPTH_TEST);
            ssaoShader.use();
            ssaoShader.set("uProj",    proj);
            ssaoShader.set("uInvProj", invProj);
            ssaoShader.set("uRadius",  cfg.shading.ssaoRadius);
            ssaoShader.set("uBias",    cfg.shading.ssaoBias);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, rt.normalTex);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, rt.depthTex);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, noiseTex);
            glBindVertexArray(blitVAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            // ── SSAO blur pass ────────────────────────────────────
            glBindFramebuffer(GL_FRAMEBUFFER, blurRt.fbo);
            blurShader.use();
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, ssaoRt.tex);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);

            // ── Blit FBO → screen ──────────────────────────────────
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, win.width(), win.height());
            blitShader.use();
            blitShader.set("uViewMode",    viewMode);
            blitShader.set("uChannelView", channelView);
            blitShader.set("uInvert",      invertColors);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, rt.colorTex);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, blurRt.tex);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, rt.depthTex);
            glBindVertexArray(blitVAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);
            glEndQuery(GL_TIME_ELAPSED);
            postQueryStarted[queryWrite] = true;

            glEnable(GL_DEPTH_TEST);

            // ── Histogram (throttled readback every 4 frames) ─────
            {
                static int  histTick = 0;
                static uint8_t histPx[256 * 144 * 3];
                if (++histTick >= 4) {
                    histTick = 0;
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, histFBO);
                    glBlitFramebuffer(0, 0, win.width(), win.height(),
                                      0, 0, 256, 144,
                                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, histFBO);
                    glReadPixels(0, 0, 256, 144, GL_RGB, GL_UNSIGNED_BYTE, histPx);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

                    uint32_t h[3][256]{};
                    const uint8_t* p = histPx;
                    for (int i = 0; i < 256 * 144; ++i, p += 3) {
                        ++h[0][p[0]]; ++h[1][p[1]]; ++h[2][p[2]];
                    }
                    memcpy(stats.hist, h, sizeof(h));
                    stats.histValid = 1;
                }
            }

            // ── Stats ─────────────────────────────────────────────
            float rawFps = (dt > 0.0f) ? 1.0f / dt : 0.0f;
            smoothFps = (smoothFps == 0.0f) ? rawFps : smoothFps * 0.9f + rawFps * 0.1f;
            stats.fps           = rawFps;
            stats.fpsSmooth     = smoothFps;
            stats.frameTimeMs   = dt * 1000.0f;
            stats.frameCap      = FRAME_CAP;
            if (++memQueryCounter >= 60) {
                memQueryCounter = 0;
                stats.memMB = queryMemoryMB();
                glfwGetWindowSize(win.handle(), &lw, &lh);
            }
            stats.gpuAllocMB    = static_cast<float>(rt.bytes()) / (1024.0f * 1024.0f);
            stats.drawCalls     = drawn;
            stats.drawCallsTotal  = total;
            stats.drawCallsCulled = total - drawn;
            stats.totalTriangles  = geom.triangleCount();
            stats.totalVertices   = geom.vertexCount();
            stats.triPerSec     = (stats.totalTriangles * smoothFps) / 1e6f;
            stats.width         = BASE_W;
            stats.height        = BASE_H;
            stats.mpixPerSec    = (BASE_W * BASE_H * smoothFps) / 1e6f;
            stats.downsample    = downsample;
            stats.logicalWidth = lw; stats.logicalHeight = lh;
            stats.camPos          = camera.position();
            stats.camRotX         = camera.pitch();
            stats.camRotY         = camera.yaw();
            stats.camFilmbackMm   = camera.filmback();
            stats.camFocalLengthMm = camera.focalLength();
            stats.camNear         = camera.nearPlane();
            stats.camFar          = camera.farPlane();
            stats.viewMode        = viewMode;
            stats.hdriYawDeg      = cfg.hdri.rotation.y;
            stats.hdriFlipV       = cfg.hdri.flipV;
            stats.skyVisible      = skyVisible;
            stats.numObjects      = 1;
            stats.objects[0]      = {geom.name().c_str(), geom.triangleCount(), geom.vertexCount()};

            hud.beginFrame();
            hud.draw(stats);
            hud.endFrame();

            // Merge native menu one-shot actions into stats, then sync checkmarks.
            if (menuFlags.doCapture)  { stats.doCapture  = true; menuFlags.doCapture  = false; }
            if (menuFlags.doSaveJson) { stats.doSaveJson = true; menuFlags.doSaveJson = false; }
            if (menuFlags.skyVisible != skyVisible) {
                skyVisible       = menuFlags.skyVisible;
                stats.skyVisible = menuFlags.skyVisible;
            }

            viewMode            = stats.viewMode;
            cfg.hdri.rotation.y = stats.hdriYawDeg;
            cfg.hdri.flipV      = stats.hdriFlipV;
            skyVisible          = stats.skyVisible;
            camera.setFocalLength(stats.camFocalLengthMm);

            menuFlags.skyVisible = skyVisible;
            menuFlags.showPanel  = stats.showPanel;
            syncOsxMenuBar(menuFlags);

            if (stats.doCapture) {
                const char* home = getenv("HOME");
                std::string desktop = home ? std::string(home) + "/Desktop" : ".";
                std::time_t t = std::time(nullptr);
                std::string fname = desktop + "/KODAK_" + std::to_string(t) + ".png";
                std::vector<unsigned char> pixels(BASE_W * BASE_H * 3);
                glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
                glReadPixels(0, 0, BASE_W, BASE_H, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                std::vector<unsigned char> flipped(pixels.size());
                int stride = BASE_W * 3;
                for (int row = 0; row < BASE_H; ++row)
                    std::copy(pixels.begin() + row * stride,
                              pixels.begin() + (row + 1) * stride,
                              flipped.begin() + (BASE_H - 1 - row) * stride);
                stbi_write_png(fname.c_str(), BASE_W, BASE_H, 3, flipped.data(), stride);
                LOG_I("Screenshot: " + fname);
                stats.doCapture = false;
            }

            if (stats.doSaveJson) {
                cfg.camera.position    = camera.position();
                cfg.camera.yaw         = camera.yaw();
                cfg.camera.pitch       = camera.pitch();
                cfg.camera.focalLength = camera.focalLength();
                saveConfig(cfg, "profile.json");
                LOG_I("Profile saved.");
                stats.doSaveJson = false;
            }

            win.swapAndPoll();
        }

        rt.destroy();
        ssaoRt.destroy();
        blurRt.destroy();
        glDeleteFramebuffers(1, &histFBO);
        glDeleteTextures(1, &histTex);
        glDeleteTextures(1, &noiseTex);
        glDeleteVertexArrays(1, &blitVAO);
        glDeleteVertexArrays(1, &boxVAO);
        glDeleteBuffers(1, &boxVBO);
        glDeleteQueries(GPU_QUERY_FRAMES, gpuQueries);
        glDeleteQueries(GPU_QUERY_FRAMES, gpuPostQueries);

    } catch (const std::exception& e) {
        LOG_E(std::string("Fatal: ") + e.what());
        return 1;
    }
    return 0;
}
