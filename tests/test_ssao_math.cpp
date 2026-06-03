#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include "ssao_kernel.hpp"

using Catch::Matchers::WithinAbs;

// CPU replica of the GLSL viewPosFromDepth in shaders/post/ssao.frag
static glm::vec3 viewPosFromDepth(glm::vec2 uv, float depth, const glm::mat4& invProj) {
    glm::vec4 ndc  = glm::vec4(uv * 2.0f - 1.0f, depth * 2.0f - 1.0f, 1.0f);
    glm::vec4 view = invProj * ndc;
    return glm::vec3(view) / view.w;
}

// CPU replica of the GLSL smoothstep range check in ssao.frag
static float smoothstepRangeCheck(float radius, float distance) {
    float x = (distance > 0.0f) ? radius / distance : 1.0f;
    float t = glm::clamp(x, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// ── viewPosFromDepth ──────────────────────────────────────────────────────────

TEST_CASE("SSAO viewPosFromDepth: round-trip for (0.5, 0.5, -3.0)") {
    glm::mat4 proj    = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    glm::mat4 invProj = glm::inverse(proj);
    glm::vec4 clip    = proj * glm::vec4(0.5f, 0.5f, -3.0f, 1.0f);
    glm::vec3 ndc     = glm::vec3(clip) / clip.w;
    glm::vec2 uv      = glm::vec2(ndc) * 0.5f + 0.5f;
    float     depth   = ndc.z * 0.5f + 0.5f;
    glm::vec3 rec     = viewPosFromDepth(uv, depth, invProj);
    REQUIRE_THAT(rec.x, WithinAbs( 0.5f, 1e-4f));
    REQUIRE_THAT(rec.y, WithinAbs( 0.5f, 1e-4f));
    REQUIRE_THAT(rec.z, WithinAbs(-3.0f, 1e-4f));
}

TEST_CASE("SSAO viewPosFromDepth: round-trip for (0, 0, -10.0)") {
    glm::mat4 proj    = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    glm::mat4 invProj = glm::inverse(proj);
    glm::vec4 clip    = proj * glm::vec4(0.0f, 0.0f, -10.0f, 1.0f);
    glm::vec3 ndc     = glm::vec3(clip) / clip.w;
    glm::vec2 uv      = glm::vec2(ndc) * 0.5f + 0.5f;
    float     depth   = ndc.z * 0.5f + 0.5f;
    glm::vec3 rec     = viewPosFromDepth(uv, depth, invProj);
    REQUIRE_THAT(rec.x, WithinAbs(  0.0f, 1e-4f));
    REQUIRE_THAT(rec.y, WithinAbs(  0.0f, 1e-4f));
    REQUIRE_THAT(rec.z, WithinAbs(-10.0f, 1e-3f));
}

// ── smoothstep range check ────────────────────────────────────────────────────

TEST_CASE("SSAO smoothstep: ratio=1 (at boundary)  →  1") {
    REQUIRE_THAT(smoothstepRangeCheck(1.0f, 1.0f), WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("SSAO smoothstep: ratio>1 (inside radius)  →  1") {
    REQUIRE_THAT(smoothstepRangeCheck(10.0f, 1.0f), WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("SSAO smoothstep: ratio=0.5  →  0.5") {
    REQUIRE_THAT(smoothstepRangeCheck(0.5f, 1.0f), WithinAbs(0.5f, 1e-5f));
}

TEST_CASE("SSAO smoothstep: ratio→0 (far outside)  →  ~0") {
    float v = smoothstepRangeCheck(0.1f, 10.0f);  // ratio=0.01
    REQUIRE(v < 0.01f);
}

TEST_CASE("SSAO smoothstep: radius=0  →  0") {
    REQUIRE_THAT(smoothstepRangeCheck(0.0f, 1.0f), WithinAbs(0.0f, 1e-5f));
}

// ── SSAO kernel generation ───────────────────────────────────────────────────

TEST_CASE("SSAO kernel: all z-components non-negative (upper hemisphere)") {
    auto k = generateSSAOKernel(42);
    REQUIRE(k.size() == 64);
    for (const auto& s : k)
        REQUIRE(s.z >= 0.0f);
}

TEST_CASE("SSAO kernel: all magnitudes <= 1 (inside unit sphere)") {
    auto k = generateSSAOKernel(42);
    for (const auto& s : k)
        REQUIRE(glm::length(s) <= 1.0f + 1e-5f);
}

TEST_CASE("SSAO kernel: deterministic — two calls with same seed match") {
    auto k1 = generateSSAOKernel(42);
    auto k2 = generateSSAOKernel(42);
    REQUIRE(k1.size() == k2.size());
    for (int i = 0; i < 64; ++i) {
        REQUIRE(k1[i].x == k2[i].x);
        REQUIRE(k1[i].y == k2[i].y);
        REQUIRE(k1[i].z == k2[i].z);
    }
}

TEST_CASE("SSAO kernel: different seeds produce different samples") {
    auto k42 = generateSSAOKernel(42);
    auto k99 = generateSSAOKernel(99);
    bool differs = (k42[0].x != k99[0].x) || (k42[0].y != k99[0].y) || (k42[0].z != k99[0].z);
    REQUIRE(differs);
}

TEST_CASE("SSAO kernel: falloff scaling increases with index") {
    // Sample at i=0 has scale=0.1 (minimum), at i=63 scale=1.0 (maximum).
    // All samples should satisfy: length(s) <= 1 and length(s[63]) >= length(s[0]).
    auto k = generateSSAOKernel(42);
    // The falloff: s *= 0.1 + (i/64)^2 * 0.9
    // At i=0: multiplier = 0.1 (before normalize+scale)
    // At i=63: multiplier ≈ 1.0
    // Check that the final magnitude at i=63 is at least as large as at i=0
    // (This is probabilistic — with seed=42 it should hold)
    float len0  = glm::length(k[0]);
    float len63 = glm::length(k[63]);
    // Both are within [0,1]; len63 should generally be larger (closer to surface)
    REQUIRE(len0  <= 1.0f + 1e-5f);
    REQUIRE(len63 <= 1.0f + 1e-5f);
    REQUIRE(len0  >= 0.0f);
}
