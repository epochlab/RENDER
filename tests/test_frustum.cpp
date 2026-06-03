#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include "frustum.hpp"

using Catch::Matchers::WithinAbs;
static constexpr float kEps = 1e-5f;

TEST_CASE("Frustum identity matrix: plane normals and w components") {
    // Identity: r0=(1,0,0,0) r1=(0,1,0,0) r2=(0,0,1,0) r3=(0,0,0,1)
    // planes[0]=left=(1,0,0,1) planes[5]=far=(0,0,-1,1), all already unit-length
    Frustum f;
    f.update(glm::mat4(1.0f));
    REQUIRE_THAT(f.planes[0].x, WithinAbs( 1.f, kEps));  // left normal x
    REQUIRE_THAT(f.planes[0].w, WithinAbs( 1.f, kEps));  // left w
    REQUIRE_THAT(f.planes[5].z, WithinAbs(-1.f, kEps));  // far normal z
    REQUIRE_THAT(f.planes[5].w, WithinAbs( 1.f, kEps));  // far w
}

TEST_CASE("Frustum identity matrix: all plane normals are unit-length") {
    Frustum f;
    f.update(glm::mat4(1.0f));
    for (const auto& p : f.planes)
        REQUIRE_THAT(glm::length(glm::vec3(p)), WithinAbs(1.f, kEps));
}

TEST_CASE("Frustum testSphere: sphere at origin is inside identity frustum") {
    Frustum f;
    f.update(glm::mat4(1.0f));
    REQUIRE(f.testSphere({0.f, 0.f, 0.f}, 0.5f) == true);
}

TEST_CASE("Frustum testSphere: sphere behind far plane is outside") {
    // far plane (0,0,-1,1): dot((0,0,-1),(0,0,5))+1 = -5+1 = -4 < -0.5
    Frustum f;
    f.update(glm::mat4(1.0f));
    REQUIRE(f.testSphere({0.f, 0.f, 5.f}, 0.5f) == false);
}

TEST_CASE("Frustum testSphere: sphere outside left plane") {
    // left plane (1,0,0,1): dot((1,0,0),(-2,0,0))+1 = -2+1 = -1 < -0.1
    Frustum f;
    f.update(glm::mat4(1.0f));
    REQUIRE(f.testSphere({-2.f, 0.f, 0.f}, 0.1f) == false);
}

TEST_CASE("Frustum testSphere: sphere straddling left plane boundary") {
    // left plane: dot=-1.5+1=-0.5, radius=0.6 → -0.5 >= -0.6 → passes
    Frustum f;
    f.update(glm::mat4(1.0f));
    REQUIRE(f.testSphere({-1.5f, 0.f, 0.f}, 0.6f) == true);
}

TEST_CASE("Frustum testSphere: sphere just outside left plane") {
    // left plane: -0.5 < -0.4 → outside
    Frustum f;
    f.update(glm::mat4(1.0f));
    REQUIRE(f.testSphere({-1.5f, 0.f, 0.f}, 0.4f) == false);
}

TEST_CASE("Frustum perspective: known-inside point passes, far-behind fails") {
    glm::mat4 proj = glm::perspective(glm::radians(60.f), 1.f, 0.1f, 100.f);
    glm::mat4 view = glm::lookAt(glm::vec3{0,0,5}, glm::vec3{0,0,0}, glm::vec3{0,1,0});
    Frustum pf;
    pf.update(proj * view);
    // Origin is 5 units in front of camera — clearly inside
    REQUIRE(pf.testSphere({0.f, 0.f, 0.f}, 0.1f) == true);
    // z=200 is behind the camera (camera looks toward -z from z=5)
    REQUIRE(pf.testSphere({0.f, 0.f, 200.f}, 0.1f) == false);
}
