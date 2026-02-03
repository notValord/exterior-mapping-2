#version 450

layout(location = 0) in vec2 texCoords;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D offlineRenderSampler;
// layout(set = 0, binding = 0) uniform sampler2DArray offlineRenderSampler;
// layout(set = 0, binding = 1) uniform OfflineRenderBuffer {
//     ivec2 grid;
//     int layerID;
//     int layerCnt;
//     uint presentationgMode;
// } ubo;

// layout(set = 0, binding = 2) uniform sampler2D novelSampler;

void main() {
    // if (presentationgMode == 0) {       // offline render
    //     if (layerID < 0) {
    //         ivec2 camID = clamp(ivec2(texCoords * vec2(grid)), ivec2(0), grid-ivec2(1));
    //         vec2 localUV = fract(texCoords * vec2(grid));
    //         if (camID.y * grid.x + camID.x < layerCnt) {
    //             outColor = texture(offlineRenderSampler, vec3(localUV, float(camID.y * grid.x + camID.x)));
    //         }
    //         else {
    //             outColor = vec4(0.0f);
    //         }
    //     }
    //     else {
    //         outColor = texture(offlineRenderSampler, vec3(texCoords, layerID));
    //     }
    // }
    // else if (presentationgMode == 1) {  // novel render
    //     outColor = texture(novelSampler, texCoords);
    // }
    // else {
    //     outColor = vec4(0.0f);
    // }

    outColor = texture(offlineRenderSampler, texCoords);
    // outColor = vec4(0.0f, 0.0f, 1.0f, 1.0f);
}