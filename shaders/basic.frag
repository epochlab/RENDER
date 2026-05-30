#version 330 core

in vec3 vNormal;
in vec3 vFragPos;
in vec2 vUV;

uniform sampler2D uAlbedo;
uniform int       uViewMode;  // 1=diffuse 2=wireframe 3=normals 4=depth 5=uv
uniform float     uNear;
uniform float     uFar;

out vec4 FragColor;

void main() {
    if (uViewMode == 2) {
        // Wireframe — lines drawn by glPolygonMode(GL_LINE)
        FragColor = vec4(0.35, 0.85, 1.0, 1.0);

    } else if (uViewMode == 3) {
        // World-space normals → RGB
        FragColor = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);

    } else if (uViewMode == 4) {
        // Linearised depth, normalised to [0,1]
        float z  = gl_FragCoord.z * 2.0 - 1.0;
        float ld = (2.0 * uNear * uFar) / (uFar + uNear - z * (uFar - uNear));
        float d  = (ld - uNear) / (uFar - uNear);
        FragColor = vec4(d, d, d, 1.0);

    } else if (uViewMode == 5) {
        // UV coordinates as RG gradient
        FragColor = vec4(vUV, 0.0, 1.0);

    } else {
        // Mode 1 — diffuse + albedo texture
        vec3 lightDir = normalize(vec3(1.0, 2.0, 1.5));
        vec3 norm     = normalize(vNormal);
        float diff    = max(dot(norm, lightDir), 0.0);
        vec3 albedo   = texture(uAlbedo, vUV).rgb;
        FragColor     = vec4(albedo * (0.18 + 0.82 * diff), 1.0);
    }
}
