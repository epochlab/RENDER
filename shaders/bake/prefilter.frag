#version 330 core
in vec2 vUV;

uniform sampler2D uHdriTex;
uniform mat3      uHdriRotMat;
uniform float     uHdriExposure;
uniform bool      uHdriFlipV;
uniform int       uSamples;
uniform float     uRoughness;

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
    // Split-sum approximation: n = v = r, so lobe centre stays at r for all roughness.
    vec3  r = normalize(uvToDir(vUV));
    float a = uRoughness * uRoughness;

    vec3 up = abs(r.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 T  = normalize(cross(up, r));
    vec3 B  = cross(r, T);

    vec3 acc = vec3(0.0);
    for (int i = 0; i < uSamples; i++) {
        float u     = (float(i) + 0.5) / float(uSamples);
        float phi   = PHI * float(i);
        float denom = max(1.0 + (a * a - 1.0) * u, 1e-6);
        float cosT  = sqrt((1.0 - u) / denom);
        float sinT  = sqrt(max(0.0, 1.0 - cosT * cosT));
        vec3  local = vec3(sinT * cos(phi), sinT * sin(phi), cosT);
        vec3  world = normalize(T * local.x + B * local.y + r * local.z);
        acc += sampleHdri(world);
    }
    fragColor = acc / float(uSamples);
}
