#version 330 core

in vec2 vUV;

uniform sampler2D uFrame;
uniform sampler2D uAO;           // blurred SSAO result (unit 1)
uniform sampler2D uDepth;        // G-buffer depth (unit 2) — used to mask sky in AO channel
uniform int       uViewMode;
uniform int       uChannelView;  // 0=off 1=R 2=G 3=B  (applied post-composite)
uniform bool      uInvert;       // invert colour values
uniform float     uExposure;     // linear camera exposure multiplier
uniform bool      uAspectEnabled;
uniform float     uAspectRatio;  // e.g. 2.39
uniform sampler3D uColorLUT;     // OCIO 3D display LUT (unit 3)
uniform bool      uLutEnabled;   // false = Raw (no transform)

out vec4 FragColor;

void main() {
    if (uViewMode == 16) {
        // occlusion: black for sky pixels (depth at far plane), AO value for geometry.
        float depth = texture(uDepth, vUV).r;
        float ao    = (depth >= 0.9999) ? 0.0 : texture(uAO, vUV).r;
        FragColor   = vec4(ao, ao, ao, 1.0);
        return;
    }

    vec3 color = texture(uFrame, vUV).rgb;
    if (uViewMode == 1) {
        float depth = texture(uDepth, vUV).r;
        float ao    = (depth >= 0.9999) ? 1.0 : texture(uAO, vUV).r;
        color *= ao;
        color *= uExposure;
    } else if (uViewMode == 9 || uViewMode == 10) {
        color *= uExposure;
    } else if (uViewMode == 8) {
        float k = dot(color, vec3(0.2126, 0.7152, 0.0722));
        color = vec3(k);
    } else if (uViewMode == 7) {
        // RGB → HSV; H=red channel, S=green, V=blue
        vec4 K = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);
        vec4 p = mix(vec4(color.bg, K.wz), vec4(color.gb, K.xy), step(color.b, color.g));
        vec4 q = mix(vec4(p.xyw, color.r), vec4(color.r, p.yzx), step(p.x, color.r));
        float d = q.x - min(q.w, q.y);
        color = vec3(abs(q.z + (q.w - q.y) / (6.0 * d + 1e-10)),
                     d / (q.x + 1e-10), q.x);
    }
    // OCIO display transform — beauty pass only, log2 shaper [-10, +10 stops].
    // LUT stores linear-light tone-mapped values (sRGB OETF removed at bake time).
    if (uLutEnabled && uViewMode == 1) {
        const float L2_MIN = -10.0, L2_MAX = 10.0;
        vec3 s = (log2(max(color, vec3(1e-10))) - L2_MIN) / (L2_MAX - L2_MIN);
        color = texture(uColorLUT, clamp(s, vec3(0.0), vec3(1.0))).rgb;
    }

    // sRGB OETF — encode linear-light beauty values for display.
    // Covers both RAW (linear passthrough) and ACES (linear tone-mapped) paths so
    // the display sees uniformly encoded values: RAW clips HDR to white (washed out),
    // ACES rolls highlights off (darker, more detail).
    if (uViewMode == 1) {
        vec3 lo  = color * 12.92;
        vec3 hi  = pow(max(color, vec3(0.0)), vec3(1.0 / 2.4)) * 1.055 - vec3(0.055);
        color    = mix(lo, hi, step(vec3(0.0031308), color));
    }

    if      (uChannelView == 1) color = vec3(color.r);
    else if (uChannelView == 2) color = vec3(color.g);
    else if (uChannelView == 3) color = vec3(color.b);
    if (uInvert) color = 1.0 - color;

    // Aspect ratio letterbox — 70% opacity bars + 1px white edge.
    if (uAspectEnabled) {
        ivec2 sz    = textureSize(uFrame, 0);
        float scr   = float(sz.x) / float(sz.y);
        float barH  = 0.5 * (1.0 - scr / uAspectRatio);   // correct: scr/ratio
        if (barH > 0.0) {
            float lineW  = 1.0 / float(sz.y);
            bool  onEdge = abs(vUV.y - barH) < lineW || abs(vUV.y - (1.0 - barH)) < lineW;
            bool  inBar  = vUV.y < barH || vUV.y > 1.0 - barH;
            if      (onEdge) color = vec3(1.0);
            else if (inBar)  color *= 0.3;
        }
    }

    FragColor = vec4(color, 1.0);
}
