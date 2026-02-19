#include <vertex.hpp>

bool Vertex::operator==(const Vertex& other) const {
    return pos == other.pos && color == other.color && texCoords == other.texCoords;
}

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Vertex::getAttribureDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    attributeDescriptions.reserve(3);
    attributeDescriptions.emplace_back(VkVertexInputAttributeDescription{
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, pos)
    });

    attributeDescriptions.emplace_back(VkVertexInputAttributeDescription{
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, color)
    });

    attributeDescriptions.emplace_back(VkVertexInputAttributeDescription{
        .location = 2,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(Vertex, texCoords)
    });

    return attributeDescriptions;
}



VkVertexInputBindingDescription CloudPoint::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{
        .binding = 0,
        .stride = sizeof(CloudPoint),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> CloudPoint::getAttribureDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    attributeDescriptions.reserve(2);
    attributeDescriptions.emplace_back(VkVertexInputAttributeDescription{
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(CloudPoint, pos)
    });

    attributeDescriptions.emplace_back(VkVertexInputAttributeDescription{
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(CloudPoint, col)
    });

    return attributeDescriptions;
}