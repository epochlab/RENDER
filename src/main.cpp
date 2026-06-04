#include <GLFW/glfw3.h>
#include <imgui.h>
#include "menu_osx.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <mach/mach.h>
#include "exr_io.hpp"
#include "color_pipeline.hpp"
#include <stb_image_write.h>
#include <stdexcept>
#include <random>
#include <string>
#include <vector>
#include <cfloat>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <fstream>
#include <numeric>
#include <string_view>
#include "window.hpp"
#include "shader.hpp"
#include "camera.hpp"
#include "texture.hpp"
#include "hud.hpp"
#include "frustum.hpp"
#include "model.hpp"
#include "config.hpp"
#include "log.hpp"
#include <nlohmann/json.hpp>
#include "ssao_kernel.hpp"

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

// ── IBL precomputation baker ──────────────────────────────────────────────────
struct IblBaker {
    GLuint irradianceTex  = 0;   // GL_RGB16F 128×64
    GLuint prefilteredTex = 0;   // GL_RGB16F 512×256, 5 mip levels
    GLuint brdfLUT        = 0;   // GL_RG16F  512×512
    GLuint fbo            = 0;
    std::unique_ptr<Shader> irrShader;
    std::unique_ptr<Shader> prefilterShader;
    std::unique_ptr<Shader> brdfShader;
    bool brdfReady = false;

    static GLuint makeFloatTex(int w, int h, GLenum internalFmt, bool mipmaps) {
        GLenum baseFmt = (internalFmt == GL_RG16F) ? GL_RG : GL_RGB;
        GLuint id;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, baseFmt, GL_FLOAT, nullptr);
        if (mipmaps) {
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        return id;
    }

    void create() {
        irrShader       = std::make_unique<Shader>("shaders/post/blit.vert", "shaders/bake/irradiance.frag");
        prefilterShader = std::make_unique<Shader>("shaders/post/blit.vert", "shaders/bake/prefilter.frag");
        brdfShader      = std::make_unique<Shader>("shaders/post/blit.vert", "shaders/bake/brdf_lut.frag");
        irradianceTex  = makeFloatTex(128, 64,  GL_RGB16F, false);
        prefilteredTex = makeFloatTex(512, 256, GL_RGB16F, true);
        brdfLUT        = makeFloatTex(512, 512, GL_RG16F,  false);
        glGenFramebuffers(1, &fbo);
    }

    void bake(GLuint hdriTexId, const glm::mat3& rot, float exposure, bool flipV, int samples) {
        GLuint bakVAO = 0;
        glGenVertexArrays(1, &bakVAO);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glDisable(GL_DEPTH_TEST);

        // ── BRDF LUT (view-independent, baked once) ───────────────────
        if (!brdfReady) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUT, 0);
            glViewport(0, 0, 512, 512);
            brdfShader->use();
            brdfShader->set("uSamples", samples);
            glBindVertexArray(bakVAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            brdfReady = true;
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdriTexId);

        // ── Irradiance map ────────────────────────────────────────────
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, irradianceTex, 0);
        glViewport(0, 0, 128, 64);
        irrShader->use();
        irrShader->set("uHdriTex",      0);
        irrShader->set("uHdriRotMat",   rot);
        irrShader->set("uHdriExposure", exposure);
        irrShader->set("uHdriFlipV",    flipV);
        irrShader->set("uSamples",      samples);
        glBindVertexArray(bakVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // ── Prefiltered specular (5 mip levels) ───────────────────────
        prefilterShader->use();
        prefilterShader->set("uHdriTex",      0);
        prefilterShader->set("uHdriRotMat",   rot);
        prefilterShader->set("uHdriExposure", exposure);
        prefilterShader->set("uHdriFlipV",    flipV);
        prefilterShader->set("uSamples",      samples);
        for (int mip = 0; mip < 5; ++mip) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, prefilteredTex, mip);
            glViewport(0, 0, 512 >> mip, 256 >> mip);
            prefilterShader->set("uRoughness", static_cast<float>(mip) / 4.0f);
            glBindVertexArray(bakVAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteVertexArrays(1, &bakVAO);
        glFinish();
    }

    void destroy() {
        irrShader.reset();
        prefilterShader.reset();
        brdfShader.reset();
        if (fbo)            { glDeleteFramebuffers(1, &fbo);        fbo           = 0; }
        if (irradianceTex)  { glDeleteTextures(1, &irradianceTex);  irradianceTex = 0; }
        if (prefilteredTex) { glDeleteTextures(1, &prefilteredTex); prefilteredTex = 0; }
        if (brdfLUT)        { glDeleteTextures(1, &brdfLUT);        brdfLUT       = 0; }
    }
};

int main(int argc, char** argv) {
    int         benchmarkN   = 0;
    std::string benchmarkOut;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--benchmark" && i + 1 < argc) {
            char* end = nullptr;
            long v = std::strtol(argv[++i], &end, 10);
            if (end == argv[i] || *end != '\0' || v <= 0)
                { fprintf(stderr, "Usage: --benchmark <N> [--output <file>]\n"); return 1; }
            benchmarkN = static_cast<int>(v);
        } else if (arg == "--output" && i + 1 < argc) {
            benchmarkOut = argv[++i];
        }
    }

    try {
        constexpr int FRAME_CAP = 0;  // 0 = vsync/unlimited

        AppConfig cfg = loadConfig("config/profile.json", "config/scene.json");
        const int BASE_W      = cfg.render.width  > 0 ? cfg.render.width  : 2048;
        const int BASE_H      = cfg.render.height > 0 ? cfg.render.height : 1152;
        const int downsample  = cfg.render.downsample > 0 ? cfg.render.downsample : 2;
        const int AO_W        = cfg.shading.ssaoHalfRes ? BASE_W / 2 : BASE_W;
        const int AO_H        = cfg.shading.ssaoHalfRes ? BASE_H / 2 : BASE_H;

        Window win(BASE_W / downsample, BASE_H / downsample, "KODAK");
        glfwSetCursorPosCallback(win.handle(), onMouseMove);
        glfwSetWindowSizeCallback(win.handle(), [](GLFWwindow*, int, int) {
            if (g_camera) g_camera->resetMouse();
        });

        OsxMenuFlags menuFlags;
        initOsxMenuBar(&menuFlags);

        // ── Shaders ────────────────────────────────────────────────
        Shader shader("shaders/geometry/pbr.vert", "shaders/geometry/pbr.frag");
        shader.use();
        shader.set("uAlbedo",        0);
        shader.set("uNormalMap",     2);
        shader.set("uIrradianceTex", 3);
        shader.set("uPrefilteredTex",4);
        shader.set("uBrdfLUT",       5);
        shader.set("uMaxMipLevel",   4.0f);

        Shader blitShader("shaders/post/blit.vert", "shaders/post/blit.frag");
        blitShader.use();
        blitShader.set("uFrame",    0);
        blitShader.set("uAO",       1);
        blitShader.set("uDepth",    2);
        blitShader.set("uColorLUT", 3);

        Shader skyShader("shaders/sky/sky.vert", "shaders/sky/sky.frag");
        skyShader.use();
        skyShader.set("uSkyHDR", 0);

        Shader ssaoShader("shaders/post/ssao.vert", "shaders/post/ssao.frag");
        ssaoShader.use();
        ssaoShader.set("gNormal",   0);
        ssaoShader.set("gDepth",    1);
        ssaoShader.set("uNoiseTex", 2);

        Shader blurShader("shaders/post/ssao.vert", "shaders/post/ssao_blur.frag");
        blurShader.use();
        blurShader.set("uSSAO",       0);
        blurShader.set("uBlurRadius", cfg.shading.ssaoBlurRadius);

        Shader blurVShader("shaders/post/ssao.vert", "shaders/post/ssao_blur_v.frag");
        blurVShader.use();
        blurVShader.set("uSSAO",       0);
        blurVShader.set("uBlurRadius", cfg.shading.ssaoBlurRadius);

        Shader dofShader("shaders/post/blit.vert", "shaders/post/dof.frag");
        dofShader.use();
        dofShader.set("uFrame", 0);
        dofShader.set("uDepth", 1);

        Shader lineShader("shaders/debug/bounds.vert", "shaders/debug/bounds.frag");

        // ── Pre-cached per-frame uniform locations ─────────────────
        const GLint pbrLocView          = shader.uniformLoc("uView");
        const GLint pbrLocProjection    = shader.uniformLoc("uProjection");
        const GLint pbrLocViewMode      = shader.uniformLoc("uViewMode");
        const GLint pbrLocNear          = shader.uniformLoc("uNear");
        const GLint pbrLocFar           = shader.uniformLoc("uFar");
        const GLint pbrLocCamPos        = shader.uniformLoc("uCamPos");
        const GLint pbrLocRoughness     = shader.uniformLoc("uRoughness");
        const GLint pbrLocMetallic      = shader.uniformLoc("uMetallic");
        const GLint pbrLocIOR           = shader.uniformLoc("uIOR");
        const GLint pbrLocNormalMatrix  = shader.uniformLoc("uNormalMatrix");
        const GLint pbrLocHdriRotMat     = shader.uniformLoc("uHdriRotMat");
        const GLint pbrLocHdriIntensity  = shader.uniformLoc("uHdriIntensity");

        const GLint skyLocInvVP         = skyShader.uniformLoc("uInvVP");
        const GLint skyLocHdriRotMat    = skyShader.uniformLoc("uHdriRotMat");
        const GLint skyLocHdriExposure  = skyShader.uniformLoc("uHdriExposure");
        const GLint skyLocHdriFlipV     = skyShader.uniformLoc("uHdriFlipV");

        const GLint ssaoLocProj         = ssaoShader.uniformLoc("uProj");
        const GLint ssaoLocInvProj      = ssaoShader.uniformLoc("uInvProj");
        const GLint ssaoLocRadius       = ssaoShader.uniformLoc("uRadius");
        const GLint ssaoLocBias         = ssaoShader.uniformLoc("uBias");

        const GLint blitLocViewMode      = blitShader.uniformLoc("uViewMode");
        const GLint blitLocChannelView   = blitShader.uniformLoc("uChannelView");
        const GLint blitLocInvert        = blitShader.uniformLoc("uInvert");
        const GLint blitLocExposure      = blitShader.uniformLoc("uExposure");
        const GLint blitLocAspectEnabled = blitShader.uniformLoc("uAspectEnabled");
        const GLint blitLocAspectRatio   = blitShader.uniformLoc("uAspectRatio");
        const GLint blitLocLutEnabled    = blitShader.uniformLoc("uLutEnabled");

        const GLint dofLocNear          = dofShader.uniformLoc("uNear");
        const GLint dofLocFar           = dofShader.uniformLoc("uFar");
        const GLint dofLocFocusDist     = dofShader.uniformLoc("uFocusDist");
        const GLint dofLocCocScale      = dofShader.uniformLoc("uCocScale");
        const GLint dofLocSamples       = dofShader.uniformLoc("uDofSamples");

        const GLint lineLocVP           = lineShader.uniformLoc("uVP");

        // ── Camera ─────────────────────────────────────────────────
        Camera camera(cfg.camera.position, win.aspectRatio(),
                      cfg.camera.filmback, cfg.camera.focalLength,
                      cfg.camera.near,     cfg.camera.far);
        camera.setYaw(cfg.camera.yaw);
        camera.setPitch(cfg.camera.pitch);
        camera.setISO(cfg.camera.iso);
        camera.setFStop(cfg.camera.fStop);
        camera.setShutterSpeed(cfg.camera.shutterSpeed);
        camera.setFocusDist(cfg.camera.focusDist);
        g_camera = &camera;

        HUD hud(win.handle());

        ColorPipeline colorPipeline;
        colorPipeline.bake(static_cast<ViewLut>(cfg.color.viewLut));

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
              std::to_string(geom.indexCount()) + " indices");
        LOG_I("HDRI: " + cfg.hdri.path);

        // ── SSAO kernel (deterministic, seed 42) ───────────────────
        std::vector<glm::vec3> ssaoKernel = generateSSAOKernel(42, cfg.shading.ssaoSamples);

        // ── SSAO noise texture (4×4, GL_REPEAT) — seed 43 ─────────
        std::mt19937 rng(43);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
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

        // Pack vec3 kernel into vec4 (std140 pads vec3 → vec4).
        std::vector<glm::vec4> kernelPadded(64, glm::vec4(0.f));
        for (int i = 0; i < cfg.shading.ssaoSamples; ++i)
            kernelPadded[i] = glm::vec4(ssaoKernel[i], 0.f);

        GLuint ssaoKernelUBO;
        glGenBuffers(1, &ssaoKernelUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, ssaoKernelUBO);
        glBufferData(GL_UNIFORM_BUFFER, 64 * sizeof(glm::vec4), kernelPadded.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, ssaoKernelUBO);
        ssaoShader.bindUniformBlock("KernelBlock", 0);

        ssaoShader.use();
        ssaoShader.set("uKernelSize", cfg.shading.ssaoSamples);
        ssaoShader.set("uNoiseScale", glm::vec2(AO_W / 4.0f, AO_H / 4.0f));

        // ── Empty VAO for fullscreen draws (gl_VertexID-driven) ────
        GLuint blitVAO = 0;
        glGenVertexArrays(1, &blitVAO);

        RenderTarget rt{};
        SsaoTarget   ssaoRt{}, blurRt{}, blurTmpRt{};

        rt.create(BASE_W, BASE_H);
        ssaoRt.create(AO_W, AO_H);
        blurRt.create(AO_W, AO_H);
        blurTmpRt.create(AO_W, AO_H);

        IblBaker baker;
        baker.create();

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

        // ── DoF render target (RGB16F, full res) ──────────────────
        GLuint dofTex = 0, dofFbo = 0;
        glGenTextures(1, &dofTex);
        glBindTexture(GL_TEXTURE_2D, dofTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, BASE_W, BASE_H, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        glGenFramebuffers(1, &dofFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, dofFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dofTex, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ── Async histogram readback PBOs (double-buffered) ───────
        GLuint histPBOs[2] = {};
        glGenBuffers(2, histPBOs);
        for (int i = 0; i < 2; ++i) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, histPBOs[i]);
            glBufferData(GL_PIXEL_PACK_BUFFER, 256 * 144 * 3, nullptr, GL_STREAM_READ);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        // Blur output is upsampled by the blit pass — use linear filter for smooth result.
        glBindTexture(GL_TEXTURE_2D, blurRt.tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        // ── GPU timer rings (geometry, SSAO, blur, composite) ─────
        constexpr int GPU_QUERY_FRAMES = 3;
        GLuint gpuQueries[GPU_QUERY_FRAMES],     gpuPostQueries[GPU_QUERY_FRAMES];
        GLuint gpuSsaoQueries[GPU_QUERY_FRAMES], gpuBlurQueries[GPU_QUERY_FRAMES], gpuBlurVQueries[GPU_QUERY_FRAMES];
        bool   queryStarted[GPU_QUERY_FRAMES]{},     postQueryStarted[GPU_QUERY_FRAMES]{};
        bool   ssaoQueryStarted[GPU_QUERY_FRAMES]{}, blurQueryStarted[GPU_QUERY_FRAMES]{}, blurVQueryStarted[GPU_QUERY_FRAMES]{};
        int    queryWrite = 0;
        glGenQueries(GPU_QUERY_FRAMES, gpuQueries);
        glGenQueries(GPU_QUERY_FRAMES, gpuPostQueries);
        glGenQueries(GPU_QUERY_FRAMES, gpuSsaoQueries);
        glGenQueries(GPU_QUERY_FRAMES, gpuBlurQueries);
        glGenQueries(GPU_QUERY_FRAMES, gpuBlurVQueries);

        FrameStats stats{};
        stats.hdriYawDeg       = cfg.hdri.rotation.y;
        stats.hdriFlipV        = cfg.hdri.flipV;
        stats.hdriIntensity    = cfg.hdri.intensity;
        stats.hdriEvOffset     = cfg.hdri.exposure;
        stats.skyVisible       = cfg.hdri.visible;
        stats.showPanel        = true;
        stats.viewLut          = cfg.color.viewLut;
        stats.camISO           = cfg.camera.iso;
        stats.camFStop         = cfg.camera.fStop;
        stats.camShutterSpeed  = cfg.camera.shutterSpeed;
        stats.camFocusDist     = cfg.camera.focusDist;
        stats.camDofEnabled    = cfg.render.dofEnabled;
        stats.camAspectEnabled = false;
        stats.camAspectRatio   = 2.35f;
        int    viewMode    = 1;
        int    channelView = 0;   // 0=off 1=R 2=G 3=B
        bool   invertColors = false;
        struct { bool r, g, b, h, i; } prevKeys{};
        bool   prevLMB    = false;
        float  smoothFps  = 0.0f;
        double lastTime   = glfwGetTime();

        std::vector<float> cpuTimes, gpuGeomTimes, gpuSsaoTimes, gpuBlurTimes, gpuPostTimes;
        if (benchmarkN > 0) {
            cpuTimes.reserve(benchmarkN);
            gpuGeomTimes.reserve(benchmarkN);
            gpuSsaoTimes.reserve(benchmarkN);
            gpuBlurTimes.reserve(benchmarkN);
            gpuPostTimes.reserve(benchmarkN);
            stats.benchmarkMode = true;
        }

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

        glm::vec3 cachedHdriAngles(std::numeric_limits<float>::max());
        glm::mat3 cachedHdriRot(1.f);
        bool      cachedHdriFlipV    = false;
        bool      hdriDirty          = false;
        bool      iblPending         = false;

        // ── Initial IBL bake (pre-loop so frame 1 uses correct maps) ──
        {
            const glm::vec3 rad(
                glm::radians(cfg.hdri.rotation.x),
                glm::radians(cfg.hdri.rotation.y),
                glm::radians(cfg.hdri.rotation.z));
            cachedHdriRot = glm::mat3(
                glm::rotate(glm::mat4(1.f), rad.z, glm::vec3(0,0,1)) *
                glm::rotate(glm::mat4(1.f), rad.y, glm::vec3(0,1,0)) *
                glm::rotate(glm::mat4(1.f), rad.x, glm::vec3(1,0,0)));
            cachedHdriAngles = cfg.hdri.rotation;
            cachedHdriFlipV  = cfg.hdri.flipV;
            skyShader.use();
            skyShader.set("uHdriRotMat", cachedHdriRot);
            // Identity: IBL bakes are rotation-agnostic; uHdriRotMat in pbr.frag applies live rotation.
            baker.bake(skyTex.id(), glm::mat3(1.f), 1.0f, cfg.hdri.flipV, cfg.render.iblSamples);
        }

        while (!win.shouldClose()) {
            double now = glfwGetTime();
            float  dt  = static_cast<float>(now - lastTime);
            lastTime   = now;

            hdriDirty = (cfg.hdri.rotation != cachedHdriAngles
                      || cfg.hdri.flipV    != cachedHdriFlipV);
            if (hdriDirty) {
                if (cfg.hdri.rotation != cachedHdriAngles) {
                    const glm::vec3 hdriRotRad(
                        glm::radians(cfg.hdri.rotation.x),
                        glm::radians(cfg.hdri.rotation.y),
                        glm::radians(cfg.hdri.rotation.z));
                    cachedHdriRot = glm::mat3(
                        glm::rotate(glm::mat4(1.f), hdriRotRad.z, glm::vec3(0,0,1)) *
                        glm::rotate(glm::mat4(1.f), hdriRotRad.y, glm::vec3(0,1,0)) *
                        glm::rotate(glm::mat4(1.f), hdriRotRad.x, glm::vec3(1,0,0)));
                    cachedHdriAngles = cfg.hdri.rotation;
                }
                if (cfg.hdri.flipV != cachedHdriFlipV) {
                    cachedHdriFlipV = cfg.hdri.flipV;
                    iblPending = true;
                }
            }
            const glm::mat3& hdriRotMat = cachedHdriRot;

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
            if (ssaoQueryStarted[queryWrite]) {
                GLint available = 0;
                glGetQueryObjectiv(gpuSsaoQueries[queryWrite], GL_QUERY_RESULT_AVAILABLE, &available);
                if (available) {
                    GLuint ns = 0;
                    glGetQueryObjectuiv(gpuSsaoQueries[queryWrite], GL_QUERY_RESULT, &ns);
                    stats.gpuSsaoMs = static_cast<float>(ns) / 1e6f;
                }
            }
            {
                float hMs = 0.0f, vMs = 0.0f;
                if (blurQueryStarted[queryWrite]) {
                    GLint available = 0;
                    glGetQueryObjectiv(gpuBlurQueries[queryWrite], GL_QUERY_RESULT_AVAILABLE, &available);
                    if (available) {
                        GLuint ns = 0;
                        glGetQueryObjectuiv(gpuBlurQueries[queryWrite], GL_QUERY_RESULT, &ns);
                        hMs = static_cast<float>(ns) / 1e6f;
                    }
                }
                if (blurVQueryStarted[queryWrite]) {
                    GLint available = 0;
                    glGetQueryObjectiv(gpuBlurVQueries[queryWrite], GL_QUERY_RESULT_AVAILABLE, &available);
                    if (available) {
                        GLuint ns = 0;
                        glGetQueryObjectuiv(gpuBlurVQueries[queryWrite], GL_QUERY_RESULT, &ns);
                        vMs = static_cast<float>(ns) / 1e6f;
                    }
                }
                stats.gpuBlurMs = hMs + vMs;
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
            glPolygonMode(GL_FRONT_AND_BACK, viewMode == 4 ? GL_LINE : GL_FILL);
            if (viewMode == 4) {
                glEnable(GL_POLYGON_OFFSET_LINE);
                glPolygonOffset(-1.0f, -1.0f);
            }

            shader.use();
            shader.setAt(pbrLocView,         view);
            shader.setAt(pbrLocProjection,   proj);
            shader.setAt(pbrLocViewMode,     viewMode);
            shader.setAt(pbrLocNear,         camera.nearPlane());
            shader.setAt(pbrLocFar,          camera.farPlane());
            shader.setAt(pbrLocCamPos,       camera.position());
            shader.setAt(pbrLocRoughness,    cfg.shading.roughness);
            shader.setAt(pbrLocMetallic,     cfg.shading.metallic);
            shader.setAt(pbrLocIOR,          cfg.shading.ior);

            const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(geomMat)));
            shader.setAt(pbrLocNormalMatrix, normalMatrix);
            shader.setAt(pbrLocHdriRotMat,    hdriRotMat);
            shader.setAt(pbrLocHdriIntensity, cfg.hdri.intensity);

            Frustum frustum;
            frustum.update(proj * view);

            glBeginQuery(GL_TIME_ELAPSED, gpuQueries[queryWrite]);

            // Sky draw (depth test + write off — drawn at far plane, never occludes).
            if (skyVisible && viewMode == 1) {
                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                skyShader.use();
                skyShader.setAt(skyLocInvVP,        invVP);
                if (hdriDirty) skyShader.setAt(skyLocHdriRotMat, hdriRotMat);
                skyShader.setAt(skyLocHdriExposure, cfg.hdri.intensity);
                skyShader.setAt(skyLocHdriFlipV,    cfg.hdri.flipV);
                skyTex.bind(0);
                glBindVertexArray(blitVAO);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                glBindVertexArray(0);
                glDepthMask(GL_TRUE);
                glEnable(GL_DEPTH_TEST);
                shader.use();
            }

            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, baker.irradianceTex);
            glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, baker.prefilteredTex);
            glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, baker.brdfLUT);

            int drawn = 0;
            const int total = 1;  // single model; extend here when multi-model is added
            glm::vec3 geomCentre = glm::vec3(geomMat * glm::vec4(geom.centre(), 1.0f));
            if (frustum.testSphere(geomCentre, geom.boundingRadius())) {
                geom.draw(shader, geomMat);
                ++drawn;
            }

            // Bounds AOV: draw AABB wireframe box after geometry.
            if (viewMode == 3) {
                glDepthMask(GL_FALSE);
                lineShader.use();
                lineShader.setAt(lineLocVP, proj * view);
                glBindVertexArray(boxVAO);
                glDrawArrays(GL_LINES, 0, 24);
                glBindVertexArray(0);
                glDepthMask(GL_TRUE);
                shader.use();
            }

            glEndQuery(GL_TIME_ELAPSED);
            queryStarted[queryWrite] = true;

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDisable(GL_POLYGON_OFFSET_LINE);

            // ── SSAO pass ─────────────────────────────────────────
            glBeginQuery(GL_TIME_ELAPSED, gpuSsaoQueries[queryWrite]);
            glBindFramebuffer(GL_FRAMEBUFFER, ssaoRt.fbo);
            glViewport(0, 0, AO_W, AO_H);
            glDisable(GL_DEPTH_TEST);
            ssaoShader.use();
            ssaoShader.setAt(ssaoLocProj,    proj);
            ssaoShader.setAt(ssaoLocInvProj, invProj);
            ssaoShader.setAt(ssaoLocRadius,  cfg.shading.ssaoRadius);
            ssaoShader.setAt(ssaoLocBias,    cfg.shading.ssaoBias);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, rt.normalTex);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, rt.depthTex);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, noiseTex);
            glBindVertexArray(blitVAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glEndQuery(GL_TIME_ELAPSED);
            ssaoQueryStarted[queryWrite] = true;

            // ── SSAO blur H pass ──────────────────────────────────
            glBeginQuery(GL_TIME_ELAPSED, gpuBlurQueries[queryWrite]);
            glBindFramebuffer(GL_FRAMEBUFFER, blurTmpRt.fbo);
            blurShader.use();
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, ssaoRt.tex);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glEndQuery(GL_TIME_ELAPSED);
            blurQueryStarted[queryWrite] = true;

            // ── SSAO blur V pass ──────────────────────────────────
            glBeginQuery(GL_TIME_ELAPSED, gpuBlurVQueries[queryWrite]);
            glBindFramebuffer(GL_FRAMEBUFFER, blurRt.fbo);
            blurVShader.use();
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, blurTmpRt.tex);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);
            glEndQuery(GL_TIME_ELAPSED);
            blurVQueryStarted[queryWrite] = true;

            // ── DoF pass (CoC-based Poisson disc blur) ────────────
            GLuint blitColorTex = rt.colorTex;  // default: passthrough
            if (stats.camDofEnabled) {
                glBindFramebuffer(GL_FRAMEBUFFER, dofFbo);
                glViewport(0, 0, BASE_W, BASE_H);
                dofShader.use();
                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, rt.colorTex);
                glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, rt.depthTex);
                dofShader.setAt(dofLocNear,      camera.nearPlane());
                dofShader.setAt(dofLocFar,       camera.farPlane());
                dofShader.setAt(dofLocFocusDist, camera.focusDist());
                dofShader.setAt(dofLocCocScale,  camera.cocScale(static_cast<float>(BASE_W)));
                dofShader.setAt(dofLocSamples,   cfg.render.dofSamples);
                glBindVertexArray(blitVAO);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                glBindVertexArray(0);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                blitColorTex = dofTex;
            }

            // ── Blit FBO → screen ──────────────────────────────────
            glBeginQuery(GL_TIME_ELAPSED, gpuPostQueries[queryWrite]);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, win.width(), win.height());
            blitShader.use();
            blitShader.setAt(blitLocViewMode,    viewMode);
            blitShader.setAt(blitLocChannelView, channelView);
            blitShader.setAt(blitLocInvert,      invertColors);
            {
                float physExposure  = camera.exposureValue();
                float finalExposure = physExposure * std::pow(2.0f, cfg.hdri.exposure);
                blitShader.setAt(blitLocExposure, finalExposure);
            }
            blitShader.setAt(blitLocAspectEnabled, stats.camAspectEnabled);
            blitShader.setAt(blitLocAspectRatio,   stats.camAspectRatio);
            blitShader.setAt(blitLocLutEnabled,    colorPipeline.enabled());
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, blitColorTex);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, blurRt.tex);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, rt.depthTex);
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_3D, colorPipeline.lut_tex());
            glBindVertexArray(blitVAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);
            glEndQuery(GL_TIME_ELAPSED);
            postQueryStarted[queryWrite] = true;
            queryWrite = (queryWrite + 1) % GPU_QUERY_FRAMES;

            glEnable(GL_DEPTH_TEST);

            // ── Histogram (throttled readback every 4 frames, async PBO) ─────
            if (!stats.benchmarkMode) {
                static int     histFrames = 0;
                static int     histTick   = 0;
                static uint8_t histPx[256 * 144 * 3];
                if (++histFrames >= 4) {
                    histFrames = 0;
                    const int cur  = histTick & 1;
                    const int prev = cur ^ 1;

                    int srcY0 = 0, srcY1 = win.height();
                    if (stats.camAspectEnabled) {
                        float scr  = float(win.width()) / float(win.height());
                        float barH = 0.5f * (1.0f - scr / stats.camAspectRatio);
                        if (barH > 0.0f) {
                            srcY0 = static_cast<int>(std::round(barH * float(win.height())));
                            srcY1 = win.height() - srcY0;
                        }
                    }
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, histFBO);
                    glBlitFramebuffer(0, srcY0, win.width(), srcY1,
                                      0, 0, 256, 144,
                                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, histFBO);

                    glBindBuffer(GL_PIXEL_PACK_BUFFER, histPBOs[cur]);
                    glReadPixels(0, 0, 256, 144, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

                    if (histTick > 0) {
                        glBindBuffer(GL_PIXEL_PACK_BUFFER, histPBOs[prev]);
                        const auto* ptr = static_cast<const uint8_t*>(
                            glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));
                        if (ptr) {
                            memcpy(histPx, ptr, sizeof(histPx));
                            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
                        }
                        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

                        uint32_t h[3][256]{};
                        const uint8_t* p = histPx;
                        for (int i = 0; i < 256 * 144; ++i, p += 3)
                            ++h[0][p[0]], ++h[1][p[1]], ++h[2][p[2]];
                        memcpy(stats.hist, h, sizeof(h));
                        stats.histValid = 1;
                    } else {
                        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                    }
                    ++histTick;
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
            stats.totalPoints     = geom.indexCount();
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
            stats.camISO          = camera.iso();
            stats.camFStop        = camera.fStop();
            stats.camShutterSpeed = camera.shutterSpeed();
            stats.camFocusDist    = camera.focusDist();
            stats.viewMode        = viewMode;
            stats.channelView     = channelView;
            stats.hdriYawDeg      = cfg.hdri.rotation.y;
            stats.hdriFlipV       = cfg.hdri.flipV;
            stats.hdriIntensity   = cfg.hdri.intensity;
            stats.hdriEvOffset    = cfg.hdri.exposure;
            stats.skyVisible      = skyVisible;
            stats.numObjects      = 1;
            stats.objects[0]      = {geom.name().c_str(), geom.triangleCount(), geom.indexCount()};

            if (stats.benchmarkMode) {
                // Skip GPU_QUERY_FRAMES warm-up frames: ring buffer zeros + Metal PSO JIT.
                static int warmup = GPU_QUERY_FRAMES;
                if (warmup > 0) { --warmup; }
                else {
                    cpuTimes.push_back(stats.frameTimeMs);
                    gpuGeomTimes.push_back(stats.gpuGeomMs);
                    gpuSsaoTimes.push_back(stats.gpuSsaoMs);
                    gpuBlurTimes.push_back(stats.gpuBlurMs);
                    gpuPostTimes.push_back(stats.gpuPostMs);
                    if (static_cast<int>(cpuTimes.size()) >= benchmarkN) break;
                }
            }

            if (!stats.benchmarkMode) {
                hud.beginFrame();
                if (iblPending) {
                    ImGui::SetNextWindowPos(ImVec2(win.width() * 0.5f - 55.0f, win.height() * 0.5f - 12.0f));
                    ImGui::SetNextWindowBgAlpha(0.75f);
                    ImGui::Begin("##baking", nullptr,
                        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
                        ImGuiWindowFlags_NoMove);
                    ImGui::Text("Baking IBL...");
                    ImGui::End();
                }
                hud.draw(stats);
                hud.endFrame();
            }

            // Merge native menu one-shot actions into stats, then sync checkmarks.
            if (menuFlags.doCaptureEXR) { stats.doCaptureEXR = true; menuFlags.doCaptureEXR = false; }
            if (menuFlags.doCapturePNG) { stats.doCapturePNG = true; menuFlags.doCapturePNG = false; }
            if (menuFlags.doSaveJson) { stats.doSaveJson = true; menuFlags.doSaveJson = false; }
            if (menuFlags.showPanel != stats.showPanel)
                stats.showPanel = menuFlags.showPanel;

            if (stats.viewLutChanged) {
                cfg.color.viewLut = stats.viewLut;
                colorPipeline.bake(static_cast<ViewLut>(cfg.color.viewLut));
                stats.viewLutChanged = false;
            }

            viewMode             = stats.viewMode;
            cfg.hdri.rotation.y  = stats.hdriYawDeg;
            cfg.hdri.flipV       = stats.hdriFlipV;
            cfg.hdri.intensity   = stats.hdriIntensity;
            cfg.hdri.exposure    = stats.hdriEvOffset;
            skyVisible           = stats.skyVisible;
            camera.setFocalLength(stats.camFocalLengthMm);
            camera.setISO(stats.camISO);
            camera.setFStop(stats.camFStop);
            camera.setShutterSpeed(stats.camShutterSpeed);
            camera.setFocusDist(stats.camFocusDist);
            cfg.camera.iso           = stats.camISO;
            cfg.camera.fStop         = stats.camFStop;
            cfg.camera.shutterSpeed  = stats.camShutterSpeed;
            cfg.camera.focusDist     = stats.camFocusDist;
            cfg.render.dofEnabled    = stats.camDofEnabled;

            menuFlags.showPanel = stats.showPanel;
            syncOsxMenuBar(menuFlags);

            if (stats.doCaptureEXR) {
                const char* home = getenv("HOME");
                std::string desktop = home ? std::string(home) + "/Desktop" : ".";
                std::time_t t = std::time(nullptr);
                std::string fname = desktop + "/KODAK_" + std::to_string(t) + ".exr";

                float finalExposure = camera.exposureValue() * std::pow(2.0f, cfg.hdri.exposure);

                // Read GL texture → AovBuffer, flipping Y (GL bottom-left → EXR top-left).
                auto read_rgb = [&](GLuint tex, const std::string& name, float mul) {
                    AovBuffer buf;
                    buf.name = name;
                    buf.rgb.resize(static_cast<size_t>(BASE_W * BASE_H * 3));
                    std::vector<float> tmp(static_cast<size_t>(BASE_W * BASE_H * 3));
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, tmp.data());
                    for (int y = 0; y < BASE_H; ++y) {
                        const int src_y = BASE_H - 1 - y;
                        for (int x = 0; x < BASE_W; ++x) {
                            const int src = (src_y * BASE_W + x) * 3;
                            const int dst = (y     * BASE_W + x) * 3;
                            buf.rgb[dst + 0] = tmp[src + 0] * mul;
                            buf.rgb[dst + 1] = tmp[src + 1] * mul;
                            buf.rgb[dst + 2] = tmp[src + 2] * mul;
                        }
                    }
                    return buf;
                };

                auto read_depth = [&](GLuint tex) {
                    AovBuffer buf;
                    buf.name = "depth";
                    buf.rgb.resize(static_cast<size_t>(BASE_W * BASE_H * 3));
                    std::vector<float> tmp(static_cast<size_t>(BASE_W * BASE_H));
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, tmp.data());
                    for (int y = 0; y < BASE_H; ++y) {
                        const int src_y = BASE_H - 1 - y;
                        for (int x = 0; x < BASE_W; ++x) {
                            const float d   = tmp[src_y * BASE_W + x];
                            const int   dst = (y * BASE_W + x) * 3;
                            buf.rgb[dst + 0] = d;
                            buf.rgb[dst + 1] = d;
                            buf.rgb[dst + 2] = d;
                        }
                    }
                    return buf;
                };

                // Read single-channel AO (GL_R16F) and replicate to RGB.
                auto read_ao = [&](GLuint tex) {
                    AovBuffer buf;
                    buf.name = "ao";
                    buf.rgb.resize(static_cast<size_t>(BASE_W * BASE_H * 3));
                    std::vector<float> tmp(static_cast<size_t>(BASE_W * BASE_H));
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, tmp.data());
                    for (int y = 0; y < BASE_H; ++y) {
                        const int src_y = BASE_H - 1 - y;
                        for (int x = 0; x < BASE_W; ++x) {
                            const float v   = tmp[src_y * BASE_W + x];
                            const int   dst = (y * BASE_W + x) * 3;
                            buf.rgb[dst + 0] = v;
                            buf.rgb[dst + 1] = v;
                            buf.rgb[dst + 2] = v;
                        }
                    }
                    return buf;
                };

                try {
                    write_exr_multilayer(fname, BASE_W, BASE_H, {
                        read_rgb(rt.colorTex,  "beauty",  finalExposure),
                        read_rgb(rt.normalTex, "normals", 1.0f),
                        read_depth(rt.depthTex),
                        read_ao(blurRt.tex),
                    });
                    LOG_I("Export OpenEXR: " + fname);
                } catch (const std::exception& e) {
                    LOG_E("EXR save failed: " + std::string(e.what()));
                }
                stats.doCaptureEXR = false;
            }

            if (stats.doCapturePNG) {
                const char* home = getenv("HOME");
                std::string desktop = home ? std::string(home) + "/Desktop" : ".";
                std::time_t t = std::time(nullptr);
                std::string fname = desktop + "/KODAK_" + std::to_string(t) + ".png";

                std::vector<unsigned char> pixels(static_cast<size_t>(BASE_W * BASE_H * 3));
                glReadPixels(0, 0, BASE_W, BASE_H, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
                // Flip Y: OpenGL origin is bottom-left, PNG expects top-left.
                std::vector<unsigned char> flipped(pixels.size());
                for (int y = 0; y < BASE_H; ++y) {
                    const int src_y = BASE_H - 1 - y;
                    std::memcpy(flipped.data() + static_cast<size_t>(y * BASE_W * 3),
                                pixels.data() + static_cast<size_t>(src_y * BASE_W * 3),
                                static_cast<size_t>(BASE_W * 3));
                }
                if (stbi_write_png(fname.c_str(), BASE_W, BASE_H, 3, flipped.data(), BASE_W * 3))
                    LOG_I("Export PNG: " + fname);
                else
                    LOG_E("PNG save failed: " + fname);
                stats.doCapturePNG = false;
            }

            if (stats.doSaveJson) {
                cfg.camera.position    = camera.position();
                cfg.camera.focalLength = camera.focalLength();
                saveConfig(cfg, "config/profile.json");
                LOG_I("Scene saved.");
                stats.doSaveJson = false;
            }

            win.swapAndPoll();

            if (iblPending) {
                // Identity: IBL bakes are rotation-agnostic; uHdriRotMat in pbr.frag applies live rotation.
                baker.bake(skyTex.id(), glm::mat3(1.f), 1.0f, cfg.hdri.flipV, cfg.render.iblSamples);
                iblPending = false;
            }
        }

        if (benchmarkN > 0 && !cpuTimes.empty()) {
            auto summarise = [](std::vector<float> v) {
                std::sort(v.begin(), v.end());
                float sum = std::accumulate(v.begin(), v.end(), 0.0f);
                auto pct  = [&](float p) { return v[static_cast<size_t>(p * (v.size() - 1))]; };
                return nlohmann::json{
                    {"mean", sum / v.size()},
                    {"p50",  pct(0.50f)},
                    {"p95",  pct(0.95f)},
                    {"p99",  pct(0.99f)},
                    {"max",  pct(1.00f)},
                };
            };
            float meanCpu = std::accumulate(cpuTimes.begin(), cpuTimes.end(), 0.0f) / cpuTimes.size();
            nlohmann::json j = {
                {"meanFps",    1000.0f / meanCpu},
                {"cpuFrameMs", summarise(cpuTimes)},
                {"gpuGeomMs",  summarise(gpuGeomTimes)},
                {"gpuSsaoMs",  summarise(gpuSsaoTimes)},
                {"gpuBlurMs",  summarise(gpuBlurTimes)},
                {"gpuPostMs",  summarise(gpuPostTimes)},
                {"config", {
                    {"width",          BASE_W},
                    {"height",         BASE_H},
                    {"iblSamples",     cfg.render.iblSamples},
                    {"ssaoSamples",    cfg.shading.ssaoSamples},
                    {"ssaoHalfRes",    cfg.shading.ssaoHalfRes},
                    {"ssaoBlurRadius", cfg.shading.ssaoBlurRadius},
                }},
            };
            if (benchmarkOut.empty()) {
                std::time_t t = std::time(nullptr);
                char buf[32];
                std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", std::localtime(&t));
                benchmarkOut = std::string("benchmarks/") + buf + ".json";
            }
            std::ofstream f(benchmarkOut);
            if (!f) { LOG_E("Could not open " + benchmarkOut + " for writing"); }
            else    { f << j.dump(2) << '\n'; LOG_I("Benchmark written to " + benchmarkOut); }
        }

        baker.destroy();
        rt.destroy();
        ssaoRt.destroy();
        blurRt.destroy();
        glDeleteFramebuffers(1, &dofFbo);
        glDeleteTextures(1, &dofTex);
        glDeleteFramebuffers(1, &histFBO);
        glDeleteTextures(1, &histTex);
        glDeleteBuffers(2, histPBOs);
        glDeleteTextures(1, &noiseTex);
        glDeleteBuffers(1, &ssaoKernelUBO);
        glDeleteVertexArrays(1, &blitVAO);
        glDeleteVertexArrays(1, &boxVAO);
        glDeleteBuffers(1, &boxVBO);
        glDeleteQueries(GPU_QUERY_FRAMES, gpuQueries);
        glDeleteQueries(GPU_QUERY_FRAMES, gpuPostQueries);
        glDeleteQueries(GPU_QUERY_FRAMES, gpuSsaoQueries);
        glDeleteQueries(GPU_QUERY_FRAMES, gpuBlurQueries);
        glDeleteQueries(GPU_QUERY_FRAMES, gpuBlurVQueries);
        blurTmpRt.destroy();

    } catch (const std::exception& e) {
        LOG_E(std::string("Fatal: ") + e.what());
        return 1;
    }
    return 0;
}
