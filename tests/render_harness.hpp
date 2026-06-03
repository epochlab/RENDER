#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <memory>
#include "shader.hpp"
#include "texture.hpp"

// RAII render harness for AOV pixel readback tests.
// Creates two 1×1 FBOs (G-buffer + screen), compiles pbr+blit shaders, and
// holds controlled textures (white HDRI, white albedo, flat normal map).
// Lifetime must be inside a valid GlContext.
struct RenderHarness {
    // G-buffer FBO
    GLuint gFbo      = 0;
    GLuint colorTex  = 0;   // GL_RGB16F — gColor (AOV output)
    GLuint normalTex = 0;   // GL_RGB16F — gNormal (not read in tests)
    GLuint depthTex  = 0;   // GL_DEPTH_COMPONENT32F

    // Screen FBO — GL_RGB8 for direct uint8 readback
    GLuint screenFbo = 0;
    GLuint screenTex = 0;

    GLuint blitVAO   = 0;   // empty VAO for gl_VertexID-driven draws

    std::unique_ptr<Shader> pbrShader;   // tests/shaders/test_geom.vert + shaders/geometry/pbr.frag
    std::unique_ptr<Shader> blitShader;  // shaders/post/blit.vert + shaders/post/blit.frag

    Texture albedoTex;     // unit 0 — Texture::white()
    Texture skyTex;        // unit 1 — Texture::white() (constant-luminance HDRI)
    Texture normalMapTex;  // unit 2 — Texture::flatNormal()
    Texture aoTex;         // unit 1 in blit pass — Texture::white() (AO=1 everywhere)

    RenderHarness();
    ~RenderHarness();

    RenderHarness(const RenderHarness&) = delete;
    RenderHarness& operator=(const RenderHarness&) = delete;

    // Renders the controlled scene for viewMode; returns the 1×1 pixel as [0,1] RGB floats.
    glm::vec3 renderMode(int viewMode) const;
};
