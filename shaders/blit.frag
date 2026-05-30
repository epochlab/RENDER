#version 330 core

in vec2 vUV;

uniform sampler2D uFrame;
uniform sampler2D uAO;    // blurred SSAO result (unit 1)
uniform sampler2D uDepth; // G-buffer depth (unit 2) — used to mask sky in AO channel
uniform int       uViewMode;

out vec4 FragColor;

void main() {
    if (uViewMode == 10) {
        // Black for sky pixels (depth at far plane), AO value for geometry.
        float depth = texture(uDepth, vUV).r;
        float ao    = (depth >= 0.9999) ? 0.0 : texture(uAO, vUV).r;
        FragColor   = vec4(ao, ao, ao, 1.0);
    } else {
        FragColor = vec4(texture(uFrame, vUV).rgb, 1.0);
    }
}
