#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <stdexcept>
#include "gl_context.hpp"
#include "shader.hpp"

static std::string writeShaderTmp(const char* name, const char* src) {
    std::string path = std::string("/tmp/kodak_shader_") + name;
    std::ofstream(path) << src;
    return path;
}

TEST_CASE("Shader compiles actual project blit shaders") {
    GlContext ctx;
    // WORKING_DIRECTORY is CMAKE_SOURCE_DIR, so relative paths work
    REQUIRE_NOTHROW(Shader("shaders/post/blit.vert", "shaders/post/blit.frag"));
}

TEST_CASE("Shader compiles minimal valid GLSL pair") {
    GlContext ctx;
    auto vp = writeShaderTmp("valid.vert",
        "#version 330 core\nvoid main(){ gl_Position = vec4(0.0); }");
    auto fp = writeShaderTmp("valid.frag",
        "#version 330 core\nout vec4 c;\nvoid main(){ c = vec4(1.0); }");
    REQUIRE_NOTHROW(Shader(vp, fp));
}

TEST_CASE("Shader throws on syntax error in fragment shader") {
    GlContext ctx;
    auto vp = writeShaderTmp("err.vert",
        "#version 330 core\nvoid main(){ gl_Position = vec4(0.0); }");
    auto fp = writeShaderTmp("err.frag",
        "#version 330 core\nout vec4 c;\nvoid main(){ SYNTAX ERROR }");
    REQUIRE_THROWS_AS(Shader(vp, fp), std::runtime_error);
}

TEST_CASE("Shader throws on missing file") {
    GlContext ctx;
    REQUIRE_THROWS_AS(
        Shader("nonexistent.vert", "nonexistent.frag"),
        std::runtime_error
    );
}
