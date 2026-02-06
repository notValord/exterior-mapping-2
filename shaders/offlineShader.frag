#version 450

// Result to present
const uint OFFLINE_RENDER = 0;
const uint NOVEL_RENDER   = 1;

// Image type
const uint COLOR    = 0;
const uint DEPTH    = 1;


layout(location = 0) in vec2 texCoords;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform OfflineRenderBuffer {
    ivec2 grid;
    int layerID;
    int layerCnt;
    uint presentationMode;
    uint presentationType;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D novelSampler;
layout(set = 1, binding = 0) uniform sampler2DArray offlineRenderSampler;

void main() {
    if (ubo.presentationMode == OFFLINE_RENDER) {
        if (ubo.layerID < 0) {      // present all images in a grid
            ivec2 camID = clamp(ivec2(texCoords * vec2(ubo.grid)), ivec2(0), ubo.grid-ivec2(1));
            vec2 localUV = fract(texCoords * vec2(ubo.grid));       // transfer to local UV to sample the whole image

            if (camID.y * ubo.grid.x + camID.x < ubo.layerCnt) {
                outColor = texture(offlineRenderSampler, vec3(localUV.x, localUV.y, float(camID.y * ubo.grid.x + camID.x)));
            }
            else {          // back out leftover cells in the grid
                outColor = vec4(0.0f);
            }
        }
        else {      // present offline images one by one
            outColor = texture(offlineRenderSampler, vec3(texCoords.x, texCoords.y, ubo.layerID));
        }
    }
    else if (ubo.presentationMode == NOVEL_RENDER) {
        outColor = texture(novelSampler, texCoords);
    }
    else {      // unknown value
        outColor = vec4(0.0f);
    }
}