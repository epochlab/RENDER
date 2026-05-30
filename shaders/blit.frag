#version 330 core

in vec2 vUV;

uniform sampler2D uFrame;
uniform sampler2D uAO;      // blurred SSAO result (unit 1)
uniform int       uViewMode;

out vec4 FragColor;

void main() {
    vec3  color = texture(uFrame, vUV).rgb;
    float ao    = texture(uAO,    vUV).r;

    if (uViewMode == 9) {
        FragColor = vec4(ao, ao, ao, 1.0);
    } else if (uViewMode == 1) {
        FragColor = vec4(color * ao, 1.0);
    } else {
        FragColor = vec4(color, 1.0);
    }
}
