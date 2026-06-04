#pragma once
#include <glm/glm.hpp>
#include <random>
#include <vector>

// Generates an n-sample hemisphere kernel used by the SSAO pass (max 64).
// Deterministic: same seed+n produces bit-identical output on every run.
inline std::vector<glm::vec3> generateSSAOKernel(unsigned seed, int n = 64) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::vector<glm::vec3> kernel(n);
    for (int i = 0; i < n; ++i) {
        glm::vec3 s(dist(rng)*2.0f-1.0f, dist(rng)*2.0f-1.0f, dist(rng));
        s = glm::normalize(s) * dist(rng);
        float scale = float(i) / float(n);
        s *= 0.1f + scale * scale * 0.9f;
        kernel[i] = s;
    }
    return kernel;
}
