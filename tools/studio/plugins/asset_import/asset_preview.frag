#version 450
layout(location = 0) in vec3 litColor;
layout(location = 0) out vec4 pixel;
void main() {
    pixel = vec4(litColor, 1.0);
}
