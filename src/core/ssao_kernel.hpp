#pragma once
#include <glm/glm.hpp>
#include <random>
#include <vector>

// Generates the 64-sample hemisphere kernel used by the SSAO pass.
// Deterministic: same seed produces bit-identical output on every run.
inline std::vector<glm::vec3> generateSSAOKernel(unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::vector<glm::vec3> kernel(64);
    for (int i = 0; i < 64; ++i) {
        glm::vec3 s(dist(rng)*2.0f-1.0f, dist(rng)*2.0f-1.0f, dist(rng));
        s = glm::normalize(s) * dist(rng);
        float scale = float(i) / 64.0f;
        s *= 0.1f + scale * scale * 0.9f;
        kernel[i] = s;
    }
    return kernel;
}
