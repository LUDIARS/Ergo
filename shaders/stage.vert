#version 450

// Per-frame view/projection
layout(set = 0, binding = 0) uniform Scene {
    mat4 view;
    mat4 proj;
    vec4 lightDir; // xyz = world-space direction toward light
} scene;

// Per-object model matrix + flat base color (push constants).
layout(push_constant) uniform PushConsts {
    mat4 model;
    vec4 baseColor;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 vNormalWS;
layout(location = 1) out vec4 vBaseColor;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    vNormalWS    = mat3(pc.model) * inNormal;
    vBaseColor   = pc.baseColor;
    gl_Position  = scene.proj * scene.view * worldPos;
}
