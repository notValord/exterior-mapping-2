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
    CAM_ID,
    SAMPLE_DEPTH
};

enum class NovelHeuristic : uint32_t {
    COLOR_HEURISTIC,
    DEPTH_HEURISTIC,
    ANGLE_HEURISTIC
};

enum class PresentationMode : uint32_t {
    OFFLINE_RENDER,
    NOVEL_RENDER
};

enum class DistanceType : uint32_t {
    POINT,
    POINT_RAY
};

enum class ConeMarching  : uint32_t {
    NO_CONE,
    ONLY_CONE,
    PARTIAL_CONE
};

struct MeshUniforms{
    const VkSampler sampler;
    const std::vector<VkImageView> textures;
    VkBuffer materials;
};

struct RayData{
    glm::vec3 origin;
    uint32_t coordX;
    glm::vec3 dir;
    uint32_t coordY;
};


// Uniform Buffer Objects
struct MVPBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct RenderFragmentObject {
    glm::vec3 lightPos;
    float _padd;
    glm::vec3 camPos;
    uint32_t depth;
};

struct RenderMaterialObject {
    glm::vec3 ambient;
    float _padd;
    glm::vec3 specular;
    float shininess;
};

struct OfflineBufferObject{
    glm::ivec2 grid;
    int layerID;
    int layerCnt;
    PresentationMode presentMode;
    ImageViewType presentType;
};

struct NovelBufferObject {
    glm::mat4 viewMat;
    glm::mat4 invViewMat;
    glm::mat4 projMat;
    glm::mat4 invProjMat;
    glm::vec2 res;
    // glm::vec2 _pad0;        // renderdoc complains
    uint32_t camCnt;
    uint32_t sampleCount;
    uint32_t sampleDebug;
    NovelHeuristic heuristic;
    DebugCompute debugFlag;
    DistanceType distanceFlag;
    ConeMarching coneFlag;
    float pixelRadius;
    float inConePercentage;
    // uint32_t _padd1; 
};

struct PointCloudObject {
    glm::vec2 res;
    uint32_t camCnt;
};

struct RayDataObject {
    glm::mat4 invViewMat;
    glm::mat4 invProjMat;
    glm::uvec2 res;
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