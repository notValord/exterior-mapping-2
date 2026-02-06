#version 450

layout(location = 0) in vec3 colorGrad;
layout(location = 1) in vec2 fragTexCoords;
layout(location = 2) in float depth;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

float LinearizeDepth()
{
    float zNear = 0.1;          // TODO: Replace by the zNear of your perspective projection
    float zFar  = 100.0;        // TODO: Replace by the zFar  of your perspective projection
    float d = (zNear * zFar) / (zFar - depth * (zFar - zNear));
    return (d - zNear) / (zFar - zNear);
}

void main() {
    // outColor = vec4(colorGrad, 1.0);
    outColor = texture(texSampler, fragTexCoords);

    // float n = LinearizeDepth();
    // outColor = vec4(n, n, n, 1.0);
}