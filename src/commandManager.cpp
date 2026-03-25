#include <commandManager.hpp>
#include <vulkanContext.hpp>
#include <camManager.hpp>
#include <pipeline.hpp>
#include <mesh.hpp>

CommandManager::CommandManager(const QueueFamilyIndices& familyIndices, const VkDevice device)
    : deviceHandle(device) {
    createCommandPool(familyIndices);
    createCommandBuffers();
}

CommandManager::~CommandManager() {
    vkDestroyCommandPool(deviceHandle, graphicsCommandPool, nullptr);
}

VkCommandPool CommandManager::getTransferCommandPool() {
    return graphicsCommandPool;
}

void CommandManager::createCommandPool(const QueueFamilyIndices& familyIndices) {

    VkCommandPoolCreateInfo graphicsCommandPoolCI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = familyIndices.graphicsFamily.value()
    };

    if (vkCreateCommandPool(deviceHandle, &graphicsCommandPoolCI, nullptr, &graphicsCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics command pool!");
    }
}

void CommandManager::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    offlineCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo commandBufferAI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = graphicsCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())
    };

    if (vkAllocateCommandBuffers(deviceHandle, &commandBufferAI, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }

    if (vkAllocateCommandBuffers(deviceHandle, &commandBufferAI, offlineCommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}



struct CommandRecordSetter{
    static void beginRenderPass(VkCommandBuffer commandBuffer, VkRenderPass renderPass, VkFramebuffer frameBuffer, const VkExtent2D& swapChainExtent, bool clearColor) {
        std::array<VkClearValue, 2> baseColor{};
        baseColor[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
        baseColor[1].depthStencil = {1.0f, 0};
        
        VkRenderPassBeginInfo renderPassBI{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = renderPass,
            .framebuffer = frameBuffer,
            .renderArea = {{0,0}, swapChainExtent},  // offset, extent
            .clearValueCount = 0,
            .pClearValues = VK_NULL_HANDLE
        };

        if (clearColor) {
            renderPassBI.clearValueCount = static_cast<uint32_t>(baseColor.size()),
            renderPassBI.pClearValues = baseColor.data();
        }

        vkCmdBeginRenderPass(commandBuffer, &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);
    }

    static void setViewport(VkCommandBuffer commandBuffer, const VkExtent2D& swapChainExtent) {
        VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapChainExtent.width),
            .height = static_cast<float>(swapChainExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    }

    static void setScissor(VkCommandBuffer commandBuffer, const VkExtent2D& swapChainExtent, VkOffset2D offset = {0, 0}) {
        VkRect2D scissor{
            .offset = offset,
            .extent = swapChainExtent
        };
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }
};

void CommandRecorder::setPipeline(const GraphicPipeline& pipeline, VkRenderPass render, VkExtent2D extent) {
    graphicPipeline = pipeline.pipeline;
    graphicPipelineLayout = pipeline.pipelineLayout;
    renderPass = render;
    swapchainExtent = extent;
}

void CommandRecorder::setPipeline(const ComputePipeline& pipeline) {
    computePipeline = pipeline.pipeline;
    computePipelineLayout = pipeline.pipelineLayout;
}

void CommandRecorder::setRenderBuffers(VkBuffer vertex, uint32_t count, VkBuffer index) {
    vertexBuffer = vertex;
    indexCount = count;
    indexBuffer = index;
}

void CommandRecorder::setComputeBuffer(VkBuffer dataBuffer, VkBuffer countBuffer) {
    computeBuffer.dataBuffer =  dataBuffer;
    computeBuffer.counterBuffer = countBuffer;
}


void CommandRecorder::recordRenderScene(VkCommandBuffer commandBuffer,
                                        const std::vector<VkDescriptorSet>& descriptorSets,
                                        VkFramebuffer framebuffer,
                                        const std::vector<glm::mat4>& pushConstants) {
    CommandRecordSetter::beginRenderPass(commandBuffer, renderPass, framebuffer, swapchainExtent, true);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipeline);

    CommandRecordSetter::setViewport(commandBuffer, swapchainExtent);
    CommandRecordSetter::setScissor(commandBuffer, swapchainExtent);

    if (vertexBuffer != VK_NULL_HANDLE) {
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    }

    if (!descriptorSets.empty()) {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
    }

    if (!pushConstants.empty()) {
        vkCmdPushConstants(commandBuffer, graphicPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)*pushConstants.size(), pushConstants.data());
    }

    if (indexBuffer != VK_NULL_HANDLE) {
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    }
    else {
        vkCmdDraw(commandBuffer, indexCount, 1, 0, 0);
    }
    vkCmdEndRenderPass(commandBuffer);

    vertexBuffer = VK_NULL_HANDLE;
    indexBuffer = VK_NULL_HANDLE;
}

void CommandRecorder::recordRenderCamCube(VkCommandBuffer commandBuffer, const std::vector<VkDescriptorSet>& descriptorSets, VkFramebuffer framebuffer, const CamerasManager& camManager) {
    CommandRecordSetter::beginRenderPass(commandBuffer, renderPass, framebuffer, swapchainExtent, false);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipeline);

    CommandRecordSetter::setViewport(commandBuffer, swapchainExtent);
    CommandRecordSetter::setScissor(commandBuffer, swapchainExtent);

    if (!descriptorSets.empty()) {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
    }

    glm::mat4 viewProj = camManager.activeCam->getProjectionMatrix() * camManager.activeCam->getViewMatrix();
    for (uint32_t i = 0; i < camManager.getCamCount(); i++) {
        glm::mat4x4 camMat = viewProj * glm::inverse(camManager.camArray[i].getViewMatrix());
        vkCmdPushConstants(commandBuffer, graphicPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &camMat);
        vkCmdDraw(commandBuffer, 36, 1, 0, 0);
    }

    vkCmdEndRenderPass(commandBuffer);

    vertexBuffer = VK_NULL_HANDLE;
    indexBuffer = VK_NULL_HANDLE;
}

void CommandRecorder::recordMultiScene(VkCommandBuffer commandBuffer,
                                        const std::vector<VkDescriptorSet>& descriptorSets,
                                        VkFramebuffer framebuffer,
                                        const std::vector<glm::mat4>& pushConstants,
                                        const std::vector<SubMesh>& meshes,
                                        const std::vector<VkDescriptorSet>& materialSets) {
    CommandRecordSetter::beginRenderPass(commandBuffer, renderPass, framebuffer, swapchainExtent, true);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipeline);

    CommandRecordSetter::setViewport(commandBuffer, swapchainExtent);
    CommandRecordSetter::setScissor(commandBuffer, swapchainExtent);

    if (vertexBuffer != VK_NULL_HANDLE) {
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    }

    if (!pushConstants.empty()) {       // set the MVP matrix per frame
        vkCmdPushConstants(commandBuffer, graphicPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::mat4), pushConstants.data());
    }

    if (!descriptorSets.empty()) {      // bind the uniform buffer
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
    }

    if (indexBuffer == VK_NULL_HANDLE) {
        throw std::runtime_error("Missing index buffer!");
    }

    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    uint32_t meshID = 0;
    for (const auto& mesh: meshes) {
        uint32_t hasTexture = 0;
        if (mesh.hasTexture()) {
            hasTexture = 1;
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipelineLayout, 1, 1, &materialSets[mesh.textureID], 0, nullptr);
        }
        else {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipelineLayout, 1, 1, &materialSets[0], 0, nullptr);     // dummy
        }

        vkCmdPushConstants(commandBuffer, graphicPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(uint32_t), &hasTexture);
        vkCmdPushConstants(commandBuffer, graphicPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4) + sizeof(uint32_t), sizeof(uint32_t), &meshID);
        vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, mesh.indexOffset, 0, 0);
        meshID++;
    }

    vkCmdEndRenderPass(commandBuffer);

    vertexBuffer = VK_NULL_HANDLE;
    indexBuffer = VK_NULL_HANDLE;
}

void CommandRecorder::clearStorageImage(VkCommandBuffer commandBuffer, VkImage storageImage) {
    VkClearColorValue clearColor = { .float32 = {0.0f, 0.0f, 0.0f, 0.0f} };

    VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    vkCmdClearColorImage(commandBuffer, storageImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &range);

    VkImageMemoryBarrier imageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = storageImage,
        .subresourceRange = range,
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
}

void CommandRecorder::barrierStorageImage(VkCommandBuffer commandBuffer, const std::vector<VkImage>& storageImages, uint32_t layerCount) {
    VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = layerCount,
    };

    std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
    imageMemoryBarriers.reserve(storageImages.size());
    for (const VkImage image : storageImages ) {
        VkImageMemoryBarrier imageMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = range,
        };

        imageMemoryBarriers.emplace_back(imageMemoryBarrier);
    }

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, imageMemoryBarriers.size(), imageMemoryBarriers.data());
}

void CommandRecorder::clearComputeBuffer(VkCommandBuffer commandBuffer) {
    vkCmdFillBuffer(commandBuffer, computeBuffer.dataBuffer, 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(commandBuffer, computeBuffer.counterBuffer, 0, sizeof(uint32_t), 0);

    barrierComputeBuffer(commandBuffer);
}

void CommandRecorder::barrierStorageBuffer(VkCommandBuffer commandBuffer,
                                           VkBuffer storageBuffer,
                                           VkAccessFlags srcMask,
                                           VkAccessFlags dstMask,
                                           VkPipelineStageFlagBits srcStage,
                                           VkPipelineStageFlagBits dstStage) {
    VkBufferMemoryBarrier fillBarriers = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = srcMask,
        .dstAccessMask = dstMask,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = storageBuffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE
    };

    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 1, &fillBarriers, 0, nullptr);
}

void CommandRecorder::barrierComputeBuffer(VkCommandBuffer commandBuffer,
                                           VkAccessFlags srcMask,
                                           VkAccessFlags dstMask,
                                           VkPipelineStageFlagBits srcStage,
                                           VkPipelineStageFlagBits dstStage) {
    std::array<VkBufferMemoryBarrier, 2> fillBarriers;
    fillBarriers[0] = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = srcMask,
        .dstAccessMask = dstMask,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = computeBuffer.dataBuffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE
    };
    fillBarriers[1] = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = srcMask,
        .dstAccessMask = dstMask,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = computeBuffer.counterBuffer,
        .offset = 0,
        .size = sizeof(uint32_t)
    };

    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, fillBarriers.size(), fillBarriers.data(), 0, nullptr);
}

void CommandRecorder::recordCompute(VkCommandBuffer commandBuffer, const std::vector<VkDescriptorSet>& descriptorSets, std::pair<uint32_t, uint32_t> dispatchSize) {
    if (!computeBuffer.isEmpty()) {
        clearComputeBuffer(commandBuffer);
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    if (!descriptorSets.empty()) {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
    }

    vkCmdDispatch(commandBuffer, dispatchSize.first, dispatchSize.second, 1);

    if (!computeBuffer.isEmpty()) {
        barrierComputeBuffer(commandBuffer,
                        VK_ACCESS_SHADER_WRITE_BIT, 
                        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, 
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
    }

    computeBuffer.dataBuffer = VK_NULL_HANDLE;     // clear buffers
    computeBuffer.counterBuffer = VK_NULL_HANDLE;
}