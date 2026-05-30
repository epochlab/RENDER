#version 330 core

in vec2 vUV;

uniform sampler2D uSSAO;  // raw SSAO texture (unit 0)

out float FragAO;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(uSSAO, 0));
    float result = 0.0;
    for (int x = -2; x <= 2; ++x)
        for (int y = -2; y <= 2; ++y)
            result += texture(uSSAO, vUV + vec2(x, y) * texelSize).r;
    FragAO = result / 25.0;
}
