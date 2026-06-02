#version 330 core

in vec3 vNormal;
in vec3 vTangent;
in vec3 vBitangent;
in vec3 vFragPos;
in vec2 vUV;

uniform sampler2D uAlbedo;
uniform sampler2D uSkyHDR;     // equirectangular HDRI for diffuse irradiance
uniform sampler2D uNormalMap;  // tangent-space normal map (unit 2)
uniform int       uViewMode;   // 1=beauty  2=alpha  3=luminance  4=hsv  5=bounds  6=wireframe
                               // 7=depth  8=world_pos  9=world_normals  10=uv  11=albedo
                               // 12=direct_diffuse  13=direct_refl  14=shading_normal
                               // 15=ao  16=fresnel
uniform vec3      uBoundsMin;  // world-space AABB min corner
uniform vec3      uBoundsMax;  // world-space AABB max corner
uniform float     uNear;
uniform float     uFar;
uniform float     uHdriExposure;
uniform mat3      uHdriRotMat; // pre-built XYZ Euler rotation — must match sky shader
uniform bool      uHdriFlipV;  // flip panorama vertically — must match sky shader
uniform int       uIblSamples; // hemisphere sample count (profile.json render.iblSamples)
uniform vec3      uCamPos;     // world-space camera position (for reflection vector)
uniform float     uRoughness;  // PBR roughness: 0 = mirror, 1 = fully diffuse
uniform float     uMetallic;  // 0 = dielectric, 1 = metal
uniform float     uIOR;       // index of refraction for dielectrics (default 1.5)
uniform mat4      uView;       // view matrix — used to transform shading normal for SSAO

layout(location = 0) out vec4 gColor;
layout(location = 1) out vec4 gNormal;  // view-space shading normals for SSAO

const float PI  = 3.14159265358979;
const float PHI = 2.3999632;  // golden angle = 2π/φ²

vec3 sampleEnvDir(vec3 dir) {
    dir = uHdriRotMat * dir;
    float phi   = atan(dir.z, dir.x);
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    float v     = uHdriFlipV ? theta / PI : 1.0 - theta / PI;
    return texture(uSkyHDR, vec2(phi / (2.0 * PI) + 0.5, v)).rgb * uHdriExposure;
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
    roughness = clamp(roughness, 0.0, 1.0);
    float a   = roughness * roughness;
    vec3  r   = reflect(-v, n);
    // Shift lobe centre: reflection dir at roughness=0, surface normal at roughness=1.
    vec3  dir = normalize(mix(r, n, a));
    vec3  up  = abs(dir.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3  T   = normalize(cross(up, dir));
    vec3  B   = cross(dir, T);

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

// IOR must be > 1 for a real interface; IOR=1 → F0=0 → no reflection at any angle.
vec3 schlickFresnel(vec3 F0, float cosTheta) {
    if (dot(F0, F0) < 1e-8) return vec3(0.0);
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Smith view-side masking (IBL remap k=a²/2, Karis 2013).
// Goes to 0 at grazing — counteracts Fresnel rim on rough surfaces.
float geoSmithIBL(float NdotV, float a) {
    float k = a * a * 0.5;
    return NdotV / max(NdotV * (1.0 - k) + k, 1e-6);
}

// Effective Fresnel with geometric attenuation — use instead of raw schlickFresnel for Ls.
vec3 fresnelWeighted(vec3 F0, float NdotV, float roughness) {
    float a   = clamp(roughness, 0.0, 1.0);
    float G_V = geoSmithIBL(NdotV, a);
    return G_V * schlickFresnel(F0, NdotV);
}

void main() {
    // SSAO G-buffer: shading normal (with normal map) in view space.
    gNormal = vec4(normalize(mat3(uView) * shadingNormal()) * 0.5 + 0.5, 1.0);

    if (uViewMode == 2) {
        // Alpha channel
        gColor = vec4(vec3(texture(uAlbedo, vUV).a), 1.0);

    } else if (uViewMode == 5) {
        // Bounds — flat grey fill; bounding box drawn as a separate GL_LINES pass in main.cpp
        gColor = vec4(0.25, 0.25, 0.25, 1.0);

    } else if (uViewMode == 6) {
        // Wireframe — white so the histogram treats it as grayscale
        gColor = vec4(1.0, 1.0, 1.0, 1.0);

    } else if (uViewMode == 7) {
        // Linearised depth [0,1]
        float z  = gl_FragCoord.z * 2.0 - 1.0;
        float ld = (2.0 * uNear * uFar) / (uFar + uNear - z * (uFar - uNear));
        float d  = (ld - uNear) / (uFar - uNear);
        gColor = vec4(d, d, d, 1.0);

    } else if (uViewMode == 8) {
        // world_pos — normalized against scene AABB for a continuous colour gradient
        vec3 extent = uBoundsMax - uBoundsMin;
        gColor = vec4(clamp((vFragPos - uBoundsMin) / max(extent, vec3(1e-6)), 0.0, 1.0), 1.0);

    } else if (uViewMode == 9) {
        // world_normals — world-space vertex normals (unperturbed)
        gColor = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);

    } else if (uViewMode == 10) {
        // UV coordinates
        gColor = vec4(fract(vUV), 0.0, 1.0);

    } else if (uViewMode == 11) {
        // Albedo — raw texture, no lighting
        gColor = vec4(texture(uAlbedo, vUV).rgb, 1.0);

    } else if (uViewMode == 12) {
        // _diffuse — full irradiance, no albedo, no Fresnel mask
        gColor = vec4(irradianceIBL(shadingNormal(), uRoughness), 1.0);

    } else if (uViewMode == 13) {
        // _refl — geometry-attenuated Fresnel specular lobe
        vec3  n       = shadingNormal();
        vec3  viewDir = normalize(uCamPos - vFragPos);
        float NoV     = max(dot(n, viewDir), 0.0);
        vec3  albedo  = texture(uAlbedo, vUV).rgb;
        float f0Dia   = pow((uIOR - 1.0) / (uIOR + 1.0), 2.0);
        vec3  F0      = mix(vec3(f0Dia), albedo, uMetallic);
        vec3  F       = fresnelWeighted(F0, NoV, uRoughness);
        gColor = vec4(F * reflectionIBL(n, viewDir, uRoughness), 1.0);

    } else if (uViewMode == 14) {
        // Shading Normal — TBN-perturbed normal visualised as colour
        gColor = vec4(shadingNormal() * 0.5 + 0.5, 1.0);

    } else if (uViewMode == 16) {
        // fresnel — geometry-attenuated F term mapped red (0) → green (1)
        vec3  n       = shadingNormal();
        vec3  viewDir = normalize(uCamPos - vFragPos);
        float NoV     = max(dot(n, viewDir), 0.0);
        vec3  albedo  = texture(uAlbedo, vUV).rgb;
        float f0Dia   = pow((uIOR - 1.0) / (uIOR + 1.0), 2.0);
        vec3  F0      = mix(vec3(f0Dia), albedo, uMetallic);
        vec3  F       = fresnelWeighted(F0, NoV, uRoughness);
        float fVal    = (F.r + F.g + F.b) / 3.0;
        gColor = vec4(mix(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), fVal), 1.0);

    } else {
        // Mode 1 (Beauty), 3 (luminance), 4 (hsv), 15 (AO) — display overridden by blit.frag
        vec3  viewDir      = normalize(uCamPos - vFragPos);
        vec3  albedo       = texture(uAlbedo, vUV).rgb;
        vec3  n            = shadingNormal();
        float NoV          = max(dot(n, viewDir), 0.0);
        float f0Dielectric = pow((uIOR - 1.0) / (uIOR + 1.0), 2.0);
        vec3  F0           = mix(vec3(f0Dielectric), albedo, uMetallic);
        vec3  F            = fresnelWeighted(F0, NoV, uRoughness);
        vec3  Ld = albedo * (1.0 - F) * (1.0 - uMetallic) * irradianceIBL(n, uRoughness);
        vec3  Ls = F * reflectionIBL(n, viewDir, uRoughness);
        gColor   = vec4(Ld + Ls, 1.0);
    }
}
