#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aTangent;  // xyz = tangent, w = handedness

uniform mat4 uModel;
uniform mat3 uNormalMatrix;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;     // world-space vertex normal
out vec3 vTangent;    // world-space tangent
out vec3 vBitangent;  // world-space bitangent
out vec3 vFragPos;
out vec2 vUV;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vFragPos  = vec3(worldPos);
    vUV       = aUV;

    vec3 N = normalize(uNormalMatrix * aNormal);
    vNormal = N;

    if (dot(aTangent.xyz, aTangent.xyz) > 1e-5) {
        vec3 T = normalize(uNormalMatrix * aTangent.xyz);
        T = normalize(T - dot(T, N) * N);  // re-orthogonalise against N
        vTangent   = T;
        vBitangent = cross(N, T) * aTangent.w;
    } else {
        // Fallback for procedural geometry with no tangent data.
        vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        vTangent   = normalize(cross(up, N));
        vBitangent = cross(N, vTangent);
    }

    gl_Position = uProjection * uView * worldPos;
}
