#pragma once

#define GLM_ENABLE_EXPERIMENTAL         // GLM hashes for vec2 and vec3
#include <glm/gtx/hash.hpp>
#include <glm/glm.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec4 color;
    glm::vec2 texCoords;

    bool operator==(const Vertex& other) const;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttribureDescriptions();
};

// a template specialization from cppreference.com, experimental
namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                   (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoords) << 1);
        }
    };
}



struct Point {
    glm::vec4 pos;
    glm::vec4 col;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttribureDescriptions();
};



struct CloudPoint {
    glm::vec4 pos;
    glm::vec3 col;
    uint32_t camID;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttribureDescriptions();
};