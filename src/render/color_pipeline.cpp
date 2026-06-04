#include "color_pipeline.hpp"
#include "log.hpp"
#include <cmath>
#include <stdexcept>
#include <vector>

ColorPipeline::ColorPipeline() {
    try {
        m_config = OCIO::Config::CreateFromBuiltinConfig(
            "cg-config-v1.0.0_aces-v1.3_ocio-v2.1");
    } catch (const OCIO::Exception& e) {
        LOG_W("OCIO: built-in config unavailable — ViewLUT disabled: " +
              std::string(e.what()));
    }
}

ColorPipeline::~ColorPipeline() {
    if (m_tex) glDeleteTextures(1, &m_tex);
}

void ColorPipeline::bake(ViewLut mode, int size) {
    m_enabled = false;
    if (mode == ViewLut::Raw || !m_config) return;

    const char* display = (mode == ViewLut::ACES_sRGB)
                        ? "sRGB - Display"
                        : "Rec.1886 Rec.709 - Display";
    const char* view = "ACES 1.0 - SDR Video";

    OCIO::ConstCPUProcessorRcPtr cpu;
    try {
        auto proc = m_config->getProcessor(
            "lin_srgb", display, view, OCIO::TRANSFORM_DIR_FORWARD);
        cpu = proc->getDefaultCPUProcessor();
    } catch (const OCIO::Exception& e) {
        LOG_W("OCIO: getProcessor failed — " + std::string(e.what()));
        return;
    }

    // Build a size^3 float LUT via log2 shaper covering [-10, +10] stops.
    constexpr float L2_MIN = -10.0f;
    constexpr float L2_MAX =  10.0f;
    const size_t n = static_cast<size_t>(size);
    std::vector<float> lut(n * n * n * 3);

    for (int iz = 0; iz < size; ++iz) {
        for (int iy = 0; iy < size; ++iy) {
            for (int ix = 0; ix < size; ++ix) {
                float pixel[3] = {
                    std::pow(2.0f, L2_MIN + (ix / (size - 1.0f)) * (L2_MAX - L2_MIN)),
                    std::pow(2.0f, L2_MIN + (iy / (size - 1.0f)) * (L2_MAX - L2_MIN)),
                    std::pow(2.0f, L2_MIN + (iz / (size - 1.0f)) * (L2_MAX - L2_MIN)),
                };
                cpu->applyRGB(pixel);
                const size_t idx = (static_cast<size_t>(iz) * n * n +
                                    static_cast<size_t>(iy) * n +
                                    static_cast<size_t>(ix)) * 3;
                lut[idx + 0] = pixel[0];
                lut[idx + 1] = pixel[1];
                lut[idx + 2] = pixel[2];
            }
        }
    }

    if (!m_tex) glGenTextures(1, &m_tex);
    glBindTexture(GL_TEXTURE_3D, m_tex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB16F,
                 size, size, size, 0, GL_RGB, GL_FLOAT, lut.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_3D, 0);

    m_enabled = true;
    LOG_I("OCIO: ViewLUT baked — " + std::string(display) +
          " / " + view + "  (" + std::to_string(size) + "^3)");
}
