#version 330 core
in vec2 vUV;

uniform sampler2D uHdriTex;
uniform mat3      uHdriRotMat;
uniform float     uHdriExposure;
uniform bool      uHdriFlipV;
uniform int       uSamples;

layout(location = 0) out vec3 fragColor;

const float PI  = 3.14159265358979;
const float PHI = 2.3999632;

vec3 uvToDir(vec2 uv) {
    float phi   = (uv.x - 0.5) * 2.0 * PI;
    float theta = (1.0 - uv.y) * PI;
    return vec3(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi));
}

vec3 sampleHdri(vec3 dir) {
    dir = uHdriRotMat * dir;
    float phi   = atan(dir.z, dir.x);
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    float v     = uHdriFlipV ? theta / PI : 1.0 - theta / PI;
    return texture(uHdriTex, vec2(phi / (2.0 * PI) + 0.5, v)).rgb * uHdriExposure;
}

void main() {
    vec3 n  = normalize(uvToDir(vUV));
    vec3 up = abs(n.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 T  = normalize(cross(up, n));
    vec3 B  = cross(n, T);

    vec3 acc = vec3(0.0);
    for (int i = 0; i < uSamples; i++) {
        float u        = (float(i) + 0.5) / float(uSamples);
        float cosTheta = pow(1.0 - u, 0.5);
        float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
        float phi      = PHI * float(i);
        vec3  local    = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
        vec3  world    = local.x * T + local.y * B + local.z * n;
        acc += sampleHdri(world);
    }
    fragColor = acc / float(uSamples);
}
