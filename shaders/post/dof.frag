#version 330 core

in vec2 vUV;

uniform sampler2D uFrame;
uniform sampler2D uDepth;
uniform float uNear;
uniform float uFar;
uniform float uFocusDist;
uniform float uCocScale;    // 0 = disable (pinhole); cocScale = imageW * focal / (fStop * filmback)
uniform int   uDofSamples;  // 1–16 Poisson taps (quality)

out vec4 FragColor;

float linearDepth(float d) {
    float ndc = d * 2.0 - 1.0;
    return 2.0 * uNear * uFar / (uFar + uNear - ndc * (uFar - uNear));
}

// 16-tap Poisson disc on unit circle
const int TAPS = 16;
const vec2 POISSON[16] = vec2[](
    vec2(-0.9420, -0.3991), vec2(-0.7482,  0.5650),
    vec2(-0.5199,  0.1822), vec2(-0.4034, -0.5651),
    vec2(-0.2197,  0.6393), vec2(-0.0259,  0.3133),
    vec2( 0.0938, -0.4580), vec2( 0.2547,  0.8084),
    vec2( 0.3487,  0.0490), vec2( 0.4699, -0.8031),
    vec2( 0.5718,  0.4276), vec2( 0.6564, -0.2081),
    vec2( 0.7375,  0.7499), vec2( 0.8218, -0.5573),
    vec2( 0.9091,  0.1736), vec2( 0.9582, -0.8563)
);

void main() {
    // Fast-path: pinhole or nearly so
    if (uCocScale < 0.5) {
        FragColor = texture(uFrame, vUV);
        return;
    }

    float rawDepth = texture(uDepth, vUV).r;
    float linDepth = linearDepth(rawDepth);

    // Sky pixels — skip blur
    if (linDepth > uFar * 0.99) {
        FragColor = texture(uFrame, vUV);
        return;
    }

    float coc = uCocScale * abs(linDepth - uFocusDist) / linDepth;
    coc = min(coc, 32.0);

    // In-focus pixels — skip blur
    if (coc < 0.5) {
        FragColor = texture(uFrame, vUV);
        return;
    }

    int  taps  = clamp(uDofSamples, 1, TAPS);
    vec2 texel = 1.0 / vec2(textureSize(uFrame, 0));
    vec3 color = vec3(0.0);
    for (int i = 0; i < taps; i++)
        color += texture(uFrame, vUV + POISSON[i] * coc * texel).rgb;
    FragColor = vec4(color / float(taps), 1.0);
}
