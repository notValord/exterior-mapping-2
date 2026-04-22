#version 450

layout(early_fragment_tests) in;

layout(location = 0) in vec3 colorFrag;
layout(location = 1) flat in uint camID;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0, rg32ui) uniform writeonly uimage2D metadataImage;

void main() {
    outColor = vec4(colorFrag, 1.0f);

    ivec2 pixelCoords = ivec2(gl_FragCoord.xy);
    uint camBitArr = 1u << camID;

    imageStore(metadataImage, pixelCoords, uvec4(1u, camBitArr, 0u, 0u));
}