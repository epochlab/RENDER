#version 330 core

in vec3 vNormal;
in vec3 vTangent;
in vec3 vBitangent;
in vec3 vFragPos;
in vec2 vUV;

uniform sampler2D uAlbedo;
uniform sampler2D uNormalMap;      // tangent-space normal map (unit 2)
uniform sampler2D uIrradianceTex;  // baked diffuse irradiance map (unit 3)
uniform sampler2D uPrefilteredTex; // baked GGX prefiltered env map (unit 4)
uniform sampler2D uBrdfLUT;        // split-sum BRDF LUT (unit 5)
uniform float     uMaxMipLevel;    // = 4.0 (prefilter mip count - 1)
uniform int       uViewMode;       // 1=beauty  2=alpha  3=bounds  4=wireframe  5=depth
                                   // 6=albedo  7=hsv  8=luminance  9=direct_diffuse  10=direct_refl
                                   // 11=world_pos  12=world_normals  13=uv  14=shading_normal
                                   // 15=fresnel  16=occlusion
uniform vec3      uBoundsMin;      // world-space AABB min corner
uniform vec3      uBoundsMax;      // world-space AABB max corner
uniform float     uNear;
uniform float     uFar;
uniform vec3      uCamPos;         // world-space camera position (for reflection vector)
uniform float     uRoughness;      // PBR roughness: 0 = mirror, 1 = fully diffuse
uniform float     uMetallic;       // 0 = dielectric, 1 = metal
uniform float     uIOR;            // index of refraction for dielectrics (default 1.5)
uniform mat4      uView;           // view matrix — used to transform shading normal for SSAO
uniform mat3      uHdriRotMat;     // HDRI rotation applied per-frame; flip/exposure are baked

layout(location = 0) out vec4 gColor;
layout(location = 1) out vec4 gNormal;  // view-space shading normals for SSAO

const float PI = 3.14159265358979;

// Convert world direction to equirectangular UV. Rotation is per-frame; flip/exposure are baked.
vec2 sampleEnvUV(vec3 dir) {
    dir = uHdriRotMat * dir;
    float phi   = atan(dir.z, dir.x);
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    return vec2(phi / (2.0 * PI) + 0.5, 1.0 - theta / PI);
}

// TBN-perturbed shading normal from normal map.
vec3 shadingNormal() {
    vec3 nts = texture(uNormalMap, vUV).rgb * 2.0 - 1.0;
    mat3 TBN = mat3(normalize(vTangent), normalize(vBitangent), normalize(vNormal));
    return normalize(TBN * nts);
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

    } else if (uViewMode == 3) {
        // Bounds — flat grey fill; bounding box drawn as a separate GL_LINES pass in main.cpp
        gColor = vec4(0.25, 0.25, 0.25, 1.0);

    } else if (uViewMode == 4) {
        // Wireframe — white so the histogram treats it as grayscale
        gColor = vec4(1.0, 1.0, 1.0, 1.0);

    } else if (uViewMode == 5) {
        // Linearised depth [0,1]
        float z  = gl_FragCoord.z * 2.0 - 1.0;
        float ld = (2.0 * uNear * uFar) / (uFar + uNear - z * (uFar - uNear));
        float d  = (ld - uNear) / (uFar - uNear);
        gColor = vec4(d, d, d, 1.0);

    } else if (uViewMode == 6) {
        // Albedo — raw texture, no lighting
        gColor = vec4(texture(uAlbedo, vUV).rgb, 1.0);

    } else if (uViewMode == 9) {
        // direct_diffuse — precomputed irradiance, no albedo, no Fresnel mask
        gColor = vec4(texture(uIrradianceTex, sampleEnvUV(shadingNormal())).rgb, 1.0);

    } else if (uViewMode == 10) {
        // direct_refl — prefiltered specular with lobe-centre shift (matches pre-Step-5 reflectionIBL)
        vec3  n       = shadingNormal();
        vec3  viewDir = normalize(uCamPos - vFragPos);
        float NoV     = max(dot(n, viewDir), 0.0);
        vec3  albedo  = texture(uAlbedo, vUV).rgb;
        float f0Dia   = pow((uIOR - 1.0) / (uIOR + 1.0), 2.0);
        vec3  F0      = mix(vec3(f0Dia), albedo, uMetallic);
        float a_sq        = uRoughness * uRoughness;
        vec3  r           = reflect(-viewDir, n);
        vec3  dir         = normalize(mix(r, n, a_sq));
        vec3  prefiltered = textureLod(uPrefilteredTex, sampleEnvUV(dir), uRoughness * uMaxMipLevel).rgb;
        vec3  F           = fresnelWeighted(F0, NoV, uRoughness);
        gColor = vec4(F * prefiltered, 1.0);

    } else if (uViewMode == 11) {
        // world_pos — normalized against scene AABB for a continuous colour gradient
        vec3 extent = uBoundsMax - uBoundsMin;
        gColor = vec4(clamp((vFragPos - uBoundsMin) / max(extent, vec3(1e-6)), 0.0, 1.0), 1.0);

    } else if (uViewMode == 12) {
        // world_normals — world-space vertex normals (unperturbed)
        gColor = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);

    } else if (uViewMode == 13) {
        // UV coordinates
        gColor = vec4(fract(vUV), 0.0, 1.0);

    } else if (uViewMode == 14) {
        // Shading Normal — TBN-perturbed normal visualised as colour
        gColor = vec4(shadingNormal() * 0.5 + 0.5, 1.0);

    } else if (uViewMode == 15) {
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
        // Mode 1 (beauty), 7 (hsv), 8 (luminance), 16 (occlusion) — display overridden by blit.frag
        vec3  viewDir      = normalize(uCamPos - vFragPos);
        vec3  albedo       = texture(uAlbedo, vUV).rgb;
        vec3  n            = shadingNormal();
        float NoV          = max(dot(n, viewDir), 0.0);
        float f0Dielectric = pow((uIOR - 1.0) / (uIOR + 1.0), 2.0);
        vec3  F0           = mix(vec3(f0Dielectric), albedo, uMetallic);
        vec3  F            = fresnelWeighted(F0, NoV, uRoughness);
        vec3  Ld = albedo * (1.0 - F) * (1.0 - uMetallic) * texture(uIrradianceTex, sampleEnvUV(n)).rgb;
        float a_sq         = uRoughness * uRoughness;
        vec3  r            = reflect(-viewDir, n);
        vec3  dir          = normalize(mix(r, n, a_sq));
        vec3  prefiltered  = textureLod(uPrefilteredTex, sampleEnvUV(dir), uRoughness * uMaxMipLevel).rgb;
        vec3  Ls           = F * prefiltered;
        gColor = vec4(Ld + Ls, 1.0);
    }
}
