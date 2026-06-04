#include "config.hpp"
#include "log.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

static float jf(const json& j, float def) {
    return j.is_number() ? j.get<float>() : def;
}
static bool jb(const json& j, bool def) {
    return j.is_boolean() ? j.get<bool>() : def;
}
static int ji(const json& j, int def) {
    return j.is_number_integer() ? j.get<int>() : def;
}
static glm::vec3 jvec3(const json& j, glm::vec3 def) {
    if (j.is_array() && j.size() == 3)
        return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
    return def;
}

static json openJson(const std::string& path, const char* label) {
    std::ifstream f(path);
    if (!f.is_open()) {
        LOG_I(std::string(label) + " not found — using defaults");
        return json{};
    }
    json j;
    try { f >> j; }
    catch (const json::exception& e) {
        LOG_W(std::string(label) + " parse error: " + e.what() + " — using defaults");
        return json{};
    }
    return j;
}

AppConfig loadConfig(const std::string& profilePath, const std::string& scenePath) {
    AppConfig cfg;

    // ── profile.json: renderer settings, camera defaults, HDRI ──
    json p = openJson(profilePath, "profile.json");

    if (p.contains("render")) {
        const auto& r = p["render"];
        cfg.render.downsample  = ji(r.value("downsample",  json(2)),     cfg.render.downsample);
        cfg.render.iblSamples  = ji(r.value("iblSamples",  json(16)),    cfg.render.iblSamples);
        cfg.render.width       = ji(r.value("width",       json(2048)),  cfg.render.width);
        cfg.render.height      = ji(r.value("height",      json(1152)),  cfg.render.height);
        cfg.render.dofEnabled  = jb(r.value("dofEnabled",  json(false)), cfg.render.dofEnabled);
        cfg.render.dofSamples  = ji(r.value("dofSamples",  json(16)),    cfg.render.dofSamples);
    }

    if (p.contains("shading")) {
        const auto& s = p["shading"];
        cfg.shading.ior            = jf(s.value("ior",            json(1.5f)),   cfg.shading.ior);
        cfg.shading.ssaoRadius     = jf(s.value("ssaoRadius",     json(0.5f)),   cfg.shading.ssaoRadius);
        cfg.shading.ssaoBias       = jf(s.value("ssaoBias",       json(0.025f)), cfg.shading.ssaoBias);
        cfg.shading.ssaoSamples    = ji(s.value("ssaoSamples",    json(64)),     cfg.shading.ssaoSamples);
        cfg.shading.ssaoBlurRadius = ji(s.value("ssaoBlurRadius", json(2)),      cfg.shading.ssaoBlurRadius);
        cfg.shading.ssaoHalfRes    = jb(s.value("ssaoHalfRes",    json(false)),  cfg.shading.ssaoHalfRes);
    }

    if (p.contains("camera")) {
        const auto& c = p["camera"];
        cfg.camera.position     = jvec3(c.value("position",     json::array({0,1,10})), cfg.camera.position);
        cfg.camera.yaw          = jf(c.value("yaw",             json(-90.f)),  cfg.camera.yaw);
        cfg.camera.pitch        = jf(c.value("pitch",           json(0.f)),    cfg.camera.pitch);
        cfg.camera.near         = jf(c.value("near",            json(0.1f)),   cfg.camera.near);
        cfg.camera.far          = jf(c.value("far",             json(100.f)),  cfg.camera.far);
        cfg.camera.filmback     = jf(c.value("filmback",        json(35.f)),   cfg.camera.filmback);
        cfg.camera.focalLength  = jf(c.value("focalLength",     json(70.f)),   cfg.camera.focalLength);
        cfg.camera.iso          = jf(c.value("iso",             json(100.f)),  cfg.camera.iso);
        cfg.camera.fStop        = jf(c.value("fStop",           json(8.f)),    cfg.camera.fStop);
        cfg.camera.shutterSpeed = jf(c.value("shutterSpeed",    json(0.01f)),  cfg.camera.shutterSpeed);
        cfg.camera.focusDist    = jf(c.value("focusDist",       json(10.f)),   cfg.camera.focusDist);
    }

    if (p.contains("hdri")) {
        const auto& h = p["hdri"];
        if (h.contains("path") && h["path"].is_string()) cfg.hdri.path = h["path"].get<std::string>();
        cfg.hdri.rotation  = jvec3(h.value("rotation",  json::array({0,0,0})), cfg.hdri.rotation);
        cfg.hdri.visible   = jb(h.value("visible",   json(true)),   cfg.hdri.visible);
        cfg.hdri.exposure  = jf(h.value("exposure",  json(0.f)),    cfg.hdri.exposure);
        cfg.hdri.intensity = jf(h.value("intensity", json(1.f)),    cfg.hdri.intensity);
        cfg.hdri.flipV     = jb(h.value("flipV",     json(false)),  cfg.hdri.flipV);
    }

    // ── scene.json: geometry path, scene rotation, shading overrides ──
    json sc = openJson(scenePath, "scene.json");

    if (sc.contains("scene")) {
        const auto& s = sc["scene"];
        if (s.contains("geometry") && s["geometry"].is_string())
            cfg.scene.geometry = s["geometry"].get<std::string>();
        cfg.scene.rotation = jvec3(s.value("rotation", json::array({0,0,0})), cfg.scene.rotation);
    }

    if (sc.contains("shading")) {
        const auto& s = sc["shading"];
        cfg.shading.roughness = jf(s.value("roughness", json(0.3f)), cfg.shading.roughness);
        cfg.shading.metallic  = jf(s.value("metallic",  json(0.0f)), cfg.shading.metallic);
    }

    return cfg;
}

void saveConfig(const AppConfig& cfg, const std::string& profilePath) {
    using ojson = nlohmann::ordered_json;

    // Read-modify-write: preserve all other fields in profile.json.
    ojson p;
    {
        std::ifstream f(profilePath);
        if (f.is_open()) {
            try { f >> p; } catch (...) {}
        }
    }

    p["camera"]["position"]     = ojson{cfg.camera.position.x, cfg.camera.position.y, cfg.camera.position.z};
    p["camera"]["focalLength"]  = cfg.camera.focalLength;
    p["camera"]["iso"]          = cfg.camera.iso;
    p["camera"]["fStop"]        = cfg.camera.fStop;
    p["camera"]["shutterSpeed"] = cfg.camera.shutterSpeed;
    p["camera"]["focusDist"]    = cfg.camera.focusDist;
    p["hdri"]["rotation"]       = ojson{cfg.hdri.rotation.x, cfg.hdri.rotation.y, cfg.hdri.rotation.z};

    std::ofstream f(profilePath);
    if (f.is_open())
        f << p.dump(2) << '\n';
    else
        LOG_E("saveConfig: could not write " + profilePath);
}
