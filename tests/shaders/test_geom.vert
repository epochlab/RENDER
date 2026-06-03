#version 330 core

out vec3 vNormal;
out vec3 vTangent;
out vec3 vBitangent;
out vec3 vFragPos;
out vec2 vUV;

void main() {
    vNormal    = vec3(0.0, 0.0, 1.0);
    vTangent   = vec3(1.0, 0.0, 0.0);
    vBitangent = vec3(0.0, 1.0, 0.0);
    vFragPos   = vec3(0.0, 0.0, 0.0);
    vUV        = vec2(0.5, 0.5);

    // Fullscreen triangle, z=0 → gl_FragCoord.z=0.5 after depth mapping
    vec2 pos;
    if      (gl_VertexID == 0) pos = vec2(-1.0, -1.0);
    else if (gl_VertexID == 1) pos = vec2( 3.0, -1.0);
    else                       pos = vec2(-1.0,  3.0);
    gl_Position = vec4(pos, 0.0, 1.0);
}
