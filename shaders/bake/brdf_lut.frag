#version 330 core
in vec2 vUV;

uniform int uSamples;

layout(location = 0) out vec2 fragColor;

const float PI  = 3.14159265358979;
const float PHI = 2.3999632;

// Smith masking (IBL remap k=a²/2), matches pbr.frag geoSmithIBL.
float geoSmithIBL(float NdotX, float a) {
    float k = a * a * 0.5;
    return NdotX / max(NdotX * (1.0 - k) + k, 1e-6);
}

void main() {
    float NdotV    = max(vUV.x, 1e-4);
    float roughness = vUV.y;
    float a        = roughness * roughness;

    // V in tangent space with N=(0,0,1)
    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);

    float F_scale = 0.0;
    float F_bias  = 0.0;

    for (int i = 0; i < uSamples; i++) {
        float u     = (float(i) + 0.5) / float(uSamples);
        float phi   = PHI * float(i);
        float denom = max(1.0 + (a * a - 1.0) * u, 1e-6);
        float cosH  = sqrt((1.0 - u) / denom);
        float sinH  = sqrt(max(0.0, 1.0 - cosH * cosH));
        vec3  H     = vec3(sinH * cos(phi), sinH * sin(phi), cosH);
        vec3  L     = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0) {
            float G     = geoSmithIBL(NdotV, a) * geoSmithIBL(NdotL, a);
            float G_Vis = G * VdotH / max(NdotH * NdotV, 1e-6);
            float Fc    = pow(1.0 - VdotH, 5.0);
            F_scale += (1.0 - Fc) * G_Vis;
            F_bias  += Fc * G_Vis;
        }
    }

    fragColor = vec2(F_scale, F_bias) / float(uSamples);
}
