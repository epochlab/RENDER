#version 330 core

in vec3 vNormal;
in vec3 vNormalVS;
in vec3 vFragPos;
in vec2 vUV;

uniform sampler2D uAlbedo;
uniform sampler2D uSkyHDR;          // equirectangular HDRI for diffuse irradiance
uniform int       uViewMode;        // 1=diffuse 2=wire 3=alpha 4=depth 5=pos 6=normals 7=uv 8=irradiance 9=ao
uniform float     uNear;
uniform float     uFar;
uniform float     uIrradianceScale; // light.exposure × light.intensity from config

layout(location = 0) out vec4 gColor;
layout(location = 1) out vec4 gNormal;  // view-space normals for SSAO

const float PI = 3.14159265358979;

vec3 sampleEnv(vec3 n) {
    float phi   = atan(n.z, n.x);
    float theta = acos(clamp(n.y, -1.0, 1.0));
    return texture(uSkyHDR, vec2(phi / (2.0 * PI) + 0.5, theta / PI)).rgb;
}

void main() {
    // Always write view-space normals for SSAO G-buffer.
    gNormal = vec4(normalize(vNormalVS) * 0.5 + 0.5, 1.0);

    if (uViewMode == 2) {
        gColor = vec4(0.35, 0.85, 1.0, 1.0);

    } else if (uViewMode == 3) {
        // Alpha channel
        gColor = vec4(vec3(texture(uAlbedo, vUV).a), 1.0);

    } else if (uViewMode == 4) {
        // Linearised depth [0,1]
        float z  = gl_FragCoord.z * 2.0 - 1.0;
        float ld = (2.0 * uNear * uFar) / (uFar + uNear - z * (uFar - uNear));
        float d  = (ld - uNear) / (uFar - uNear);
        gColor = vec4(d, d, d, 1.0);

    } else if (uViewMode == 5) {
        // World-space position
        gColor = vec4(clamp(vFragPos * 0.1 + 0.5, 0.0, 1.0), 1.0);

    } else if (uViewMode == 6) {
        // World-space normals
        gColor = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);

    } else if (uViewMode == 7) {
        // UV coordinates
        gColor = vec4(fract(vUV), 0.0, 1.0);

    } else if (uViewMode == 8) {
        // Irradiance only — HDRI diffuse, no albedo
        gColor = vec4(sampleEnv(normalize(vNormal)) * uIrradianceScale, 1.0);

    } else {
        // Mode 1 — albedo × HDRI diffuse irradiance
        vec3 albedo     = texture(uAlbedo, vUV).rgb;
        vec3 irradiance = sampleEnv(normalize(vNormal)) * uIrradianceScale;
        gColor = vec4(albedo * irradiance, 1.0);
    }
}
