#version 450

layout(push_constant) uniform Push {
    vec4 world_origin;
    ivec4 flags;        // x: shape_id (0=circle, 1=square)
} pc;

layout(set = 0, binding = 1) uniform sampler2D particle_tex;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 tex = texture(particle_tex, v_uv);
    float mask = max(max(tex.r, tex.g), max(tex.b, tex.a));
    if (pc.flags.x == 0) {
        // Circle: discard pixels outside the inscribed disc, with a soft edge.
        vec2 d = v_uv - vec2(0.5);
        float r2 = dot(d, d);
        if (r2 > 0.25) discard;
        float fade = clamp(1.0 - r2 * 4.0, 0.0, 1.0);
        outColor = vec4(v_color.rgb * mask, v_color.a * mask * fade);
    } else {
        vec3 tint = vec3(0.35, 0.85, 1.0);
        float alpha = clamp(mask * 1.8, 0.0, 1.0);
        outColor = vec4(v_color.rgb * tint * mask, alpha);
    }
}
