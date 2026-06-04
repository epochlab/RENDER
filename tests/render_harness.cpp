#include "render_harness.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

static GLuint makeTex(GLenum internalFmt, GLenum fmt, GLenum type, GLenum filter) {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, 1, 1, 0, fmt, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Depth textures: disable shadow comparison so texture() returns raw float
    if (internalFmt == GL_DEPTH_COMPONENT32F)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

RenderHarness::RenderHarness()
    : albedoTex(Texture::white())
    , skyTex(Texture::white())
    , normalMapTex(Texture::flatNormal())
    , aoTex(Texture::white())
    , iblIrradianceTex(Texture::white())
    , iblPrefilteredTex(Texture::white())
    , iblBrdfLutTex(Texture::white())
{
    // ── G-buffer FBO ──────────────────────────────────────────────────────
    colorTex  = makeTex(GL_RGB16F,            GL_RGB,   GL_FLOAT,        GL_NEAREST);
    normalTex = makeTex(GL_RGB16F,            GL_RGB,   GL_FLOAT,        GL_NEAREST);
    depthTex  = makeTex(GL_DEPTH_COMPONENT32F,GL_DEPTH_COMPONENT,GL_FLOAT,GL_NEAREST);

    glGenFramebuffers(1, &gFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex,  0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normalTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,  GL_TEXTURE_2D, depthTex,  0);
    const GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, bufs);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("RenderHarness: G-buffer FBO incomplete");

    // ── Screen FBO ────────────────────────────────────────────────────────
    screenTex = makeTex(GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, GL_NEAREST);
    glGenFramebuffers(1, &screenFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, screenFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screenTex, 0);
    const GLenum screenBuf = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &screenBuf);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("RenderHarness: screen FBO incomplete");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ── Empty VAO ─────────────────────────────────────────────────────────
    glGenVertexArrays(1, &blitVAO);

    // ── Shaders ───────────────────────────────────────────────────────────
    pbrShader  = std::make_unique<Shader>("tests/shaders/test_geom.vert",
                                         "shaders/geometry/pbr.frag");
    blitShader = std::make_unique<Shader>("shaders/post/blit.vert",
                                         "shaders/post/blit.frag");

    // ── Static pbr uniforms ───────────────────────────────────────────────
    pbrShader->use();
    pbrShader->set("uAlbedo",         0);
    pbrShader->set("uNormalMap",      2);
    pbrShader->set("uIrradianceTex",  3);
    pbrShader->set("uPrefilteredTex", 4);
    pbrShader->set("uBrdfLUT",        5);
    pbrShader->set("uMaxMipLevel",    4.0f);
    pbrShader->set("uCamPos",   glm::vec3(0.0f, 0.0f, 5.0f));
    pbrShader->set("uRoughness", 0.0f);
    pbrShader->set("uMetallic",  0.0f);
    pbrShader->set("uIOR",       1.5f);
    pbrShader->set("uBoundsMin", glm::vec3(-1.0f, -1.0f, -1.0f));
    pbrShader->set("uBoundsMax", glm::vec3( 1.0f,  1.0f,  1.0f));
    pbrShader->set("uNear",  0.1f);
    pbrShader->set("uFar",  100.0f);
    pbrShader->set("uView",       glm::mat4(1.0f));
    pbrShader->set("uHdriRotMat",    glm::mat3(1.0f));
    pbrShader->set("uHdriIntensity", 1.0f);

    // ── Static blit uniforms ──────────────────────────────────────────────
    blitShader->use();
    blitShader->set("uFrame",       0);
    blitShader->set("uAO",          1);
    blitShader->set("uDepth",       2);
    blitShader->set("uChannelView", 0);
    blitShader->set("uInvert",      false);
    blitShader->set("uExposure",      1.0f);
    blitShader->set("uAspectEnabled", false);
    blitShader->set("uAspectRatio",   2.39f);
}

RenderHarness::~RenderHarness() {
    if (blitVAO)   glDeleteVertexArrays(1, &blitVAO);
    if (gFbo)      glDeleteFramebuffers(1, &gFbo);
    if (screenFbo) glDeleteFramebuffers(1, &screenFbo);
    if (colorTex)  glDeleteTextures(1, &colorTex);
    if (normalTex) glDeleteTextures(1, &normalTex);
    if (depthTex)  glDeleteTextures(1, &depthTex);
    if (screenTex) glDeleteTextures(1, &screenTex);
}

glm::vec3 RenderHarness::renderMode(int viewMode) const {
    glViewport(0, 0, 1, 1);

    // ── Geometry pass → G-buffer ──────────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, gFbo);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    pbrShader->use();
    pbrShader->set("uViewMode", viewMode);
    albedoTex.bind(0);
    normalMapTex.bind(2);
    iblIrradianceTex.bind(3);
    iblPrefilteredTex.bind(4);
    iblBrdfLutTex.bind(5);

    glBindVertexArray(blitVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    // ── Blit pass → screen ────────────────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, screenFbo);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    blitShader->use();
    blitShader->set("uViewMode", viewMode);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTex);   // G-buffer color
    aoTex.bind(1);                             // AO = white (1.0)
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, depthTex);   // G-buffer depth

    glBindVertexArray(blitVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    // ── Pixel readback ────────────────────────────────────────────────────
    glBindFramebuffer(GL_READ_FRAMEBUFFER, screenFbo);
    unsigned char px[3] = {0, 0, 0};
    glReadPixels(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, px);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    return glm::vec3(px[0] / 255.0f, px[1] / 255.0f, px[2] / 255.0f);
}
