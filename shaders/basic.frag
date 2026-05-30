#version 330 core

in vec3 vNormal;
in vec3 vFragPos;
in vec2 vUV;

uniform sampler2D uAlbedo;

out vec4 FragColor;

void main() {
    vec3 lightDir = normalize(vec3(1.0, 2.0, 1.5));
    vec3 norm     = normalize(vNormal);
    float diff    = max(dot(norm, lightDir), 0.0);
    vec3 albedo   = texture(uAlbedo, vUV).rgb;
    FragColor     = vec4(albedo * (0.18 + 0.82 * diff), 1.0);
}
