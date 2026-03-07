#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inTexCoords;

layout(location = 0) out vec4 colorGrad;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec3 fragPosition;
layout(location = 3) out vec2 fragTexCoords;
layout(location = 4) out float depth;


layout(set = 0, binding = 1) uniform UniformModelObject {
    mat4 model;
} ubo;

layout(push_constant) uniform PushConstant {
    mat4 MVP;
    uint hasTexture;
    uint materialID;
} pc;

void main() {
    gl_Position = pc.MVP * vec4(inPosition, 1.0);

    colorGrad = inColor;
    fragPosition = (ubo.model * vec4(inPosition, 1.0)).xyz;
    normal = inNormal;
    fragTexCoords = inTexCoords;
    depth = gl_Position.z/gl_Position.w;
}
