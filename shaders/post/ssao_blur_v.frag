#version 330 core

in vec2 vUV;

uniform sampler2D uSSAO;       // H-pass output (unit 0)
uniform int       uBlurRadius;

out float FragAO;

void main() {
    vec2  texelSize = 1.0 / vec2(textureSize(uSSAO, 0));
    float result    = 0.0;
    for (int i = -uBlurRadius; i <= uBlurRadius; ++i)
        result += texture(uSSAO, vUV + vec2(0.0, texelSize.y * float(i))).r;
    FragAO = result / float(2 * uBlurRadius + 1);
}
