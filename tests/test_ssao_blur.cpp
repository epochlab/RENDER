#include <catch2/catch_test_macros.hpp>
#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>
#include "gl_context.hpp"
#include "shader.hpp"

TEST_CASE("SSAO blur: separable H×V matches 2D box filter", "[ssao][blur][render]") {
    GlContext ctx;

    constexpr int W = 16, H = 16, N = W * H;
    constexpr int RADIUS = 2;

    // Non-trivial input pattern in [0, 1]
    std::vector<float> src(N);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            src[y * W + x] = float((x * 7 + y * 13) % 256) / 255.0f;

    GLuint inTex;
    glGenTextures(1, &inTex);
    glBindTexture(GL_TEXTURE_2D, inTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, W, H, 0, GL_RED, GL_FLOAT, src.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Helper: make a 1-channel GL_R32F FBO
    auto makeFbo = [&](GLuint& fbo, GLuint& tex) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, W, H, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        const GLenum buf = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &buf);
        REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    };

    GLuint ref2dFbo, ref2dTex;
    GLuint tmpFbo,   tmpTex;
    GLuint sepFbo,   sepTex;
    makeFbo(ref2dFbo, ref2dTex);
    makeFbo(tmpFbo,   tmpTex);
    makeFbo(sepFbo,   sepTex);

    Shader blur2d("shaders/post/ssao.vert", "tests/shaders/ssao_blur_2d.frag");
    blur2d.use();
    blur2d.set("uSSAO", 0);
    blur2d.set("uBlurRadius", RADIUS);

    Shader blurH("shaders/post/ssao.vert", "shaders/post/ssao_blur.frag");
    blurH.use();
    blurH.set("uSSAO", 0);
    blurH.set("uBlurRadius", RADIUS);

    Shader blurV("shaders/post/ssao.vert", "shaders/post/ssao_blur_v.frag");
    blurV.use();
    blurV.set("uSSAO", 0);
    blurV.set("uBlurRadius", RADIUS);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glViewport(0, 0, W, H);
    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);

    glBindFramebuffer(GL_FRAMEBUFFER, ref2dFbo);
    blur2d.use();
    glBindTexture(GL_TEXTURE_2D, inTex);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, tmpFbo);
    blurH.use();
    glBindTexture(GL_TEXTURE_2D, inTex);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, sepFbo);
    blurV.use();
    glBindTexture(GL_TEXTURE_2D, tmpTex);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindVertexArray(0);
    glFinish();

    std::vector<float> ref2d(N), sep(N);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, ref2dFbo);
    glReadPixels(0, 0, W, H, GL_RED, GL_FLOAT, ref2d.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sepFbo);
    glReadPixels(0, 0, W, H, GL_RED, GL_FLOAT, sep.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    float maxDiff = 0.0f;
    for (int i = 0; i < N; ++i)
        maxDiff = std::max(maxDiff, std::abs(ref2d[i] - sep[i]));

    INFO("max pixel diff = " << maxDiff);
    CHECK(maxDiff < 2e-5f);

    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &ref2dFbo);
    glDeleteFramebuffers(1, &tmpFbo);
    glDeleteFramebuffers(1, &sepFbo);
    glDeleteTextures(1, &ref2dTex);
    glDeleteTextures(1, &tmpTex);
    glDeleteTextures(1, &sepTex);
    glDeleteTextures(1, &inTex);
}
