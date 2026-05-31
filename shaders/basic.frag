#version 330 core

in vec3 vNormal;
in vec3 vNormalVS;
in vec3 vTangent;
in vec3 vBitangent;
in vec3 vFragPos;
in vec2 vUV;

uniform sampler2D uAlbedo;
uniform sampler2D uSkyHDR;     // equirectangular HDRI for diffuse irradiance
uniform sampler2D uNormalMap;  // tangent-space normal map (unit 2)
uniform int       uViewMode;   // 1=beauty  2=wire    3=alpha   4=depth    5=world_pos
                               // 6=world_normals 7=uv  8=albedo  9=_diffuse 10=_refl
                               // 11=shading_normal  12=ao
uniform float     uNear;
uniform float     uFar;
uniform float     uHdriExposure;
uniform vec3      uHdriRot;    // XYZ Euler rotation in radians — must match sky shader
uniform int       uIblSamples; // hemisphere sample count (profile.json render.iblSamples)
uniform vec3      uCamPos;     // world-space camera position (for reflection vector)
uniform float     uRoughness;  // PBR roughness: 0 = mirror, 1 = fully diffuse
uniform mat4      uView;       // view matrix — used to transform shading normal for SSAO

layout(location = 0) out vec4 gColor;
layout(location = 1) out vec4 gNormal;  // view-space shading normals for SSAO

const float PI = 3.14159265358979;

vec3 rotateXYZ(vec3 v, vec3 angles) {
    float cx = cos(angles.x), sx = sin(angles.x);
    float cy = cos(angles.y), sy = sin(angles.y);
    float cz = cos(angles.z), sz = sin(angles.z);
    v = vec3(v.x, cx*v.y - sx*v.z, sx*v.y + cx*v.z);
    v = vec3(cy*v.x + sy*v.z, v.y, -sy*v.x + cy*v.z);
    v = vec3(cz*v.x - sz*v.y, sz*v.x + cz*v.y, v.z);
    return v;
}

vec3 sampleEnvDir(vec3 dir) {
    dir = rotateXYZ(dir, uHdriRot);
    float phi   = atan(dir.z, dir.x);
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    return texture(uSkyHDR, vec2(phi / (2.0 * PI) + 0.5, 1.0 - theta / PI)).rgb * uHdriExposure;
}

// TBN-perturbed shading normal from normal map.
vec3 shadingNormal() {
    vec3 nts = texture(uNormalMap, vUV).rgb * 2.0 - 1.0;
    mat3 TBN = mat3(normalize(vTangent), normalize(vBitangent), normalize(vNormal));
    return normalize(TBN * nts);
}

// Cosine-weighted Fibonacci hemisphere integration (Lambertian irradiance).
// roughness modulates the cosine distribution: 1.0 = standard Lambertian.
vec3 irradianceIBL(vec3 n, float roughness) {
    vec3 up        = abs(n.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, n));
    vec3 bitangent = cross(n, tangent);

    const float PHI = 2.3999632; // golden angle = 2π/φ²
    float exponent  = 0.5 + 0.5 * clamp(roughness, 0.0, 1.0);
    vec3 acc = vec3(0.0);
    for (int i = 0; i < uIblSamples; i++) {
        float u        = (float(i) + 0.5) / float(uIblSamples);
        float cosTheta = pow(1.0 - u, exponent);
        float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
        float phi      = PHI * float(i);
        vec3  local    = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
        vec3  world    = local.x * tangent + local.y * bitangent + local.z * n;
        acc += sampleEnvDir(world);
    }
    return acc / float(uIblSamples);
}

// GGX-lobe IBL for specular reflection.
// roughness=0 → perfect mirror. roughness=1 → cosine-weighted hemisphere on the surface
// normal (= diffuse irradiance). Lobe centre shifts from reflect dir to normal as a=r² grows.
vec3 reflectionIBL(vec3 n, vec3 v, float roughness) {
    float a   = roughness * roughness;
    vec3  r   = reflect(-v, n);
    // Shift lobe centre: reflection dir at roughness=0, surface normal at roughness=1.
    vec3  dir = normalize(mix(r, n, a));
    vec3  up  = abs(dir.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3  T   = normalize(cross(up, dir));
    vec3  B   = cross(dir, T);

    const float PHI = 2.3999632;
    vec3 acc = vec3(0.0);
    for (int i = 0; i < uIblSamples; i++) {
        float u     = (float(i) + 0.5) / float(uIblSamples);
        float phi   = PHI * float(i);
        float denom = max(1.0 + (a * a - 1.0) * u, 1e-6);
        float cosT  = sqrt((1.0 - u) / denom);
        float sinT  = sqrt(max(0.0, 1.0 - cosT * cosT));
        vec3  local = vec3(sinT * cos(phi), sinT * sin(phi), cosT);
        acc += sampleEnvDir(normalize(T * local.x + B * local.y + dir * local.z));
    }
    return acc / float(uIblSamples);
}

void main() {
    // SSAO G-buffer: shading normal (with normal map) in view space.
    gNormal = vec4(normalize(mat3(uView) * shadingNormal()) * 0.5 + 0.5, 1.0);

    if (uViewMode == 2) {
        // Wireframe
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
        // world_pos — fract gives full-range colour at any scene scale
        gColor = vec4(fract(vFragPos), 1.0);

    } else if (uViewMode == 6) {
        // world_normals — world-space vertex normals (unperturbed)
        gColor = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);

    } else if (uViewMode == 7) {
        // UV coordinates
        gColor = vec4(fract(vUV), 0.0, 1.0);

    } else if (uViewMode == 8) {
        // Albedo — raw texture, no lighting
        gColor = vec4(texture(uAlbedo, vUV).rgb, 1.0);

    } else if (uViewMode == 9) {
        // _diffuse — HDRI irradiance using shading normal, no albedo
        gColor = vec4(irradianceIBL(shadingNormal(), uRoughness), 1.0);

    } else if (uViewMode == 10) {
        // _refl — GGX-lobe IBL sample from shading normal
        vec3 viewDir = normalize(uCamPos - vFragPos);
        gColor = vec4(reflectionIBL(shadingNormal(), viewDir, uRoughness), 1.0);

    } else if (uViewMode == 11) {
        // Shading Normal — TBN-perturbed normal visualised as colour
        gColor = vec4(shadingNormal() * 0.5 + 0.5, 1.0);

    } else {
        // Mode 1 (Beauty) and mode 12 (AO, display overridden by blit.frag)
        vec3 viewDir  = normalize(uCamPos - vFragPos);
        vec3 albedo   = texture(uAlbedo, vUV).rgb;
        vec3 diffuse  = albedo * irradianceIBL(shadingNormal(), uRoughness);
        vec3 specular = reflectionIBL(shadingNormal(), viewDir, uRoughness);
        gColor = vec4(mix(specular, diffuse, uRoughness), 1.0);
    }
}
