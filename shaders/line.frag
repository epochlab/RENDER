#version 330 core
layout(location = 0) out vec4 gColor;
layout(location = 1) out vec4 gNormal;
void main() {
    gColor  = vec4(1.0, 1.0, 0.0, 1.0);   // yellow bounding box
    gNormal = vec4(0.5, 0.5, 1.0, 1.0);   // flat normal — irrelevant for lines
}
