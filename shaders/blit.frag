#version 330 core
in vec2 vUV;
uniform sampler2D uFrame;
out vec4 FragColor;
void main() {
    vec3 hdr = texture(uFrame, vUV).rgb;
    vec3 ldr = hdr / (hdr + vec3(1.0));  // Reinhard tonemapping
    FragColor = vec4(ldr, 1.0);
}
