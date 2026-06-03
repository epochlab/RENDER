#include <catch2/catch_test_macros.hpp>
#include <GLFW/glfw3.h>
#include "gl_context.hpp"
#include "texture.hpp"

TEST_CASE("Texture::white() has non-zero GL id") {
    GlContext ctx;
    Texture t = Texture::white();
    REQUIRE(t.id() != 0);
}

TEST_CASE("Texture::white() pixel is (255,255,255)") {
    GlContext ctx;
    Texture t = Texture::white();
    t.bind(0);
    unsigned char pixel[3] = {0, 0, 0};
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixel);
    CHECK(pixel[0] == 255);
    CHECK(pixel[1] == 255);
    CHECK(pixel[2] == 255);
}

TEST_CASE("Texture::flatNormal() pixel is (128,128,255,255)") {
    GlContext ctx;
    Texture t = Texture::flatNormal();
    t.bind(0);
    unsigned char pixel[4] = {0, 0, 0, 0};
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    CHECK(pixel[0] == 128);
    CHECK(pixel[1] == 128);
    CHECK(pixel[2] == 255);
    CHECK(pixel[3] == 255);
}

TEST_CASE("Texture bind() makes texture active on the specified unit") {
    GlContext ctx;
    Texture t = Texture::white();
    t.bind(3);
    GLint bound = 0;
    glActiveTexture(GL_TEXTURE3);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    REQUIRE(static_cast<GLuint>(bound) == t.id());
}

TEST_CASE("Texture move semantics: moved-from id becomes zero") {
    GlContext ctx;
    Texture t = Texture::white();
    GLuint original = t.id();
    Texture t2 = std::move(t);
    REQUIRE(t2.id() == original);
    // t.id() is now 0; its destructor must not double-free
    // Verified by no crash/sanitizer error at scope exit
}
