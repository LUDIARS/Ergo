#version 450

layout(set = 0, binding = 0) uniform Scene {
    mat4 view;
    mat4 proj;
} scene;

layout(push_constant) uniform Push {
    vec4 world_origin;  // xyz: origin, w: world_scale (multiplier on inst_size)
    ivec4 flags;        // x: shape_id (0=circle, 1=square)
} pc;

// Per-vertex attributes (one quad shared by every particle).
layout(location = 0) in vec2 vert_pos;   // [-0.5, 0.5]^2
layout(location = 1) in vec2 vert_uv;    // [0, 1]^2

// Per-instance attributes (one set per live particle).
layout(location = 2) in vec3 inst_pos;
layout(location = 3) in float inst_size;
layout(location = 4) in vec4 inst_color;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;

void main() {
    // Extract camera-aligned axes from the view matrix to make the quad
    // face the camera (cheap world-space billboard).
    vec3 right = vec3(scene.view[0][0], scene.view[1][0], scene.view[2][0]);
    vec3 up    = vec3(scene.view[0][1], scene.view[1][1], scene.view[2][1]);

    float s = inst_size * pc.world_origin.w;
    vec3 world = pc.world_origin.xyz + inst_pos
               + right * (vert_pos.x * s)
               + up    * (vert_pos.y * s);

    gl_Position = scene.proj * scene.view * vec4(world, 1.0);
    v_uv = vert_uv;
    v_color = inst_color;
}
