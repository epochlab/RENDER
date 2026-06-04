#pragma once
#include <string>
#include <glm/glm.hpp>

struct AppConfig {
    struct Camera {
        glm::vec3 position    {0.f, 1.f, 10.f};
        float     yaw         = -90.f;
        float     pitch       =   0.f;
        float     near        =   0.1f;
        float     far         = 100.f;
        float     filmback    =  35.f;
        float     focalLength =  70.f;
        float     iso         = 100.f;
        float     fStop       =   8.f;
        float     shutterSpeed =  0.01f;
        float     focusDist   =  10.f;
    } camera;

    struct Render {
        int  downsample = 2;
        int  iblSamples = 16;
        int  width      = 2048;
        int  height     = 1152;
        bool dofEnabled = false;
        int  dofSamples = 16;   // Poisson disc taps (1–16)
    } render;

    struct Shading {
        float roughness      = 0.3f;
        float ssaoRadius     = 0.5f;
        float ssaoBias       = 0.025f;
        int   ssaoSamples    = 64;    // hemisphere kernel size (max 64)
        int   ssaoBlurRadius = 2;     // 1 = 3×3, 2 = 5×5
        bool  ssaoHalfRes    = false; // true = half-res SSAO, false = full-res
        float metallic       = 0.0f; // 0 = dielectric, 1 = metal
        float ior            = 1.5f; // index of refraction (dielectrics only)
    } shading;

    struct Hdri {
        std::string path      = "assets/hdr/HDR_111_Parking_Lot_2_Env.hdr";
        glm::vec3   rotation  {};      // XYZ Euler degrees
        bool        visible   = true;
        float       exposure  = 0.f;   // EV offset in stops applied in blit
        float       intensity = 1.f;   // light intensity baked into IBL maps
        bool        flipV     = false;
    } hdri;

    struct Scene {
        std::string geometry = "assets/geo/rock_shopk_gltf_high/Rock_shopk_High.gltf";
        glm::vec3   rotation {};   // XYZ Euler degrees applied to loaded geometry
    } scene;
};

// Returns defaults when either file is absent or a key is missing.
AppConfig loadConfig(const std::string& profilePath, const std::string& scenePath);

// Updates camera + HDRI live state in profilePath (read-modify-write).
void saveConfig(const AppConfig& cfg, const std::string& profilePath);
