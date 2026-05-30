#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <mach/mach.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include "window.hpp"
#include "shader.hpp"
#include "camera.hpp"
#include "mesh.hpp"
#include "texture.hpp"
#include "hud.hpp"
#include "frustum.hpp"
#include "model.hpp"

static Camera* g_camera = nullptr;

static void onMouseMove(GLFWwindow*, double xpos, double ypos) {
    if (g_camera) g_camera->processMouseMove(xpos, ypos);
}

static const char* viewModeName(int m) {
    switch (m) {
        case 1: return "Diffuse";    case 2: return "Wireframe";
        case 3: return "Depth";      case 4: return "Position";
        case 5: return "Normals";    case 6: return "UV";
        default: return "Unknown";
    }
}

static float queryMemoryMB() {
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
        return 0.0f;
    return static_cast<float>(info.phys_footprint) / (1024.0f * 1024.0f);
}

// ── Off-screen render target ───────────────────────────────────────────────
struct RenderTarget {
    GLuint fbo = 0, colorTex = 0, depthRbo = 0;
    int    w   = 0, h = 0;

    void create(int width, int height) {
        w = width; h = height;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glGenTextures(1, &colorTex);
        glBindTexture(GL_TEXTURE_2D, colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

        glGenRenderbuffers(1, &depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void destroy() {
        if (fbo)      { glDeleteFramebuffers(1, &fbo);       fbo      = 0; }
        if (colorTex) { glDeleteTextures(1, &colorTex);      colorTex = 0; }
        if (depthRbo) { glDeleteRenderbuffers(1, &depthRbo); depthRbo = 0; }
        w = h = 0;
    }

    void ensureSize(int width, int height) {
        if (w != width || h != height) { destroy(); create(width, height); }
    }

    // Tracked allocation: RGB8 colour (3B) + DEPTH24 in a 4B/texel renderbuffer.
    size_t bytes() const { return static_cast<size_t>(w) * h * (3 + 4); }
};

int main() {
    try {
        constexpr int BASE_W = 2048, BASE_H = 1152;
        constexpr int RENDER_SCALE = 2;   // window = BASE/SCALE logical pixels
        constexpr int FRAME_CAP    = 0;   // 0 = vsync/unlimited; >0 caps the loop (testing aid)

        Window win(BASE_W / RENDER_SCALE, BASE_H / RENDER_SCALE, "Renderer");
        glfwSetInputMode(win.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursorPosCallback(win.handle(), onMouseMove);
        // Reset camera mouse state on resize to prevent jump
        glfwSetWindowSizeCallback(win.handle(), [](GLFWwindow*, int, int) {
            if (g_camera) g_camera->resetMouse();
        });

        Shader shader("shaders/basic.vert", "shaders/basic.frag");
        shader.use();
        shader.set("uAlbedo", 0);

        Shader blitShader("shaders/blit.vert", "shaders/blit.frag");
        blitShader.use();
        blitShader.set("uFrame", 0);

        Camera camera({0.0f, 1.0f, 10.0f}, win.aspectRatio());
        g_camera = &camera;

        HUD hud(win.handle());

        Model rock = Model::loadGLTF("assets/geo/rock_shopk_gltf_high/Rock_shopk_High.gltf");

        // Empty VAO for fullscreen blit (no VBO — blit.vert uses gl_VertexID)
        GLuint blitVAO = 0;
        glGenVertexArrays(1, &blitVAO);

        RenderTarget rt{};

        // GPU timer: ring of queries read N frames later, so the CPU never stalls
        // waiting on the GPU (GL_QUERY_RESULT blocks; the ring lets us defer the read).
        constexpr int GPU_QUERY_FRAMES = 3;
        GLuint gpuQueries[GPU_QUERY_FRAMES];
        bool   queryStarted[GPU_QUERY_FRAMES] = {};
        int    queryWrite = 0;
        glGenQueries(GPU_QUERY_FRAMES, gpuQueries);

        FrameStats stats{};
        int    viewMode  = 1;
        bool   prevKeys[6] = {};
        float  smoothFps = 0.0f;
        double lastTime  = glfwGetTime();

        while (!win.shouldClose()) {
            double now = glfwGetTime();
            float  dt  = static_cast<float>(now - lastTime);
            lastTime   = now;

            if (glfwGetKey(win.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(win.handle(), GLFW_TRUE);

            // ── View mode keys 1–6 ────────────────────────────────
            static const int viewKeys[6] = {
                GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5, GLFW_KEY_6
            };
            for (int i = 0; i < 6; ++i) {
                bool down = glfwGetKey(win.handle(), viewKeys[i]) == GLFW_PRESS;
                if (down && !prevKeys[i]) viewMode = i + 1;
                prevKeys[i] = down;
            }

            camera.setAspect(win.aspectRatio());
            camera.processInput(win.handle(), dt);

            // ── Ensure FBO matches physical framebuffer (1:1) ────
            rt.ensureSize(win.width(), win.height());

            // ── Read the query we're about to reuse (written GPU_QUERY_FRAMES ago,
            //    so its result is ready — no CPU stall). ─────────────
            if (queryStarted[queryWrite]) {
                GLint available = 0;
                glGetQueryObjectiv(gpuQueries[queryWrite], GL_QUERY_RESULT_AVAILABLE, &available);
                if (available) {
                    GLuint ns = 0;
                    glGetQueryObjectuiv(gpuQueries[queryWrite], GL_QUERY_RESULT, &ns);
                    stats.gpuTimeMs = static_cast<float>(ns) / 1e6f;
                }
            }

            // ── Render scene → off-screen FBO ──────────────────────
            glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
            glViewport(0, 0, win.width(), win.height());
            glEnable(GL_DEPTH_TEST);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glPolygonMode(GL_FRONT_AND_BACK, viewMode == 2 ? GL_LINE : GL_FILL);

            // Cache matrices once — reused for uniforms, frustum, and stats.
            const glm::mat4 view = camera.viewMatrix();
            const glm::mat4 proj = camera.projectionMatrix();

            // Frame-constant uniforms: set once, not per draw call (C1).
            shader.use();
            shader.set("uView",       view);
            shader.set("uProjection", proj);
            shader.set("uViewMode",   viewMode);
            shader.set("uNear",       camera.nearPlane());
            shader.set("uFar",        camera.farPlane());

            // Scene: rock model only.
            const glm::mat4 mRock = rock.transform();

            Frustum frustum;
            frustum.update(proj * view);

            glBeginQuery(GL_TIME_ELAPSED, gpuQueries[queryWrite]);

            int drawn = 0, total = 1;

            glm::vec3 rockCentre = glm::vec3(mRock * glm::vec4(rock.centre(), 1.0f));
            if (frustum.testSphere(rockCentre, rock.boundingRadius())) {
                rock.draw(shader, mRock);
                ++drawn;
            }

            glEndQuery(GL_TIME_ELAPSED);
            queryStarted[queryWrite] = true;
            queryWrite = (queryWrite + 1) % GPU_QUERY_FRAMES;

            // ── Blit FBO → screen ──────────────────────────────────
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, win.width(), win.height());
            glDisable(GL_DEPTH_TEST);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            blitShader.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, rt.colorTex);
            glBindVertexArray(blitVAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);

            // ── Collect stats ──────────────────────────────────────
            float rawFps = (dt > 0.0f) ? 1.0f / dt : 0.0f;
            smoothFps    = (smoothFps == 0.0f) ? rawFps : smoothFps * 0.9f + rawFps * 0.1f;
            stats.fps         = rawFps;
            stats.fpsSmooth   = smoothFps;
            stats.frameTimeMs = dt * 1000.0f;
            stats.frameCap       = FRAME_CAP;
            stats.memMB          = queryMemoryMB();
            stats.gpuAllocMB     = static_cast<float>(rt.bytes()) / (1024.0f * 1024.0f);
            stats.drawCalls      = drawn;
            stats.drawCallsTotal = total;
            stats.drawCallsCulled = total - drawn;
            stats.totalTriangles  = rock.triangleCount();
            stats.totalVertices   = rock.vertexCount();
            stats.width          = win.width();
            stats.height         = win.height();
            stats.renderScale    = RENDER_SCALE;
            { int lw, lh; glfwGetWindowSize(win.handle(), &lw, &lh);
              stats.logicalWidth = lw; stats.logicalHeight = lh; }
            stats.camPos             = camera.position();
            stats.camRotX            = camera.pitch();
            stats.camRotY            = camera.yaw();
            stats.camRotZ            = 0.0f;
            stats.camFilmbackMm      = camera.filmback();
            stats.camFocalLengthMm   = camera.focalLength();
            stats.camNear            = camera.nearPlane();
            stats.camFar             = camera.farPlane();
            stats.viewMode           = viewMode;
            stats.viewModeName       = ::viewModeName(viewMode);
            stats.numObjects = 1;
            stats.objects[0] = {"rock", rock.triangleCount(), rock.vertexCount()};

            // ── HUD overlay ────────────────────────────────────────
            hud.beginFrame();
            hud.draw(stats);
            hud.endFrame();

            // ── Optional frame cap (testing aid; inert when 0) ─────
            if (FRAME_CAP > 0) {
                double target  = 1.0 / FRAME_CAP;
                double elapsed = glfwGetTime() - now;
                if (elapsed < target)
                    std::this_thread::sleep_for(std::chrono::duration<double>(target - elapsed));
            }

            win.swapAndPoll();
        }

        rt.destroy();
        glDeleteVertexArrays(1, &blitVAO);
        glDeleteQueries(GPU_QUERY_FRAMES, gpuQueries);

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
