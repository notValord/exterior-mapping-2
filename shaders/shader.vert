#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoords;

layout(location = 0) out vec3 colorGrad;
layout(location = 1) out vec2 fragTexCoords;
layout(location = 2) out float depth;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;


// set up push constant for offline rendering
// for fragment shader have a depth flag to show depth values
void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);

    colorGrad = inColor;
    fragTexCoords = inTexCoords;
    depth = gl_Position.z/gl_Position.w;
}
