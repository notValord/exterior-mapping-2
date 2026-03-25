#version 450

layout(location = 0) in vec3 colorFrag;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(colorFrag, 0.3);
}