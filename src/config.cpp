#include "config.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

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

AppConfig loadConfig(const std::string& path) {
    AppConfig cfg;
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cout << "profile.json not found — using defaults\n";
        return cfg;
    }

    json j;
    try { f >> j; }
    catch (const json::exception& e) {
        std::cerr << "profile.json parse error: " << e.what() << " — using defaults\n";
        return cfg;
    }

    if (j.contains("camera")) {
        const auto& c = j["camera"];
        cfg.camera.position    = jvec3(c.value("position", json::array({0,1,10})), cfg.camera.position);
        cfg.camera.yaw         = jf(c.value("yaw",          json(-90.f)), cfg.camera.yaw);
        cfg.camera.pitch       = jf(c.value("pitch",        json(0.f)),   cfg.camera.pitch);
        cfg.camera.near        = jf(c.value("near",         json(0.1f)),  cfg.camera.near);
        cfg.camera.far         = jf(c.value("far",          json(100.f)), cfg.camera.far);
        cfg.camera.filmback    = jf(c.value("filmback",     json(35.f)),  cfg.camera.filmback);
        cfg.camera.focalLength = jf(c.value("focalLength",  json(50.f)),  cfg.camera.focalLength);
    }

    if (j.contains("render")) {
        const auto& r = j["render"];
        cfg.render.scale = ji(r.value("scale", json(2)), cfg.render.scale);
    }

    if (j.contains("hdri")) {
        const auto& h = j["hdri"];
        if (h.contains("path") && h["path"].is_string()) cfg.hdri.path = h["path"].get<std::string>();
        cfg.hdri.rotation = jvec3(h.value("rotation", json::array({0,0,0})), cfg.hdri.rotation);
        cfg.hdri.visible  = jb(h.value("visible",  json(true)),  cfg.hdri.visible);
        cfg.hdri.exposure = jf(h.value("exposure", json(1.f)),   cfg.hdri.exposure);
    }

    if (j.contains("scene")) {
        const auto& s = j["scene"];
        if (s.contains("geometry") && s["geometry"].is_string())
            cfg.scene.geometry = s["geometry"].get<std::string>();
        cfg.scene.rotation = jvec3(s.value("rotation", json::array({0,0,0})), cfg.scene.rotation);
    }

    return cfg;
}
