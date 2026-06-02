#version 330 core

in vec2 vUV;

uniform sampler2D uFrame;
uniform sampler2D uAO;         // blurred SSAO result (unit 1)
uniform sampler2D uDepth;      // G-buffer depth (unit 2) — used to mask sky in AO channel
uniform int       uViewMode;
uniform int       uChannelView; // 0=off 1=R 2=G 3=B  (applied post-composite)
uniform bool      uInvert;      // invert colour values

out vec4 FragColor;

void main() {
    if (uViewMode == 15) {
        // AO: black for sky pixels (depth at far plane), AO value for geometry.
        float depth = texture(uDepth, vUV).r;
        float ao    = (depth >= 0.9999) ? 0.0 : texture(uAO, vUV).r;
        FragColor   = vec4(ao, ao, ao, 1.0);
    } else {
        vec3 color = texture(uFrame, vUV).rgb;
        if (uViewMode == 1) {
            float depth = texture(uDepth, vUV).r;
            float ao    = (depth >= 0.9999) ? 1.0 : texture(uAO, vUV).r;
            color *= ao;
        } else if (uViewMode == 3) {
            float k = dot(color, vec3(0.2126, 0.7152, 0.0722));
            color = vec3(k);
        } else if (uViewMode == 4) {
            // RGB → HSV; H=red channel, S=green, V=blue
            vec4 K = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);
            vec4 p = mix(vec4(color.bg, K.wz), vec4(color.gb, K.xy), step(color.b, color.g));
            vec4 q = mix(vec4(p.xyw, color.r), vec4(color.r, p.yzx), step(p.x, color.r));
            float d = q.x - min(q.w, q.y);
            color = vec3(abs(q.z + (q.w - q.y) / (6.0 * d + 1e-10)),
                         d / (q.x + 1e-10), q.x);
        }
        if      (uChannelView == 1) color = vec3(color.r);
        else if (uChannelView == 2) color = vec3(color.g);
        else if (uChannelView == 3) color = vec3(color.b);
        if (uInvert) color = 1.0 - color;
        FragColor = vec4(color, 1.0);
    }
}
