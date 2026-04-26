#pragma once


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE     // GLM uses openGl depth range -1 to 1

#include <glm/glm.hpp>

#include <json.hpp>
using json = nlohmann::json;

/**
 * @enum ImageViewType
 * @brief Selects the type of image view used for rendering.
 */
enum class ImageViewType : uint32_t {
    COLOR,
    DEPTH
};

/**
 * @enum DebugCompute
 * @brief Controls which debug compute visualization is enabled.
 */
enum class DebugCompute : uint32_t {
    NO_DEBUG,      ///< No debug output.
    INTERSECTION,  ///< Visualize ray intersection positions.
    CAM_COUNT,     ///< Visualize camera count information.
    CAM_ID,        ///< Visualize camera ID values.
    SAMPLE_DEPTH   ///< Visualize sample depth information.
};

/**
 * @enum NovelHeuristic
 * @brief Chooses the heuristic used by novel view synthesis.
 */
enum class NovelHeuristic : uint32_t {
    COLOR_HEURISTIC,
    DEPTH_HEURISTIC,
    ANGLE_HEURISTIC
};

/**
 * @enum PresentationMode
 * @brief Selects the current presentation or rendering mode.
 */
enum class PresentationMode : uint32_t {
    OFFLINE_RENDER,
    NOVEL_RENDER
};

/**
 * @enum DistanceType
 * @brief Chooses the distance metric used by compute shaders.
 */
enum class DistanceType : uint32_t {
    POINT,
    POINT_RAY
};

/**
 * @enum ConeMarching
 * @brief Controls cone marching behavior for ray-based reconstruction.
 */
enum class ConeMarching  : uint32_t {
    NO_CONE,
    ONLY_CONE,
    PARTIAL_CONE
};

struct SceneJson {
    std::string name;
    float scale;
};

inline void to_json(json& j, const SceneJson& scene) {
    j = json{
        {"scene", scene.name},
        {"scale", scene.scale}
    };
}

inline void from_json(const json& j, SceneJson& s) {
    j.at("scene").get_to(s.name);
    j.at("scale").get_to(s.scale);
}

struct CamJson {
    glm::vec3 pos;
    float yaw;
    float pitch;
    float fov;
};

inline void to_json(json& j, const CamJson& cam) {
    j = json{
        {"pos", {cam.pos.x, cam.pos.y, cam.pos.z}},
        {"yaw", cam.yaw},
        {"pitch", cam.pitch},
        {"fov", cam.fov}
    };
}

inline void from_json(const json& j, CamJson& c) {
    auto pos = j.at("pos");
    c.pos = glm::vec3(pos[0], pos[1], pos[2]);

    j.at("yaw").get_to(c.yaw);
    j.at("pitch").get_to(c.pitch);
    j.at("fov").get_to(c.fov);
}

struct CamSetupJson {
    uint32_t camCount;
    std::vector<CamJson> cams;
};

inline void to_json(json& j, const CamSetupJson& setup) {
    j = json{
        {"camCount", setup.camCount},
        {"cams", setup.cams}
    };
}

inline void from_json(const json& j, CamSetupJson& cs) {
    j.at("camCount").get_to(cs.camCount);
    j.at("cams").get_to(cs.cams);
}


struct TestInfo {
    std::string algorithm;
    float fov;
    int sampleCount;
    uint32_t width;
    uint32_t height;
};

/**
 * @struct MeshUniforms
 * @brief Descriptor data for mesh material sampling.
 */
struct MeshUniforms{
    const VkSampler sampler;
    const std::vector<VkImageView> textures;
    VkBuffer materials;
};

/**
 * @struct RayData
 * @brief Per-ray payload used for ray generation and tracing.
 */
struct RayData{
    glm::vec3 origin;
    uint32_t coordX;
    glm::vec3 dir;
    uint32_t coordY;
};

// Uniform Buffer Objects

/**
 * @struct MVPBufferObject
 * @brief Standard model-view-projection uniform buffer.
 */
struct MVPBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

/**
 * @struct RenderFragmentObject
 * @brief Fragment shader data for simple shading.
 */
struct RenderFragmentObject {
    glm::vec3 lightPos;
    float _padd;
    glm::vec3 camPos;
    uint32_t depth;
};

/**
 * @struct RenderMaterialObject
 * @brief Material lighting parameters for the renderer.
 */
struct RenderMaterialObject {
    glm::vec3 ambient;
    float _padd;
    glm::vec3 specular;
    float shininess;
};

/**
 * @struct OfflineBufferObject
 * @brief Uniforms for offline rendering and presentation control.
 */
struct OfflineBufferObject{
    glm::ivec2 grid;
    int layerID;
    int layerCnt;
    PresentationMode presentMode;
    ImageViewType presentType;
};

/**
 * @struct NovelBufferObject
 * @brief Uniforms used by novel view synthesis compute shaders.
 */
struct NovelBufferObject {
    glm::mat4 viewMat;
    glm::mat4 invViewMat;
    glm::mat4 projMat;
    glm::mat4 invProjMat;

    glm::vec2 res;
    glm::vec2 _pad0;        // renderdoc complains

    uint32_t camCnt;
    uint32_t sampleCount;
    uint32_t sampleDebug;
    NovelHeuristic heuristic;

    DebugCompute debugFlag;
    DistanceType distanceFlag;
    uint32_t bestNCount;
    uint32_t _padd1; 

    ConeMarching coneFlag;
    float pixelRadius;
    float inConePercentage;
};

struct PointCloudObject {
    glm::vec2 res;
    uint32_t camCnt;
};

/**
 * @struct RayDataObject
 * @brief Uniforms for ray generation in ray tracing.
 */
struct RayDataObject {
    glm::mat4 invViewMat;
    glm::mat4 invProjMat;
    glm::uvec2 res;
};

/**
 * @struct ReconDataObject
 * @brief Uniforms for novel reconstruction passes.
 */
struct ReconDataObject {
    glm::uvec2 novelRes;
    uint32_t layerCount;
};


// Storage Buffer Data

/**
 * @struct CamArrayData
 * @brief Per-camera data stored in a compute shader SSBO.
 */
struct CamArrayData {
    glm::vec4 frustumPlanes[6];
    glm::mat4 viewMat;
    glm::mat4 invViewMat;
    glm::mat4 projMat;
    glm::mat4 invProjMat;
};

/**
 * @struct CamArrayInvMatrices
 * @brief Stores only inverse camera matrices for ray reconstruction.
 */
struct CamArrayInvMatrices {
    glm::mat4 invViewMat;
    glm::mat4 invProjMat;
};