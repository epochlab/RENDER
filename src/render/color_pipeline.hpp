#pragma once
#include <GLFW/glfw3.h>
#include <OpenColorIO/OpenColorIO.h>

namespace OCIO = OCIO_NAMESPACE;

enum class ViewLut { Raw = 0, ACES_sRGB = 1, ACES_Rec709 = 2 };

class ColorPipeline {
public:
    ColorPipeline();
    ~ColorPipeline();

    ColorPipeline(const ColorPipeline&)            = delete;
    ColorPipeline& operator=(const ColorPipeline&) = delete;

    // Bakes a new 33^3 3D LUT for the given display transform and uploads to GPU.
    // No-op (enabled() == false) when mode == Raw.
    void bake(ViewLut mode, int size = 33);

    GLuint lut_tex() const { return m_tex; }
    bool   enabled() const { return m_enabled; }

private:
    OCIO::ConstConfigRcPtr m_config;
    GLuint m_tex     = 0;
    bool   m_enabled = false;
};
