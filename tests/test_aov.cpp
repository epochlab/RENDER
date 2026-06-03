#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "gl_context.hpp"
#include "render_harness.hpp"

// Pixel readback tests for all 16 AOV view modes.
//
// Controlled inputs (set up in RenderHarness):
//   vNormal=(0,0,1)  vFragPos=(0,0,0)  vUV=(0.5,0.5)
//   uCamPos=(0,0,5) → viewDir=(0,0,1) → NdotV=1
//   White 1×1 HDRI → irradianceIBL = reflectionIBL = (1,1,1)
//   White albedo, flat normal map, roughness=0, metallic=0, IOR=1.5
//   uBoundsMin=(-1,-1,-1) uBoundsMax=(1,1,1), near=0.1, far=100
//
// Tolerance 0.01 covers 1/255 ≈ 0.004 rounding in GL_RGB8 readback.

TEST_CASE("AOV render modes: pixel readback for all 16 view modes") {
    GlContext ctx;
    RenderHarness h;
    using Catch::Matchers::WithinAbs;
    static constexpr float kTol = 0.01f;

    // Helper: check all three components
    auto check = [&](glm::vec3 px, float er, float eg, float eb) {
        CHECK_THAT(px.r, WithinAbs(er, kTol));
        CHECK_THAT(px.g, WithinAbs(eg, kTol));
        CHECK_THAT(px.b, WithinAbs(eb, kTol));
    };

    SECTION("mode 1  beauty: Ld+Ls=1 with white IBL + energy conservation") {
        // F0=0.04, Ld=0.96, Ls=0.04; AO=1 (white aoTex); beauty=(1,1,1)
        check(h.renderMode(1), 1.0f, 1.0f, 1.0f);
    }

    SECTION("mode 2  alpha: white albedo alpha=1") {
        check(h.renderMode(2), 1.0f, 1.0f, 1.0f);
    }

    SECTION("mode 3  bounds: hardcoded grey 0.25") {
        // 0.25 × 255 = 63.75 → byte 64 → 64/255 ≈ 0.251
        check(h.renderMode(3), 0.251f, 0.251f, 0.251f);
    }

    SECTION("mode 4  wireframe: hardcoded white") {
        check(h.renderMode(4), 1.0f, 1.0f, 1.0f);
    }

    SECTION("mode 5  depth: NDC z=0 → gl_FragCoord.z=0.5 → d≈0.001 → byte 0") {
        // z_ndc=0, ld=2*0.1*100/(100.1)≈0.1998, d=(0.1998-0.1)/99.9≈0.001
        glm::vec3 px = h.renderMode(5);
        CHECK_THAT(px.r, WithinAbs(0.0f, kTol));
        CHECK_THAT(px.g, WithinAbs(0.0f, kTol));
        CHECK_THAT(px.b, WithinAbs(0.0f, kTol));
    }

    SECTION("mode 6  albedo: white texture") {
        check(h.renderMode(6), 1.0f, 1.0f, 1.0f);
    }

    SECTION("mode 7  hsv: white → (H=0,S=0,V=1) → output (0,0,1) = blue") {
        // blit.frag applies HSV conversion; achromatic white has S=0, V=1
        check(h.renderMode(7), 0.0f, 0.0f, 1.0f);
    }

    SECTION("mode 8  luminance: 0.2126+0.7152+0.0722=1.0 → white") {
        check(h.renderMode(8), 1.0f, 1.0f, 1.0f);
    }

    SECTION("mode 9  direct_diffuse: irradianceIBL with white HDRI = (1,1,1)") {
        check(h.renderMode(9), 1.0f, 1.0f, 1.0f);
    }

    SECTION("mode 10  direct_refl: fresnelWeighted(F0=0.04, NdotV=1, r=0) × white IBL") {
        // fresnelWeighted = 0.04; byte 10/255 ≈ 0.039
        check(h.renderMode(10), 0.039f, 0.039f, 0.039f);
    }

    SECTION("mode 11  world_pos: (0,0,0) in [-1,1]^3 bounds → (0.5,0.5,0.5)") {
        // 128/255 ≈ 0.502
        check(h.renderMode(11), 0.502f, 0.502f, 0.502f);
    }

    SECTION("mode 12  world_normals: vNormal=(0,0,1) → (0.5,0.5,1.0)") {
        // R=G=128/255≈0.502, B=255/255=1.0
        check(h.renderMode(12), 0.502f, 0.502f, 1.0f);
    }

    SECTION("mode 13  uv: fract(0.5,0.5) → (0.5,0.5,0)") {
        check(h.renderMode(13), 0.502f, 0.502f, 0.0f);
    }

    SECTION("mode 14  shading_normal: flat normal map → (0,0,1) → same encoding as mode 12") {
        check(h.renderMode(14), 0.502f, 0.502f, 1.0f);
    }

    SECTION("mode 15  fresnel: F_scalar=0.04; mix(red,green,0.04)=(0.96,0.04,0)") {
        // R=245/255≈0.961, G=10/255≈0.039, B=0
        check(h.renderMode(15), 0.961f, 0.039f, 0.0f);
    }

    SECTION("mode 16  occlusion: depth=0.5<0.9999; aoTex=white → ao=1 → (1,1,1)") {
        check(h.renderMode(16), 1.0f, 1.0f, 1.0f);
    }
}
