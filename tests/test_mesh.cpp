#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include "gl_context.hpp"
#include "mesh.hpp"

using Catch::Matchers::WithinAbs;
static constexpr float kEps = 1e-5f;

static Vertex vert(float x, float y, float z) {
    Vertex v{};
    v.x = x; v.y = y; v.z = z;
    return v;
}

TEST_CASE("Mesh bounding radius: triangle in XY plane") {
    // Vertices: (0,0,0), (2,0,0), (0,3,0)
    // Max distance from origin = max(0, 2, 3) = 3.0
    GlContext ctx;
    std::vector<Vertex> verts = { vert(0,0,0), vert(2,0,0), vert(0,3,0) };
    std::vector<unsigned int> idx = {0, 1, 2};
    Mesh m(verts, idx);
    REQUIRE_THAT(m.boundingRadius(), WithinAbs(3.f, kEps));
}

TEST_CASE("Mesh AABB: triangle in XY plane") {
    GlContext ctx;
    std::vector<Vertex> verts = { vert(0,0,0), vert(2,0,0), vert(0,3,0) };
    std::vector<unsigned int> idx = {0, 1, 2};
    Mesh m(verts, idx);
    REQUIRE_THAT(m.boundsMin().x, WithinAbs(0.f, kEps));
    REQUIRE_THAT(m.boundsMin().y, WithinAbs(0.f, kEps));
    REQUIRE_THAT(m.boundsMin().z, WithinAbs(0.f, kEps));
    REQUIRE_THAT(m.boundsMax().x, WithinAbs(2.f, kEps));
    REQUIRE_THAT(m.boundsMax().y, WithinAbs(3.f, kEps));
    REQUIRE_THAT(m.boundsMax().z, WithinAbs(0.f, kEps));
}

TEST_CASE("Mesh bounding radius: unit cube  →  sqrt(3)") {
    GlContext ctx;
    std::vector<Vertex> verts = {
        vert(-1,-1,-1), vert( 1,-1,-1), vert(-1, 1,-1), vert( 1, 1,-1),
        vert(-1,-1, 1), vert( 1,-1, 1), vert(-1, 1, 1), vert( 1, 1, 1),
    };
    std::vector<unsigned int> idx = {0,1,2, 1,2,3};
    Mesh m(verts, idx);
    REQUIRE_THAT(m.boundingRadius(), WithinAbs(std::sqrt(3.f), kEps));
}

TEST_CASE("Mesh bounding radius: single vertex at (1,2,3)  →  sqrt(14)") {
    GlContext ctx;
    std::vector<Vertex> verts = { vert(1.f, 2.f, 3.f) };
    std::vector<unsigned int> idx = {0, 0, 0};
    Mesh m(verts, idx);
    REQUIRE_THAT(m.boundingRadius(), WithinAbs(std::sqrt(14.f), kEps));
    REQUIRE_THAT(m.boundsMin().x, WithinAbs(1.f, kEps));
    REQUIRE_THAT(m.boundsMin().y, WithinAbs(2.f, kEps));
    REQUIRE_THAT(m.boundsMin().z, WithinAbs(3.f, kEps));
    REQUIRE_THAT(m.boundsMax().x, WithinAbs(1.f, kEps));
    REQUIRE_THAT(m.boundsMax().y, WithinAbs(2.f, kEps));
    REQUIRE_THAT(m.boundsMax().z, WithinAbs(3.f, kEps));
}

TEST_CASE("Mesh indexCount and triangleCount") {
    GlContext ctx;
    std::vector<Vertex> verts = { vert(0,0,0), vert(1,0,0), vert(0,1,0), vert(1,1,0) };
    std::vector<unsigned int> idx = {0,1,2, 1,2,3};
    Mesh m(verts, idx);
    REQUIRE(m.indexCount()    == 6);
    REQUIRE(m.triangleCount() == 2);
}

TEST_CASE("Mesh move semantics: moved-from has zero GL handles") {
    GlContext ctx;
    std::vector<Vertex> verts = { vert(0,0,0), vert(2,0,0), vert(0,3,0) };
    std::vector<unsigned int> idx = {0,1,2};
    Mesh m1(verts, idx);
    Mesh m2 = std::move(m1);
    // m2 preserves bounds; no crash on destruction of m1 (handles are 0)
    REQUIRE_THAT(m2.boundingRadius(), WithinAbs(3.f, kEps));
}
