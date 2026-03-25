#version 450

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in uint camID;

layout(location = 0) out vec3 colorFrag;
layout(location = 1) flat out uint outCamID;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    gl_Position = ubo.proj * ubo.view * vec4(inPosition.xyz, 1.0);
    gl_PointSize = 1.5; // optional: size of each point
    colorFrag = inColor;
    outCamID = camID;
}