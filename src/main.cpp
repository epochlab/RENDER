#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <mach/mach.h>
#include <stb_image_write.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <random>
#include <string>
#include <vector>
#include <ctime>
#include <sys/stat.h>
#include "window.hpp"
#include "shader.hpp"
#include "camera.hpp"
#include "texture.hpp"
#include "hud.hpp"
#include "frustum.hpp"
#include "model.hpp"
#include "config.hpp"

static Camera* g_camera      = nullptr;
static bool    g_mouseEnabled = false;

static void onMouseMove(GLFWwindow*, double xpos, double ypos) {
    if (g_mouseEnabled && g_camera)
        g_camera->processMouseMove(xpos, ypos);
}

static const char* viewModeName(int m) {
    switch (m) {
        case 1:  return "Beauty";        case 2:  return "Wireframe";
        case 3:  return "Alpha";         case 4:  return "Depth";
        case 5:  return "Position";      case 6:  return "Normals";
        case 7:  return "UV";            case 8:  return "Albedo";
        case 9:  return "Direct Diffuse"; case 10: return "AO";
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

    void ensureSize(int width, int height) {
        if (w != width || h != height) { destroy(); create(width, height); }
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
        constexpr int BASE_W    = 2048, BASE_H = 1152;
        constexpr int FRAME_CAP = 0;  // 0 = vsync/unlimited

        AppConfig cfg = loadConfig("profile.json");
        const int renderScale = cfg.render.scale > 0 ? cfg.render.scale : 2;

        Window win(BASE_W / renderScale, BASE_H / renderScale, "BOUNCE");
        glfwSetCursorPosCallback(win.handle(), onMouseMove);
        glfwSetWindowSizeCallback(win.handle(), [](GLFWwindow*, int, int) {
            if (g_camera) g_camera->resetMouse();
        });

        // ── Shaders ────────────────────────────────────────────────
        Shader shader("shaders/basic.vert", "shaders/basic.frag");
        shader.use();
        shader.set("uAlbedo",     0);
        shader.set("uSkyHDR",     1);
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
        blurShader.set("uSSAO", 0);

        // ── Camera ─────────────────────────────────────────────────
        Camera camera(cfg.camera.position, win.aspectRatio(),
                      cfg.camera.filmback, cfg.camera.focalLength,
                      cfg.camera.near,     cfg.camera.far);
        camera.setYaw(cfg.camera.yaw);
        camera.setPitch(cfg.camera.pitch);
        g_camera = &camera;

        HUD hud(win.handle());

        Texture skyTex(cfg.hdri.path);
        Model   rock = Model::loadGLTF(cfg.scene.geometry);

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
        ssaoShader.set("uNoiseScale", glm::vec2(BASE_W / 4.0f, BASE_H / 4.0f));

        // ── Empty VAO for fullscreen draws (gl_VertexID-driven) ────
        GLuint blitVAO = 0;
        glGenVertexArrays(1, &blitVAO);

        RenderTarget rt{};
        SsaoTarget   ssaoRt{}, blurRt{};

        rt.create(BASE_W, BASE_H);
        ssaoRt.create(BASE_W, BASE_H);
        blurRt.create(BASE_W, BASE_H);

        // ── GPU timer ring ─────────────────────────────────────────
        constexpr int GPU_QUERY_FRAMES = 3;
        GLuint gpuQueries[GPU_QUERY_FRAMES];
        bool   queryStarted[GPU_QUERY_FRAMES] = {};
        int    queryWrite = 0;
        glGenQueries(GPU_QUERY_FRAMES, gpuQueries);

        FrameStats stats{};
        int    viewMode    = 1;
        bool   prevKeys[10] = {};
        bool   prevSpace  = false;
        bool   prevH      = false;
        bool   prevK      = false;
        bool   prevJ      = false;
        bool   prevB      = false;
        bool   showHUD    = true;
        float  smoothFps  = 0.0f;
        double lastTime   = glfwGetTime();

        const glm::vec3 hdriRotRad(
            glm::radians(cfg.hdri.rotation.x),
            glm::radians(cfg.hdri.rotation.y),
            glm::radians(cfg.hdri.rotation.z));

        bool skyVisible = cfg.hdri.visible;

        glm::mat4 sceneRot(1.0f);
        sceneRot = glm::rotate(sceneRot, glm::radians(cfg.scene.rotation.x), glm::vec3(1,0,0));
        sceneRot = glm::rotate(sceneRot, glm::radians(cfg.scene.rotation.y), glm::vec3(0,1,0));
        sceneRot = glm::rotate(sceneRot, glm::radians(cfg.scene.rotation.z), glm::vec3(0,0,1));

        while (!win.shouldClose()) {
            double now = glfwGetTime();
            float  dt  = static_cast<float>(now - lastTime);
            lastTime   = now;

            if (glfwGetKey(win.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(win.handle(), GLFW_TRUE);

            // ── Space: hold to enable mouse look ──────────────────
            bool spaceNow = glfwGetKey(win.handle(), GLFW_KEY_SPACE) == GLFW_PRESS;
            if (spaceNow && !prevSpace) {
                g_mouseEnabled = true;
                glfwSetInputMode(win.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                camera.resetMouse();
            }
            if (!spaceNow && prevSpace) {
                g_mouseEnabled = false;
                glfwSetInputMode(win.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            prevSpace = spaceNow;

            // ── H: toggle HUD ─────────────────────────────────────
            bool hNow = glfwGetKey(win.handle(), GLFW_KEY_H) == GLFW_PRESS;
            if (hNow && !prevH) showHUD = !showHUD;
            prevH = hNow;

            // ── K: screenshot ─────────────────────────────────────
            bool kNow = glfwGetKey(win.handle(), GLFW_KEY_K) == GLFW_PRESS;
            if (kNow && !prevK) {
                mkdir("screenshots", 0755);
                std::time_t t = std::time(nullptr);
                std::string fname = "screenshots/BOUNCE_" + std::to_string(t) + ".png";
                std::vector<unsigned char> pixels(BASE_W * BASE_H * 3);
                glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
                glReadPixels(0, 0, BASE_W, BASE_H, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                // Flip vertically: OpenGL origin is bottom-left.
                std::vector<unsigned char> flipped(pixels.size());
                int stride = BASE_W * 3;
                for (int row = 0; row < BASE_H; ++row)
                    std::copy(pixels.begin() + row * stride,
                              pixels.begin() + (row + 1) * stride,
                              flipped.begin() + (BASE_H - 1 - row) * stride);
                stbi_write_png(fname.c_str(), BASE_W, BASE_H, 3, flipped.data(), stride);
                std::cout << "Screenshot: " << fname << '\n';
            }
            prevK = kNow;

            // ── J: save camera pos/rot as profile default ─────────
            bool jNow = glfwGetKey(win.handle(), GLFW_KEY_J) == GLFW_PRESS;
            if (jNow && !prevJ) {
                cfg.camera.position = camera.position();
                cfg.camera.yaw      = camera.yaw();
                cfg.camera.pitch    = camera.pitch();
                saveConfig(cfg, "profile.json");
                std::cout << "Profile saved: pos("
                    << cfg.camera.position.x << ", "
                    << cfg.camera.position.y << ", "
                    << cfg.camera.position.z << ") yaw="
                    << cfg.camera.yaw << " pitch=" << cfg.camera.pitch << '\n';
            }
            prevJ = jNow;

            // ── B: toggle sky background ───────────────────────────
            bool bNow = glfwGetKey(win.handle(), GLFW_KEY_B) == GLFW_PRESS;
            if (bNow && !prevB) skyVisible = !skyVisible;
            prevB = bNow;

            // ── View mode keys 1–9 + 0 (mode 10 = AO) ───────────
            static const int viewKeys[10] = {
                GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5,
                GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8, GLFW_KEY_9, GLFW_KEY_0
            };
            for (int i = 0; i < 10; ++i) {
                bool down = glfwGetKey(win.handle(), viewKeys[i]) == GLFW_PRESS;
                if (down && !prevKeys[i]) viewMode = i + 1;
                prevKeys[i] = down;
            }

            camera.setAspect(win.aspectRatio());
            camera.processInput(win.handle(), dt);

            // ── GPU query read ─────────────────────────────────────
            if (queryStarted[queryWrite]) {
                GLint available = 0;
                glGetQueryObjectiv(gpuQueries[queryWrite], GL_QUERY_RESULT_AVAILABLE, &available);
                if (available) {
                    GLuint ns = 0;
                    glGetQueryObjectuiv(gpuQueries[queryWrite], GL_QUERY_RESULT, &ns);
                    stats.gpuTimeMs = static_cast<float>(ns) / 1e6f;
                }
            }

            // Cache matrices.
            const glm::mat4 view = camera.viewMatrix();
            const glm::mat4 proj = camera.projectionMatrix();
            const glm::mat4 invProj = glm::inverse(proj);

            // ── Geometry pass → G-buffer FBO ──────────────────────
            glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
            glViewport(0, 0, BASE_W, BASE_H);
            glEnable(GL_DEPTH_TEST);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glPolygonMode(GL_FRONT_AND_BACK, viewMode == 2 ? GL_LINE : GL_FILL);

            shader.use();
            shader.set("uView",            view);
            shader.set("uProjection",      proj);
            shader.set("uViewMode",        viewMode);
            shader.set("uNear",            camera.nearPlane());
            shader.set("uFar",             camera.farPlane());
            shader.set("uHdriExposure",    cfg.hdri.exposure);
            shader.set("uHdriRot",         hdriRotRad);

            const glm::mat4 mRock = sceneRot * rock.transform();
            Frustum frustum;
            frustum.update(proj * view);

            glBeginQuery(GL_TIME_ELAPSED, gpuQueries[queryWrite]);

            // Sky draw (depth test + write off — drawn at far plane, never occludes).
            if (skyVisible && viewMode == 1) {
                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                skyShader.use();
                skyShader.set("uInvVP",       glm::inverse(proj * view));
                skyShader.set("uHdriRot",     hdriRotRad);
                skyShader.set("uHdriExposure", cfg.hdri.exposure);
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
            glm::vec3 modelCentre = glm::vec3(mRock * glm::vec4(rock.centre(), 1.0f));
            if (frustum.testSphere(modelCentre, rock.boundingRadius())) {
                rock.draw(shader, mRock);
                ++drawn;
            }

            glEndQuery(GL_TIME_ELAPSED);
            queryStarted[queryWrite] = true;
            queryWrite = (queryWrite + 1) % GPU_QUERY_FRAMES;

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            // ── SSAO pass ─────────────────────────────────────────
            glBindFramebuffer(GL_FRAMEBUFFER, ssaoRt.fbo);
            glViewport(0, 0, BASE_W, BASE_H);
            glDisable(GL_DEPTH_TEST);
            ssaoShader.use();
            ssaoShader.set("uProj",    proj);
            ssaoShader.set("uInvProj", invProj);
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
            blitShader.set("uViewMode", viewMode);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, rt.colorTex);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, blurRt.tex);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, rt.depthTex);
            glBindVertexArray(blitVAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);

            // ── Stats ─────────────────────────────────────────────
            float rawFps = (dt > 0.0f) ? 1.0f / dt : 0.0f;
            smoothFps = (smoothFps == 0.0f) ? rawFps : smoothFps * 0.9f + rawFps * 0.1f;
            stats.fps           = rawFps;
            stats.fpsSmooth     = smoothFps;
            stats.frameTimeMs   = dt * 1000.0f;
            stats.frameCap      = FRAME_CAP;
            stats.memMB         = queryMemoryMB();
            stats.gpuAllocMB    = static_cast<float>(rt.bytes()) / (1024.0f * 1024.0f);
            stats.drawCalls     = drawn;
            stats.drawCallsTotal= total;
            stats.drawCallsCulled = total - drawn;
            stats.totalTriangles  = rock.triangleCount();
            stats.totalVertices   = rock.vertexCount();
            stats.width         = BASE_W;
            stats.height        = BASE_H;
            stats.renderScale   = renderScale;
            { int lw, lh; glfwGetWindowSize(win.handle(), &lw, &lh);
              stats.logicalWidth = lw; stats.logicalHeight = lh; }
            stats.camPos          = camera.position();
            stats.camRotX         = camera.pitch();
            stats.camRotY         = camera.yaw();
            stats.camRotZ         = 0.0f;
            stats.camFilmbackMm   = camera.filmback();
            stats.camFocalLengthMm= camera.focalLength();
            stats.camNear         = camera.nearPlane();
            stats.camFar          = camera.farPlane();
            stats.viewMode        = viewMode;
            stats.viewModeName    = ::viewModeName(viewMode);
            stats.numObjects      = 1;
            stats.objects[0]      = {rock.name().c_str(), rock.triangleCount(), rock.vertexCount()};

            if (showHUD) {
                hud.beginFrame();
                hud.draw(stats);
                hud.endFrame();
            }

            if (FRAME_CAP > 0) {
                double target  = 1.0 / FRAME_CAP;
                double elapsed = glfwGetTime() - now;
                if (elapsed < target)
                    std::this_thread::sleep_for(std::chrono::duration<double>(target - elapsed));
            }

            win.swapAndPoll();
        }

        rt.destroy();
        ssaoRt.destroy();
        blurRt.destroy();
        glDeleteTextures(1, &noiseTex);
        glDeleteVertexArrays(1, &blitVAO);
        glDeleteQueries(GPU_QUERY_FRAMES, gpuQueries);

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
