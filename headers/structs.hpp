#pragma once

#include <glm/glm.hpp>

enum class ImageViewType : uint32_t {
    COLOR,
    DEPTH
};

enum class DebugCompute : uint32_t {
    NO_DEBUG,
    INTERSECTION,
    CAM_COUNT,
    CAM_ID
};

enum class PresentationMode : uint32_t {
    OFFLINE_RENDER,
    NOVEL_RENDER
};

enum class VertexInputFlags : uint32_t {
    POS_COL_UV,
    POS,
    NONE
};

struct DepthStencilFlags{
    VkBool32 testEnable;
    VkBool32 writeEnable;
};

struct RasterizationFlags{
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
};


// Uniform Buffer Objects
struct MVPBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct RenderFragmentObject {
    uint32_t depth;
};

struct OfflineBufferObject{
    glm::ivec2 grid;
    int layerID;
    int layerCnt;
    PresentationMode presentMode;
    ImageViewType presentType;
};

struct NovelBufferObject {
    // mat4 viewMat;
    glm::mat4 invViewMat;
    // mat4 projMat;
    glm::mat4 invProjMat;
    glm::vec2 res;
    // glm::vec2 _pad0;        // renderdoc complains
    uint32_t camCnt;
    uint32_t sampleCount;
    DebugCompute debugFlag;
    // uint32_t _padd1; 
};

struct PointCloudObject {
    glm::vec2 res;
    uint32_t camCnt;
};


// Storage Buffer Data
struct CamArrayData {
    glm::vec4 frustumPlanes[6];
    glm::mat4 viewMat;
    glm::mat4 invViewMat;
    glm::mat4 projMat;
    glm::mat4 invProjMat;
};

struct CamArrayInvMatrices {
    glm::mat4 invViewMat;
    glm::mat4 invProjMat;
};