#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdint>
#include <algorithm>

// CPU replicas of the math embedded in src/main.cpp and src/ui/hud.cpp.
// No GL context required.

using Catch::Matchers::WithinAbs;

// ── EMA FPS (main.cpp line ~578) ─────────────────────────────────────────────
// smooth = (smooth==0) ? raw : smooth*0.9 + raw*0.1

static float emaFps(float smooth, float raw) {
    return (smooth == 0.0f) ? raw : smooth * 0.9f + raw * 0.1f;
}

TEST_CASE("EMA FPS: cold start returns raw value") {
    REQUIRE_THAT(emaFps(0.0f, 60.0f), WithinAbs(60.0f, 1e-5f));
}

TEST_CASE("EMA FPS: stable signal stays constant") {
    float s = 60.0f;
    for (int i = 0; i < 100; ++i) s = emaFps(s, 60.0f);
    REQUIRE_THAT(s, WithinAbs(60.0f, 1e-3f));
}

TEST_CASE("EMA FPS: spike is dampened") {
    // smooth=60, one spike frame raw=1 → 60*0.9+1*0.1=54.1
    float s = emaFps(60.0f, 1.0f);
    REQUIRE_THAT(s, WithinAbs(54.1f, 1e-3f));
    REQUIRE(s > 10.0f);  // spike not dominating
}

TEST_CASE("EMA FPS: converges toward new stable value") {
    float s = 60.0f;
    for (int i = 0; i < 200; ++i) s = emaFps(s, 30.0f);
    REQUIRE_THAT(s, WithinAbs(30.0f, 0.01f));
}

// ── HDRI Euler rotation matrix (main.cpp lines ~341–344) ─────────────────────
// mat3(Rz * Ry * Rx) where angles are in degrees

static glm::mat3 hdriRotMat(float rxDeg, float ryDeg, float rzDeg) {
    float rx = glm::radians(rxDeg);
    float ry = glm::radians(ryDeg);
    float rz = glm::radians(rzDeg);
    return glm::mat3(
        glm::rotate(glm::mat4(1.f), rz, glm::vec3(0,0,1)) *
        glm::rotate(glm::mat4(1.f), ry, glm::vec3(0,1,0)) *
        glm::rotate(glm::mat4(1.f), rx, glm::vec3(1,0,0)));
}

TEST_CASE("HDRI rotation: identity (0,0,0) leaves vectors unchanged") {
    glm::mat3 R = hdriRotMat(0,0,0);
    glm::vec3 v = R * glm::vec3(1,0,0);
    REQUIRE_THAT(v.x, WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(v.y, WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(v.z, WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("HDRI rotation: Rx(90°) maps (0,1,0) → (0,0,1)") {
    glm::mat3 R = hdriRotMat(90,0,0);
    glm::vec3 v = R * glm::vec3(0,1,0);
    REQUIRE_THAT(v.x, WithinAbs(0.0f, 1e-4f));
    REQUIRE_THAT(v.y, WithinAbs(0.0f, 1e-4f));
    REQUIRE_THAT(v.z, WithinAbs(1.0f, 1e-4f));
}

TEST_CASE("HDRI rotation: Ry(90°) maps (1,0,0) → (0,0,-1)") {
    glm::mat3 R = hdriRotMat(0,90,0);
    glm::vec3 v = R * glm::vec3(1,0,0);
    REQUIRE_THAT(v.x, WithinAbs( 0.0f, 1e-4f));
    REQUIRE_THAT(v.y, WithinAbs( 0.0f, 1e-4f));
    REQUIRE_THAT(v.z, WithinAbs(-1.0f, 1e-4f));
}

TEST_CASE("HDRI rotation: result matrix is orthonormal") {
    glm::mat3 R = hdriRotMat(30, 45, 60);
    glm::vec3 c0(R[0]), c1(R[1]), c2(R[2]);
    REQUIRE_THAT(glm::length(c0), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(glm::length(c1), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(glm::length(c2), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(glm::dot(c0, c1), WithinAbs(0.0f, 1e-5f));
}

// ── Triangle kernel smoothing (hud.cpp lines ~228–238) ───────────────────────
// Weighted 9-tap convolution; taps outside [1,254] are skipped.

static float smoothBin(int b, const uint32_t* hist, uint32_t peak, int radius = 4) {
    float sum = 0.0f, wsum = 0.0f;
    for (int k = b - radius; k <= b + radius; ++k) {
        if (k < 1 || k > 254) continue;
        float w   = float(radius + 1 - std::abs(k - b));
        float val = std::sqrt(float(std::min(hist[k], peak)) / float(peak));
        sum  += w * val;
        wsum += w;
    }
    return wsum > 0.0f ? sum / wsum : 0.0f;
}

TEST_CASE("Histogram triangle kernel: impulse at bin 2 with radius=4") {
    uint32_t hist[256] = {};
    hist[2] = 1;
    // Taps at bin 2 (b=2): k ∈ [-2,6] ∩ [1,254] → {1,2,3,4,5,6}
    // k=2: w=5, contribution=5*1=5; others: 0.
    // wsum = 4+5+4+3+2+1 = 19
    REQUIRE_THAT(smoothBin(2, hist, 1, 4), WithinAbs(5.0f/19.0f, 1e-5f));
}

TEST_CASE("Histogram triangle kernel: impulse at bin 2, query at bin 50 → 0") {
    uint32_t hist[256] = {};
    hist[2] = 1;
    REQUIRE_THAT(smoothBin(50, hist, 1, 4), WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("Histogram triangle kernel: plateau → 1.0 at any interior bin") {
    uint32_t hist[256];
    for (int i = 0; i < 256; ++i) hist[i] = 100;
    // All taps = sqrt(100/100) = 1.0; weighted average = 1.0
    REQUIRE_THAT(smoothBin(128, hist, 100, 4), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(smoothBin( 10, hist, 100, 4), WithinAbs(1.0f, 1e-5f));
}

// ── Sqrt histogram normalisation ──────────────────────────────────────────────

static float sqrtNorm(uint32_t count, uint32_t peak) {
    return std::sqrt(float(count) / float(peak));
}

TEST_CASE("Histogram sqrt: count==peak → 1.0") {
    REQUIRE_THAT(sqrtNorm(1000, 1000), WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("Histogram sqrt: count==0 → 0.0") {
    REQUIRE_THAT(sqrtNorm(0, 1000), WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("Histogram sqrt: count==peak/4 → 0.5") {
    REQUIRE_THAT(sqrtNorm(250, 1000), WithinAbs(0.5f, 1e-5f));
}

// ── Grayscale detection (hud.cpp line ~211) ───────────────────────────────────

static bool isGrayscale(const uint32_t h[3][256]) {
    for (int b = 0; b < 256; ++b)
        if (h[0][b] != h[1][b] || h[1][b] != h[2][b]) return false;
    return true;
}

TEST_CASE("Histogram grayscale detection: identical channels → true") {
    uint32_t h[3][256] = {};
    for (int b = 0; b < 256; ++b) h[0][b] = h[1][b] = h[2][b] = uint32_t(b * 10);
    REQUIRE(isGrayscale(h) == true);
}

TEST_CASE("Histogram grayscale detection: channel 1 differs → false") {
    uint32_t h[3][256] = {};
    h[1][100] = 1;
    REQUIRE(isGrayscale(h) == false);
}

TEST_CASE("Histogram grayscale detection: all zeros → true") {
    uint32_t h[3][256] = {};
    REQUIRE(isGrayscale(h) == true);
}

// ── Near-binary detection (hud.cpp line ~258) ─────────────────────────────────
// interiorTotal = sum(hist[0][1..254]); nearBinary iff interiorTotal < 256*144/100 = 368

static bool isNearBinary(const uint32_t hist[256]) {
    uint32_t interior = 0;
    for (int b = 1; b < 255; ++b) interior += hist[b];
    return interior < static_cast<uint32_t>(256 * 144 / 100);  // < 368
}

TEST_CASE("Near-binary: interiorTotal=0 → true") {
    uint32_t h[256] = {};
    REQUIRE(isNearBinary(h) == true);
}

TEST_CASE("Near-binary: interiorTotal=367 → true (just below threshold)") {
    uint32_t h[256] = {};
    h[1] = 367;
    REQUIRE(isNearBinary(h) == true);
}

TEST_CASE("Near-binary: interiorTotal=368 → false (at threshold)") {
    uint32_t h[256] = {};
    h[1] = 368;
    REQUIRE(isNearBinary(h) == false);
}

TEST_CASE("Near-binary: interiorTotal=369 → false") {
    uint32_t h[256] = {};
    h[1] = 369;
    REQUIRE(isNearBinary(h) == false);
}

// ── Frame-time min/max (hud.cpp lines ~62–68) ────────────────────────────────

static std::pair<float,float> frameTimeMinMax(const float* fps, int n) {
    float tMin = 1e9f, tMax = 0.0f;
    for (int i = 0; i < n; ++i) {
        if (fps[i] <= 0.0f) continue;
        float ms = 1000.0f / fps[i];
        if (ms < tMin) tMin = ms;
        if (ms > tMax) tMax = ms;
    }
    return {tMin, tMax};
}

TEST_CASE("Frame-time min/max: [60,120,30] → min≈8.33 max≈33.33") {
    float fps[] = {60.0f, 120.0f, 30.0f};
    auto [mn, mx] = frameTimeMinMax(fps, 3);
    REQUIRE_THAT(mn, WithinAbs(1000.0f/120.0f, 1e-3f));
    REQUIRE_THAT(mx, WithinAbs(1000.0f/30.0f,  1e-3f));
}

TEST_CASE("Frame-time min/max: zeros are skipped") {
    float fps[] = {0.0f, 0.0f, 60.0f};
    auto [mn, mx] = frameTimeMinMax(fps, 3);
    REQUIRE_THAT(mn, WithinAbs(1000.0f/60.0f, 1e-3f));
    REQUIRE_THAT(mx, WithinAbs(1000.0f/60.0f, 1e-3f));
}

TEST_CASE("Frame-time min/max: single entry") {
    float fps[] = {90.0f};
    auto [mn, mx] = frameTimeMinMax(fps, 1);
    REQUIRE_THAT(mn, WithinAbs(1000.0f/90.0f, 1e-3f));
    REQUIRE_THAT(mx, WithinAbs(1000.0f/90.0f, 1e-3f));
}
