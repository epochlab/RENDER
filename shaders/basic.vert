#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;    // world-space normal (view modes 5, 6, irradiance)
out vec3 vNormalVS;  // view-space normal (SSAO G-buffer)
out vec3 vFragPos;
out vec2 vUV;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vFragPos   = vec3(worldPos);
    vNormal    = mat3(transpose(inverse(uModel)))          * aNormal;
    vNormalVS  = mat3(transpose(inverse(uView * uModel)))  * aNormal;
    vUV        = aUV;
    gl_Position = uProjection * uView * worldPos;
}
