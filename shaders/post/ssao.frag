#version 330 core

in vec2 vUV;

uniform sampler2D gNormal;    // view-space normals (unit 0), packed [0,1]
uniform sampler2D gDepth;     // depth texture (unit 1)
uniform sampler2D uNoiseTex;  // 4×4 rotation noise, GL_REPEAT (unit 2)

layout(std140) uniform KernelBlock { vec4 uKernel[64]; };
uniform int   uKernelSize;
uniform mat4  uProj;
uniform mat4  uInvProj;
uniform vec2  uNoiseScale;    // framebuffer_size / 4.0
uniform float uRadius;
uniform float uBias;

out float FragAO;

vec3 viewPosFromDepth(vec2 uv, float depth) {
    vec4 ndc  = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = uInvProj * ndc;
    return view.xyz / view.w;
}

void main() {
    float depth = texture(gDepth, vUV).r;

    // Skip sky fragments (depth at far plane = 1.0, no geometry).
    if (depth >= 0.9999) { FragAO = 1.0; return; }

    vec3 fragPosVS = viewPosFromDepth(vUV, depth);
    vec3 normal    = normalize(texture(gNormal, vUV).rgb * 2.0 - 1.0);

    vec3 randomVec = normalize(texture(uNoiseTex, vUV * uNoiseScale).rgb * 2.0 - 1.0);
    vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    int   validSamples = 0;
    float occlusion    = 0.0;

    for (int i = 0; i < uKernelSize; ++i) {
        vec3 sampleVS = fragPosVS + TBN * uKernel[i].xyz * uRadius;

        vec4 offset = uProj * vec4(sampleVS, 1.0);
        offset.xyz /= offset.w;
        offset.xyz  = offset.xyz * 0.5 + 0.5;

        float sampleDepth = texture(gDepth, offset.xy).r;

        // Exclude sky pixels from the occlusion computation entirely.
        if (sampleDepth >= 0.9999) continue;

        validSamples++;
        vec3  samplePosVS = viewPosFromDepth(offset.xy, sampleDepth);
        float rangeCheck  = smoothstep(0.0, 1.0, uRadius / abs(fragPosVS.z - samplePosVS.z));
        occlusion += (samplePosVS.z >= sampleVS.z + uBias ? 1.0 : 0.0) * rangeCheck;
    }

    FragAO = validSamples > 0 ? 1.0 - (occlusion / float(validSamples)) : 1.0;
}
