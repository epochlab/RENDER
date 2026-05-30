#version 330 core

in vec2 vTexCoord;

uniform sampler2D uSkyHDR;  // equirectangular HDR panorama
uniform mat4      uInvVP;   // inverse(projection * view), updated each frame

out vec4 FragColor;

const float PI = 3.14159265358979;

void main() {
    // Reconstruct world-space view direction from NDC position.
    vec2  ndc   = vTexCoord * 2.0 - 1.0;
    vec4  world = uInvVP * vec4(ndc, 1.0, 1.0);
    vec3  dir   = normalize(world.xyz / world.w);

    // Convert to equirectangular UV.
    float phi   = atan(dir.z, dir.x);               // longitude [-π, π]
    float theta = acos(clamp(dir.y, -1.0, 1.0));    // polar angle from +Y [0, π]
    vec2  uv    = vec2(phi / (2.0 * PI) + 0.5, theta / PI);

    FragColor = vec4(texture(uSkyHDR, uv).rgb, 1.0);
}
