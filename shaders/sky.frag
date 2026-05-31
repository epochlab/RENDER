#version 330 core

in vec2 vTexCoord;

uniform sampler2D uSkyHDR;  // equirectangular HDR panorama
uniform mat4      uInvVP;   // inverse(projection * view), updated each frame
uniform vec3      uHdriRot; // XYZ Euler rotation in radians applied to the sky direction
uniform float     uHdriExposure;

out vec4 FragColor;

const float PI = 3.14159265358979;

// Rotate vec3 by Euler XYZ (intrinsic, right-hand).
vec3 rotateXYZ(vec3 v, vec3 angles) {
    float cx = cos(angles.x), sx = sin(angles.x);
    float cy = cos(angles.y), sy = sin(angles.y);
    float cz = cos(angles.z), sz = sin(angles.z);

    v = vec3(v.x, cx*v.y - sx*v.z, sx*v.y + cx*v.z);
    v = vec3(cy*v.x + sy*v.z, v.y, -sy*v.x + cy*v.z);
    v = vec3(cz*v.x - sz*v.y, sz*v.x + cz*v.y, v.z);
    return v;
}

void main() {
    vec2  ndc   = vTexCoord * 2.0 - 1.0;
    vec4  world = uInvVP * vec4(ndc, 1.0, 1.0);
    vec3  dir   = normalize(world.xyz / world.w);

    dir = rotateXYZ(dir, uHdriRot);

    float phi   = atan(dir.z, dir.x);
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    vec2  uv    = vec2(phi / (2.0 * PI) + 0.5, 1.0 - theta / PI);

    FragColor = vec4(texture(uSkyHDR, uv).rgb * uHdriExposure, 1.0);
}
