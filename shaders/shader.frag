#version 450

layout(location = 0) in vec3 colorGrad;
layout(location = 1) in vec2 fragTexCoords;
layout(location = 2) in float depth;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

layout(binding = 2) uniform RenderFragmentObject {
    uint depth;
} ubo;

float LinearizeDepth()      // bounded to 0-1
{
    float zNear = 0.1;          // TODO: Replace by the zNear of your perspective projection
    float zFar  = 100.0;        // TODO: Replace by the zFar  of your perspective projection
    float d = (zNear * zFar) / (zFar - depth * (zFar - zNear));
    return (d - zNear) / (zFar - zNear);
}

float differentDepth() {
    float zNear = 0.1;
    float zFar  = 100.0;
    float z = gl_FragCoord.z;
    return (2.0 * zNear) / (zFar + zNear - z * (zFar - zNear));
}

void main() {
    if (ubo.depth == 1) {
        float n = differentDepth();
        outColor = vec4(n, n, n, 1.0);
    }
    else {
        outColor = texture(texSampler, fragTexCoords);
    }
    // outColor = vec4(colorGrad, 1.0);
    // outColor = texture(texSampler, fragTexCoords);
}