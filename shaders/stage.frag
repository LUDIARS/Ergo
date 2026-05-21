#version 450

layout(set = 0, binding = 0) uniform Scene {
    mat4 view;
    mat4 proj;
    vec4 lightDir;
} scene;

layout(location = 0) in vec3 vNormalWS;
layout(location = 1) in vec4 vBaseColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(vNormalWS);
    vec3 L = normalize(scene.lightDir.xyz);
    float ndl = max(dot(N, L), 0.0);
    // Half-Lambert + small ambient so unlit faces aren't pitch black.
    float diffuse = ndl * 0.85 + 0.15;
    outColor = vec4(vBaseColor.rgb * diffuse, vBaseColor.a);
}
