#pragma once
#include <string>
#include <glm/glm.hpp>

struct AppConfig {
    struct Camera {
        glm::vec3 position   {0.f, 1.f, 10.f};
        float     yaw        = -90.f;
        float     pitch      =   0.f;
        float     near       =   0.1f;
        float     far        = 100.f;
        float     filmback   =  35.f;
        float     focalLength=  50.f;
    } camera;

    struct Render {
        int downsample = 2;
        int iblSamples = 16;
        int width      = 2048;
        int height     = 1152;
    } render;

    struct Shading {
        float roughness      = 0.3f;
        float ssaoRadius     = 0.5f;
        float ssaoBias       = 0.025f;
        int   ssaoBlurRadius = 2;     // 1 = 3×3, 2 = 5×5
    } shading;

    struct Hdri {
        std::string path    = "assets/hdr/HDR_111_Parking_Lot_2_Env.hdr";
        glm::vec3   rotation{};      // XYZ Euler degrees
        bool        visible = true;
        float       exposure= 1.f;
    } hdri;

    struct Scene {
        std::string geometry = "assets/geo/rock_shopk_gltf_high/Rock_shopk_High.gltf";
        glm::vec3   rotation {};   // XYZ Euler degrees applied to loaded geometry
    } scene;
};

// Returns defaults when the file is absent or a key is missing.
AppConfig loadConfig(const std::string& path);

// Writes cfg back to path, preserving JSON field order.
void saveConfig(const AppConfig& cfg, const std::string& path);
