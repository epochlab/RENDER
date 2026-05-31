#version 330 core

in vec2 vUV;

uniform sampler2D uFrame;
uniform sampler2D uAO;    // blurred SSAO result (unit 1)
uniform sampler2D uDepth; // G-buffer depth (unit 2) — used to mask sky in AO channel
uniform int       uViewMode;

out vec4 FragColor;

void main() {
    if (uViewMode == 13) {
        // Black for sky pixels (depth at far plane), AO value for geometry.
        float depth = texture(uDepth, vUV).r;
        float ao    = (depth >= 0.9999) ? 0.0 : texture(uAO, vUV).r;
        FragColor   = vec4(ao, ao, ao, 1.0);
    } else {
        vec3  color = texture(uFrame, vUV).rgb;
        if (uViewMode == 1) {
            float depth = texture(uDepth, vUV).r;
            float ao    = (depth >= 0.9999) ? 1.0 : texture(uAO, vUV).r;
            color *= ao;
        }
        FragColor = vec4(color, 1.0);
    }
}
