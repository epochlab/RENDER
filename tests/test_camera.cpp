#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include "camera.hpp"

using Catch::Matchers::WithinAbs;
static constexpr float kEps = 1e-4f;

// glm stores matrices column-major: M[col][row].
// glm::perspective puts 1/tan(vFov/2) at P[1][1].

TEST_CASE("Camera projection: filmback=35 focal=70 aspect=1  →  P[1][1]==4") {
    // hFov = 2*atan(35/140) = 2*atan(0.25), tan(hFov/2) = 0.25, f = 4.0
    Camera cam({0,0,0}, 1.0f, 35.f, 70.f);
    glm::mat4 P = cam.projectionMatrix();
    REQUIRE_THAT(P[1][1], WithinAbs(4.0f, kEps));
    // square aspect → P[0][0] == P[1][1]
    REQUIRE_THAT(P[0][0], WithinAbs(4.0f, kEps));
}

TEST_CASE("Camera projection: filmback=36 focal=36 aspect=1  →  P[1][1]==2") {
    // tan(hFov/2) = 36/72 = 0.5, f = 2.0
    Camera cam({0,0,0}, 1.0f, 36.f, 36.f);
    glm::mat4 P = cam.projectionMatrix();
    REQUIRE_THAT(P[1][1], WithinAbs(2.0f, kEps));
}

TEST_CASE("Camera projection: filmback=35 focal=50 aspect=16/9  →  P[1][1]≈5.0794") {
    // hFov: tan(h/2)=0.35, vFov: tan(v/2)=0.35/(16/9)=0.196875, f=1/0.196875≈5.0794
    Camera cam({0,0,0}, 16.f/9.f, 35.f, 50.f);
    glm::mat4 P = cam.projectionMatrix();
    REQUIRE_THAT(P[1][1], WithinAbs(5.0794f, 1e-3f));
}

TEST_CASE("Camera projection: near/far depth terms") {
    // near=0.1, far=100 → P[2][2]=-(far+near)/(far-near), P[3][2]=-2*far*near/(far-near)
    Camera cam({0,0,0}, 1.0f, 35.f, 70.f, 0.1f, 100.f);
    glm::mat4 P = cam.projectionMatrix();
    float expected22 = -(100.f + 0.1f) / (100.f - 0.1f);  // ≈ -1.002002
    float expected32 = -2.f * 100.f * 0.1f / (100.f - 0.1f); // ≈ -0.200200
    REQUIRE_THAT(P[2][2], WithinAbs(expected22, kEps));
    REQUIRE_THAT(P[3][2], WithinAbs(expected32, kEps));
}

TEST_CASE("Camera setAspect updates P[0][0] proportionally") {
    Camera cam({0,0,0}, 1.0f, 35.f, 70.f);
    cam.setAspect(2.0f);
    glm::mat4 P = cam.projectionMatrix();
    // P[0][0] = P[1][1] / aspect
    REQUIRE_THAT(P[0][0], WithinAbs(P[1][1] / 2.0f, kEps));
}

TEST_CASE("Camera viewMatrix upper-left 3x3 is orthonormal") {
    Camera cam({0.f, 0.f, 5.f}, 1.0f);
    glm::mat4 V = cam.viewMatrix();
    glm::vec3 c0{V[0][0], V[0][1], V[0][2]};
    glm::vec3 c1{V[1][0], V[1][1], V[1][2]};
    glm::vec3 c2{V[2][0], V[2][1], V[2][2]};
    REQUIRE_THAT(glm::length(c0), WithinAbs(1.f, kEps));
    REQUIRE_THAT(glm::length(c1), WithinAbs(1.f, kEps));
    REQUIRE_THAT(glm::length(c2), WithinAbs(1.f, kEps));
    REQUIRE_THAT(glm::dot(c0, c1), WithinAbs(0.f, kEps));
    REQUIRE_THAT(glm::dot(c0, c2), WithinAbs(0.f, kEps));
    REQUIRE_THAT(glm::dot(c1, c2), WithinAbs(0.f, kEps));
}

TEST_CASE("Camera front() at default orientation (yaw=-90, pitch=0)") {
    Camera cam({0,0,0}, 1.0f);
    glm::vec3 f = cam.front();
    REQUIRE_THAT(f.x, WithinAbs( 0.f, kEps));
    REQUIRE_THAT(f.y, WithinAbs( 0.f, kEps));
    REQUIRE_THAT(f.z, WithinAbs(-1.f, kEps));
}

TEST_CASE("Camera front() at yaw=0 pitch=0  →  (1,0,0)") {
    Camera cam({0,0,0}, 1.0f);
    cam.setYaw(0.f);
    glm::vec3 f = cam.front();
    REQUIRE_THAT(f.x, WithinAbs(1.f, kEps));
    REQUIRE_THAT(f.y, WithinAbs(0.f, kEps));
    REQUIRE_THAT(f.z, WithinAbs(0.f, kEps));
}

TEST_CASE("Camera setPitch clamps to [-89, +89]") {
    Camera cam({0,0,0}, 1.0f);
    cam.setPitch(200.f);
    REQUIRE_THAT(cam.pitch(), WithinAbs(89.f, kEps));
    cam.setPitch(-200.f);
    REQUIRE_THAT(cam.pitch(), WithinAbs(-89.f, kEps));
    cam.setPitch(45.f);
    REQUIRE_THAT(cam.pitch(), WithinAbs(45.f, kEps));
}

// ── Exposure ──────────────────────────────────────────────────────────────────

TEST_CASE("Camera exposure: ISO 100 / f8 / 1/100s → 1.0", "[camera]") {
    Camera cam({0,0,0}, 1.78f);
    cam.setISO(100.f);
    cam.setFStop(8.f);
    cam.setShutterSpeed(0.01f);
    REQUIRE_THAT(cam.exposureValue(), WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("Camera exposure: double ISO doubles exposure", "[camera]") {
    Camera cam({0,0,0}, 1.78f);
    cam.setISO(100.f); cam.setFStop(8.f); cam.setShutterSpeed(0.01f);
    float e0 = cam.exposureValue();
    cam.setISO(200.f);
    REQUIRE_THAT(cam.exposureValue(), WithinAbs(2.f * e0, 1e-5f));
}

TEST_CASE("Camera exposure: double f-stop quarters exposure", "[camera]") {
    Camera cam({0,0,0}, 1.78f);
    cam.setISO(100.f); cam.setFStop(8.f); cam.setShutterSpeed(0.01f);
    float e0 = cam.exposureValue();
    cam.setFStop(16.f);
    REQUIRE_THAT(cam.exposureValue(), WithinAbs(0.25f * e0, 1e-5f));
}

TEST_CASE("Camera exposure: double shutter doubles exposure", "[camera]") {
    Camera cam({0,0,0}, 1.78f);
    cam.setISO(100.f); cam.setFStop(8.f); cam.setShutterSpeed(0.01f);
    float e0 = cam.exposureValue();
    cam.setShutterSpeed(0.02f);
    REQUIRE_THAT(cam.exposureValue(), WithinAbs(2.f * e0, 1e-5f));
}

TEST_CASE("Camera CoC scale: higher f-stop gives proportionally smaller scale", "[camera]") {
    Camera cam({0,0,0}, 1.78f, 35.f, 50.f);
    cam.setFStop(2.f);
    float wide = cam.cocScale(2048.f);
    cam.setFStop(16.f);
    float narrow = cam.cocScale(2048.f);
    // Doubling f-stop halves cocScale; here 16/2=8× difference expected.
    REQUIRE_THAT(narrow, WithinAbs(wide / 8.f, 1e-3f));
}
