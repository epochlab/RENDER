#version 330 core
// Fullscreen triangle — same pattern as blit.vert / sky.vert.
out vec2 vUV;
void main() {
    vec2 pos;
    if      (gl_VertexID == 0) pos = vec2(-1.0, -1.0);
    else if (gl_VertexID == 1) pos = vec2( 3.0, -1.0);
    else                       pos = vec2(-1.0,  3.0);
    gl_Position = vec4(pos, 0.0, 1.0);
    vUV = pos * 0.5 + 0.5;
}
