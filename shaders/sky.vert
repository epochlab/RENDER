#version 330 core
// Fullscreen triangle at the far plane — no VBO, driven by gl_VertexID (same as blit.vert).
// z = 1.0 places the sky at NDC far plane so all geometry passes depth test in front of it.
out vec2 vTexCoord;
void main() {
    vec2 pos;
    if      (gl_VertexID == 0) pos = vec2(-1.0, -1.0);
    else if (gl_VertexID == 1) pos = vec2( 3.0, -1.0);
    else                       pos = vec2(-1.0,  3.0);
    gl_Position = vec4(pos, 1.0, 1.0);
    vTexCoord   = pos * 0.5 + 0.5;
}
