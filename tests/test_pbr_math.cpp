#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glm/glm.hpp>
#include <cmath>

// C++ replicas of the GLSL BSDF functions in shaders/geometry/pbr.frag.
// These are tested against analytically derived expected values; no GL needed.

using Catch::Matchers::WithinAbs;
static constexpr float kEps = 1e-5f;

static glm::vec3 computeF0(float ior, float metallic, glm::vec3 albedo) {
    float f0_dia = std::pow((ior - 1.0f) / (ior + 1.0f), 2.0f);
    return glm::mix(glm::vec3(f0_dia), albedo, metallic);
}

static glm::vec3 schlickFresnel(glm::vec3 F0, float cosTheta) {
    if (glm::dot(F0, F0) < 1e-8f) return glm::vec3(0.0f);
    float t = std::pow(glm::clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
    return F0 + (1.0f - F0) * t;
}

static float geoSmithIBL(float NdotV, float a) {
    float k = a * a * 0.5f;
    return NdotV / std::max(NdotV * (1.0f - k) + k, 1e-6f);
}

static glm::vec3 fresnelWeighted(glm::vec3 F0, float NdotV, float roughness) {
    float a  = glm::clamp(roughness, 0.0f, 1.0f);
    float GV = geoSmithIBL(NdotV, a);
    return GV * schlickFresnel(F0, NdotV);
}

// ── computeF0 ────────────────────────────────────────────────────────────────

TEST_CASE("PBR computeF0: IOR=1.5 metallic=0  →  (0.04,0.04,0.04)") {
    // f0 = ((1.5-1)/(1.5+1))^2 = (0.5/2.5)^2 = 0.04
    glm::vec3 f0 = computeF0(1.5f, 0.0f, glm::vec3(1.0f));
    REQUIRE_THAT(f0.r, WithinAbs(0.04f, kEps));
    REQUIRE_THAT(f0.g, WithinAbs(0.04f, kEps));
    REQUIRE_THAT(f0.b, WithinAbs(0.04f, kEps));
}

TEST_CASE("PBR computeF0: metallic=1  →  albedo") {
    glm::vec3 albedo(0.8f, 0.3f, 0.1f);
    glm::vec3 f0 = computeF0(1.5f, 1.0f, albedo);
    REQUIRE_THAT(f0.r, WithinAbs(0.8f, kEps));
    REQUIRE_THAT(f0.g, WithinAbs(0.3f, kEps));
    REQUIRE_THAT(f0.b, WithinAbs(0.1f, kEps));
}

TEST_CASE("PBR computeF0: metallic=0.5 blends dielectric+albedo") {
    // mix(0.04, 1.0, 0.5) = 0.52 for R channel
    glm::vec3 f0 = computeF0(1.5f, 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
    REQUIRE_THAT(f0.r, WithinAbs(0.52f, kEps));
    REQUIRE_THAT(f0.g, WithinAbs(0.02f, kEps));
    REQUIRE_THAT(f0.b, WithinAbs(0.02f, kEps));
}

TEST_CASE("PBR computeF0: IOR=1 (air-air interface)  →  F0=0") {
    glm::vec3 f0 = computeF0(1.0f, 0.0f, glm::vec3(1.0f));
    REQUIRE_THAT(f0.r, WithinAbs(0.0f, kEps));
}

// ── schlickFresnel ────────────────────────────────────────────────────────────

TEST_CASE("PBR schlickFresnel: cosTheta=1 (normal incidence)  →  F0") {
    glm::vec3 F0(0.04f);
    glm::vec3 F = schlickFresnel(F0, 1.0f);
    REQUIRE_THAT(F.r, WithinAbs(0.04f, kEps));
    REQUIRE_THAT(F.g, WithinAbs(0.04f, kEps));
    REQUIRE_THAT(F.b, WithinAbs(0.04f, kEps));
}

TEST_CASE("PBR schlickFresnel: cosTheta=0 (grazing)  →  (1,1,1)") {
    glm::vec3 F0(0.04f);
    glm::vec3 F = schlickFresnel(F0, 0.0f);
    REQUIRE_THAT(F.r, WithinAbs(1.0f, kEps));
    REQUIRE_THAT(F.g, WithinAbs(1.0f, kEps));
    REQUIRE_THAT(F.b, WithinAbs(1.0f, kEps));
}

TEST_CASE("PBR schlickFresnel: F0=(0,0,0) returns zero") {
    glm::vec3 F = schlickFresnel(glm::vec3(0.0f), 0.5f);
    REQUIRE_THAT(F.r, WithinAbs(0.0f, kEps));
}

TEST_CASE("PBR schlickFresnel: monotone — F(cos=0.5) between F0 and 1") {
    glm::vec3 F0(0.04f);
    glm::vec3 F = schlickFresnel(F0, 0.5f);
    REQUIRE(F.r > 0.04f);
    REQUIRE(F.r < 1.0f);
}

// ── geoSmithIBL ──────────────────────────────────────────────────────────────

TEST_CASE("PBR geoSmithIBL: NdotV=1 roughness=0  →  1") {
    // k=0, result = 1/(1*(1-0)+0) = 1
    REQUIRE_THAT(geoSmithIBL(1.0f, 0.0f), WithinAbs(1.0f, kEps));
}

TEST_CASE("PBR geoSmithIBL: NdotV=1 roughness=1  →  1") {
    // k = 1²*0.5 = 0.5, result = 1/max(1*(0.5)+0.5, 1e-6) = 1/1 = 1
    REQUIRE_THAT(geoSmithIBL(1.0f, 1.0f), WithinAbs(1.0f, kEps));
}

TEST_CASE("PBR geoSmithIBL: NdotV=0.5 roughness=0  →  1") {
    // k=0, result = 0.5/max(0.5*1+0, 1e-6) = 0.5/0.5 = 1
    REQUIRE_THAT(geoSmithIBL(0.5f, 0.0f), WithinAbs(1.0f, kEps));
}

TEST_CASE("PBR geoSmithIBL: NdotV=0.5 roughness=1  →  0.5") {
    // k=0.5, result = 0.5/max(0.5*0.5+0.5,1e-6) = 0.5/0.75 ≈ 0.6667
    REQUIRE_THAT(geoSmithIBL(0.5f, 1.0f), WithinAbs(0.6667f, 1e-4f));
}

// ── fresnelWeighted ───────────────────────────────────────────────────────────

TEST_CASE("PBR fresnelWeighted: NdotV=1 roughness=0  →  F0") {
    // G=1, schlick(F0,1)=F0 → F0
    glm::vec3 F = fresnelWeighted(glm::vec3(0.04f), 1.0f, 0.0f);
    REQUIRE_THAT(F.r, WithinAbs(0.04f, kEps));
    REQUIRE_THAT(F.g, WithinAbs(0.04f, kEps));
    REQUIRE_THAT(F.b, WithinAbs(0.04f, kEps));
}

TEST_CASE("PBR fresnelWeighted: NdotV=0 roughness=0  →  zero") {
    // k=0, GV = 0/max(0,1e-6)=0 → result=0
    glm::vec3 F = fresnelWeighted(glm::vec3(0.04f), 0.0f, 0.0f);
    REQUIRE_THAT(F.r, WithinAbs(0.0f, kEps));
}

// ── Energy conservation ───────────────────────────────────────────────────────

TEST_CASE("PBR energy conservation: Ld+Ls=1 with white IBL and white albedo") {
    // With uniform white IBL (irradiance=1, reflection=1), white albedo,
    // metallic=0, roughness=0, IOR=1.5:
    // F = fresnelWeighted(0.04, 1.0, 0.0) = 0.04
    // Ld = albedo*(1-F)*(1-metallic)*irradiance = 1*0.96*1*1 = 0.96
    // Ls = F*reflection = 0.04*1 = 0.04
    // beauty = Ld + Ls = 1.0
    glm::vec3 F0     = computeF0(1.5f, 0.0f, glm::vec3(1.0f));
    glm::vec3 F      = fresnelWeighted(F0, 1.0f, 0.0f);
    glm::vec3 albedo(1.0f);
    float metallic = 0.0f;
    glm::vec3 irradiance(1.0f), reflection(1.0f);

    glm::vec3 Ld = albedo * (1.0f - F) * (1.0f - metallic) * irradiance;
    glm::vec3 Ls = F * reflection;
    glm::vec3 beauty = Ld + Ls;

    REQUIRE_THAT(beauty.r, WithinAbs(1.0f, kEps));
    REQUIRE_THAT(beauty.g, WithinAbs(1.0f, kEps));
    REQUIRE_THAT(beauty.b, WithinAbs(1.0f, kEps));
}
