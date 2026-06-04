#version 330 core

in vec2 vUV;

uniform sampler2D uSSAO;
uniform int       uBlurRadius;

out float FragAO;

void main() {
    vec2  texelSize = 1.0 / vec2(textureSize(uSSAO, 0));
    float result    = 0.0;
    int   count     = 0;
    for (int x = -uBlurRadius; x <= uBlurRadius; ++x)
        for (int y = -uBlurRadius; y <= uBlurRadius; ++y) {
            result += texture(uSSAO, vUV + vec2(x, y) * texelSize).r;
            ++count;
        }
    FragAO = result / float(count);
}
