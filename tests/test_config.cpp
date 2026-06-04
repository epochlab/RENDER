#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <fstream>
#include <cstdio>
#include "config.hpp"

using Catch::Matchers::WithinAbs;
static constexpr float kEps = 1e-5f;

static std::string writeTmp(const char* name, const char* content) {
    std::string path = std::string("/tmp/kodak_test_") + name + ".json";
    std::ofstream(path) << content;
    return path;
}

TEST_CASE("Config defaults when paths are empty") {
    AppConfig cfg = loadConfig("", "");
    REQUIRE(cfg.render.width      == 2048);
    REQUIRE(cfg.render.height     == 1152);
    REQUIRE(cfg.render.downsample == 2);
    REQUIRE(cfg.render.iblSamples == 16);
    REQUIRE_THAT(cfg.shading.ior,         WithinAbs(1.5f,  kEps));
    REQUIRE_THAT(cfg.shading.ssaoRadius,  WithinAbs(0.5f,  kEps));
    REQUIRE_THAT(cfg.shading.ssaoBias,    WithinAbs(0.025f,kEps));
    REQUIRE(cfg.shading.ssaoBlurRadius == 2);
    REQUIRE(cfg.shading.ssaoHalfRes    == false);
    REQUIRE_THAT(cfg.camera.focalLength, WithinAbs(70.f, kEps));
    REQUIRE_THAT(cfg.camera.yaw,         WithinAbs(-90.f, kEps));
    REQUIRE(cfg.hdri.visible  == true);
    REQUIRE_THAT(cfg.hdri.exposure,  WithinAbs(0.f,  kEps));   // EV offset default = 0
    REQUIRE_THAT(cfg.hdri.intensity, WithinAbs(1.f,  kEps));
}

TEST_CASE("Config profile.json overrides render and shading") {
    std::string p = writeTmp("profile", R"({
        "render":  {"width":1920,"height":1080,"downsample":1,"iblSamples":32},
        "shading": {"ior":1.33,"ssaoRadius":0.3,"ssaoBias":0.01,"ssaoBlurRadius":1,"ssaoHalfRes":true}
    })");
    AppConfig cfg = loadConfig(p, "");
    std::remove(p.c_str());

    REQUIRE(cfg.render.width      == 1920);
    REQUIRE(cfg.render.height     == 1080);
    REQUIRE(cfg.render.downsample == 1);
    REQUIRE(cfg.render.iblSamples == 32);
    REQUIRE_THAT(cfg.shading.ior,        WithinAbs(1.33f, 1e-4f));
    REQUIRE_THAT(cfg.shading.ssaoRadius, WithinAbs(0.3f,  kEps));
    REQUIRE_THAT(cfg.shading.ssaoBias,   WithinAbs(0.01f, kEps));
    REQUIRE(cfg.shading.ssaoBlurRadius == 1);
    REQUIRE(cfg.shading.ssaoHalfRes    == true);
}

TEST_CASE("Config scene.json overrides camera and HDRI") {
    // Camera and HDRI are now in profile.json, not scene.json.
    std::string p = writeTmp("profile_camera", R"({
        "camera": {"position":[1,2,3],"yaw":-45.0,"pitch":10.0,
                   "near":0.01,"far":200.0,"filmback":56.0,"focalLength":85.0},
        "hdri":   {"path":"assets/test.hdr","rotation":[0,90,0],
                   "visible":false,"exposure":2.0,"intensity":0.5,"flipV":true}
    })");
    AppConfig cfg = loadConfig(p, "");
    std::remove(p.c_str());

    REQUIRE_THAT(cfg.camera.position.x, WithinAbs(1.f, kEps));
    REQUIRE_THAT(cfg.camera.position.y, WithinAbs(2.f, kEps));
    REQUIRE_THAT(cfg.camera.position.z, WithinAbs(3.f, kEps));
    REQUIRE_THAT(cfg.camera.yaw,         WithinAbs(-45.f,  kEps));
    REQUIRE_THAT(cfg.camera.pitch,       WithinAbs(10.f,   kEps));
    REQUIRE_THAT(cfg.camera.near,        WithinAbs(0.01f,  kEps));
    REQUIRE_THAT(cfg.camera.far,         WithinAbs(200.f,  kEps));
    REQUIRE_THAT(cfg.camera.filmback,    WithinAbs(56.f,   kEps));
    REQUIRE_THAT(cfg.camera.focalLength, WithinAbs(85.f,   kEps));
    REQUIRE(cfg.hdri.path    == "assets/test.hdr");
    REQUIRE(cfg.hdri.visible == false);
    REQUIRE_THAT(cfg.hdri.exposure,   WithinAbs(2.f,  kEps));
    REQUIRE_THAT(cfg.hdri.intensity,  WithinAbs(0.5f, kEps));
    REQUIRE_THAT(cfg.hdri.rotation.y, WithinAbs(90.f, kEps));
    REQUIRE(cfg.hdri.flipV == true);
}

TEST_CASE("Config malformed JSON falls back to defaults") {
    std::string p = writeTmp("bad_profile", "{not valid json");
    std::string s = writeTmp("bad_scene",   "{");
    AppConfig cfg = loadConfig(p, s);
    std::remove(p.c_str());
    std::remove(s.c_str());

    REQUIRE(cfg.render.width == 2048);
    REQUIRE_THAT(cfg.shading.ior, WithinAbs(1.5f, kEps));
}

TEST_CASE("Config wrong-type field falls back per-key") {
    std::string p = writeTmp("wrongtype", R"({"render":{"width":"bad","height":1080}})");
    AppConfig cfg = loadConfig(p, "");
    std::remove(p.c_str());

    // "width" has wrong type → struct default
    REQUIRE(cfg.render.width  == 2048);
    // "height" is correct → parsed
    REQUIRE(cfg.render.height == 1080);
}

TEST_CASE("saveConfig round-trip persists position, focalLength, hdri.rotation") {
    std::string path = "/tmp/kodak_test_roundtrip.json";
    // Seed a profile file with a camera.pitch value not written by saveConfig
    std::ofstream(path) << R"({"camera":{"pitch":15.0}})";

    AppConfig cfg;
    cfg.camera.position    = {4.f, 5.f, 6.f};
    cfg.camera.focalLength = 100.f;
    cfg.hdri.rotation      = {0.f, 45.f, 0.f};
    saveConfig(cfg, path);   // writes to profile.json

    AppConfig reloaded = loadConfig(path, "");   // reads camera/hdri from profile.json
    std::remove(path.c_str());

    REQUIRE_THAT(reloaded.camera.position.x,  WithinAbs(4.f,   kEps));
    REQUIRE_THAT(reloaded.camera.position.y,  WithinAbs(5.f,   kEps));
    REQUIRE_THAT(reloaded.camera.position.z,  WithinAbs(6.f,   kEps));
    REQUIRE_THAT(reloaded.camera.focalLength, WithinAbs(100.f, kEps));
    REQUIRE_THAT(reloaded.hdri.rotation.y,    WithinAbs(45.f,  kEps));
    // pitch was in the original file and is preserved (read-modify-write)
    REQUIRE_THAT(reloaded.camera.pitch, WithinAbs(15.f, kEps));
}

TEST_CASE("saveConfig creates file if absent") {
    std::string path = "/tmp/kodak_test_newfile.json";
    std::remove(path.c_str());

    AppConfig cfg;
    saveConfig(cfg, path);

    bool exists = std::ifstream(path).is_open();
    std::remove(path.c_str());
    REQUIRE(exists);
}
