#pragma once

#define GLM_ENABLE_EXPERIMENTAL         // GLM hashes for vec2 and vec3
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE     // GLM uses openGl depth range -1 to 1
#include <glm/gtx/hash.hpp>
#include <glm/glm.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

/**
 * @struct Vertex
 * @brief Vertex data with position, normal, color, and texture coordinates.
 * Used for standard mesh rendering with full lighting and texture support.
 */
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec4 color;
    glm::vec2 texCoords;

    /// Equality comparison operator for vertex deduplication.
    bool operator==(const Vertex& other) const;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

// Template specialization for Vertex hashing (from cppreference.com, experimental)
namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                   (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoords) << 1);
        }
    };
}



/**
 * @struct Point
 * @brief Simple vertex with position and color.
 */
struct Point {
    glm::vec4 pos;
    glm::vec4 col;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};



/**
 * @struct CloudPoint
 * @brief Vertex with position, color, and camera identification.
 * Used for point cloud rendering with per-point camera source information.
 */
struct CloudPoint {
    glm::vec4 pos;
    glm::vec3 col;
    uint32_t camID;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};