#version 450

layout(location = 0) in vec2 texCoords;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D offlineRenderSampler;

void main() {
    outColor = texture(offlineRenderSampler, texCoords);
    // outColor = vec4(0.0f, 0.0f, 1.0f, 1.0f);
}