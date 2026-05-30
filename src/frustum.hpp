#pragma once
#include <glm/glm.hpp>
#include <cmath>

// View-frustum culling via Gribb-Hartmann plane extraction.
// Planes are extracted from the combined projection*view matrix and stored
// in world space as ax + by + cz + d = 0, with the normal pointing inward.
struct Frustum {
    // 0:left 1:right 2:bottom 3:top 4:near 5:far
    glm::vec4 planes[6]{};

    void update(const glm::mat4& m) {
        // Rows of m (glm is column-major: m[col][row]).
        auto row = [&](int r) {
            return glm::vec4(m[0][r], m[1][r], m[2][r], m[3][r]);
        };
        const glm::vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
        planes[0] = r3 + r0;   // left
        planes[1] = r3 - r0;   // right
        planes[2] = r3 + r1;   // bottom
        planes[3] = r3 - r1;   // top
        planes[4] = r3 + r2;   // near
        planes[5] = r3 - r2;   // far
        for (auto& p : planes) {
            float len = glm::length(glm::vec3(p));
            if (len > 0.0f) p /= len;
        }
    }

    // True if the sphere is at least partially inside the frustum.
    bool testSphere(glm::vec3 centre, float radius) const {
        for (const auto& p : planes) {
            if (glm::dot(glm::vec3(p), centre) + p.w < -radius)
                return false;
        }
        return true;
    }

    // True if the axis-aligned box is at least partially inside the frustum.
    bool testAABB(glm::vec3 mn, glm::vec3 mx) const {
        for (const auto& p : planes) {
            const glm::vec3 n(p);
            // Positive vertex: the box corner furthest along the plane normal.
            glm::vec3 pv{ n.x >= 0.0f ? mx.x : mn.x,
                         n.y >= 0.0f ? mx.y : mn.y,
                         n.z >= 0.0f ? mx.z : mn.z };
            if (glm::dot(n, pv) + p.w < 0.0f)
                return false;
        }
        return true;
    }
};
