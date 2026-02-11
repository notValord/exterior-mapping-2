#include <application.hpp>

static unsigned int SCREEN_WIDTH = 800;
static unsigned int SCREEN_HEIGHT = 600;

static const std::string MODEL_PATH = "../resources/models/viking_room.obj";
static const std::string TEXTURE_PATH = "../resources/textures/viking_room.png";

static const std::string CUBE_TEXTURE_PATH = "../resources/textures/camera.jpg";

const size_t MAX_FRAMES_IN_FLIGHT = 2;

void App::run() {;
    mainLoop();
}

App::App() 
    : appWindow(SCREEN_WIDTH, SCREEN_HEIGHT), 
       vulkanContext(appWindow.window),
       commandManager(vulkanContext.familyIndices, vulkanContext.device),
       memManager(vulkanContext.getPhysicalDeviceInstance(), commandManager.getTransferCommandPool(), vulkanContext.graphicsQueue),
       swapchain(vulkanContext.getDeviceSurfaceHandle(), vulkanContext.familyIndices, vulkanContext.device, appWindow.window, memManager),
       descripManager(vulkanContext.device),
       graphicsPipeline(vulkanContext.device, swapchain.getAttachementsFormats(), descripManager.renderDescriptors.descriptorSetLayout),
       computePipeline(vulkanContext.device, descripManager.computeDescriptors.descriptorSetLayout, descripManager.computeDescriptors.sharedDescriptorSetLayout, descripManager.pointCloudDescriptors.descriptorSetLayout),
       syncManager(vulkanContext.device),
       textureManager(TEXTURE_PATH, CUBE_TEXTURE_PATH, vulkanContext.device, memManager, vulkanContext.getDeviceProperties()),
       mesh(MODEL_PATH, vulkanContext.device, memManager),
       camManager(vulkanContext.device, memManager, swapchain.swapChainExtent, swapchain.getAttachementsFormats(), graphicsPipeline.renderPass, vulkanContext.getDeviceProperties()),
       uniformManager(memManager, swapchain.swapChainExtent, camManager.getCamCount()),
       inputManager(appWindow.window, camManager, swapchain.getAttachementsFormats(), swapchain.swapChainImageViews, vulkanContext.getPhysicalDeviceInstance(),
            vulkanContext.graphicsQueue, vulkanContext.familyIndices, swapchain.swapChainExtent, memManager),
       debugUtil(memManager, camManager.getCamCount()){
    // order of members in a class
    
    std::cout << "Initial done" << std::endl;

    // Debug utils with separate pipelines
    graphicsPipeline.setupFrustumPipeline(descripManager.frustumDescriptors.descriptorSetLayout, swapchain.getAttachementsFormats());
    graphicsPipeline.setupIntersectionPipeline(descripManager.frustumDescriptors.descriptorSetLayout, swapchain.getAttachementsFormats());
    graphicsPipeline.setupCamCubePipeline(descripManager.camCubeDestriptors.descriptorSetLayout, swapchain.getAttachementsFormats());
    graphicsPipeline.setupOfflinePipeline(descripManager.offlineDescriptors.descriptorSetLayout, descripManager.computeDescriptors.sharedDescriptorSetLayout);

    swapchain.createFramebuffers(graphicsPipeline.renderPass);
    descripManager.createDescriptorPoolSet(uniformManager, textureManager.modelTexture.getSamplerView(), textureManager.cubeTexture.getSamplerView(), camManager);

    inputManager.setCallbacks();
}

App::~App() {
    // reverse order of constructors
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

void App::recordCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    CommandRecordSetter::beginRenderPass(commandBuffer, graphicsPipeline.renderPass, framebuffer, swapchain.swapChainExtent, true);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.graphicsPipeline);

    CommandRecordSetter::setViewport(commandBuffer, swapchain.swapChainExtent);
    CommandRecordSetter::setScissor(commandBuffer, swapchain.swapChainExtent);

    VkBuffer vertexBuffers[] = {mesh.vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.pipelineLayout, 0, 1, &descripManager.renderDescriptors.descriptorSets[currentFrame], 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, mesh.getIndicesSize(), 1, 0, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
}

void App::recordFrustumCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    CommandRecordSetter::beginRenderPass(commandBuffer, graphicsPipeline.onTopRenderPass, framebuffer, swapchain.swapChainExtent, false);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.frustumPipeline);

    CommandRecordSetter::setViewport(commandBuffer, swapchain.swapChainExtent);
    CommandRecordSetter::setScissor(commandBuffer, swapchain.swapChainExtent);

    vkCmdBindIndexBuffer(commandBuffer, debugUtil.frustumIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.frustumPipelineLayout, 0, 1, &descripManager.frustumDescriptors.descriptorSets[currentFrame], 0, nullptr);

    for (uint32_t i = 0; i < camManager.getCamCount(); i++) {
        VkBuffer vertexBuffers[] = {debugUtil.frustumVertexBuffers[currentFrame][i]};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdDrawIndexed(commandBuffer, 36, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(commandBuffer);
}

void App::recordCamCubeCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    CommandRecordSetter::beginRenderPass(commandBuffer, graphicsPipeline.onTopRenderPass, framebuffer, swapchain.swapChainExtent, false);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.camCubePipeline);

    CommandRecordSetter::setViewport(commandBuffer, swapchain.swapChainExtent);
    CommandRecordSetter::setScissor(commandBuffer, swapchain.swapChainExtent);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.camCubePipelineLayout, 0, 1, &descripManager.camCubeDestriptors.descriptorSets[currentFrame], 0, nullptr);

    for (uint32_t i = 0; i < camManager.getCamCount(); i++) {
        std::array<glm::mat4x4, 2> camMatrices{};
        camMatrices[0] = glm::inverse(camManager.camArray[i].getViewMatrix());
        camMatrices[1] = camManager.activeCam->getProjectionMatrix() * camManager.activeCam->getViewMatrix();
        vkCmdPushConstants(commandBuffer, graphicsPipeline.camCubePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)*2, camMatrices.data());
        vkCmdDraw(commandBuffer, 36, 1, 0, 0);
    }

    vkCmdEndRenderPass(commandBuffer);
}

void App::recordOfflineCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    CommandRecordSetter::beginRenderPass(commandBuffer, graphicsPipeline.renderPass, framebuffer, swapchain.swapChainExtent, true);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.offlineRenderPipeline);

    CommandRecordSetter::setViewport(commandBuffer, swapchain.swapChainExtent);
    CommandRecordSetter::setScissor(commandBuffer, swapchain.swapChainExtent);

    PresentationMode mode = PresentationMode::OFFLINE_RENDER;
    if (inputManager.novelRender) {     // update novel image once for each frame
        descripManager.offlineDescriptors.updateDescriptorSets(camManager, currentFrame);       // todo setup flags when re
        mode = PresentationMode::NOVEL_RENDER;
    }
    else if (inputManager.setupOfflineImage) {      // need a trigger to once create and update the layered image
        camManager.createLayeredImage();
        descripManager.computeDescriptors.updateSharedImageDescriptor(camManager);
        std::cout << "Layered imaged created" << std::endl;

        inputManager.setupOfflineImage = false;
        // todo later whether present color or depth, flag inputManager.presentType
    }

    camManager.novelView.swapTransferLayoutRenderPresent(currentFrame, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    uniformManager.offlineUniforms.updateUniformBuffers(currentFrame, camManager, mode, inputManager.presentType);    // always update the uniform buffer

    VkDescriptorSet descriptorSets[] = {descripManager.offlineDescriptors.descriptorSets[currentFrame], descripManager.computeDescriptors.sharedDescriptorSet};
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.offlineRenderPipelineLayout, 0, 2, descriptorSets, 0, nullptr);

    vkCmdDraw(commandBuffer, 6, 1, 0, 0);   // draw a quad

    vkCmdEndRenderPass(commandBuffer);
}

void App::recordLineCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    CommandRecordSetter::beginRenderPass(commandBuffer, graphicsPipeline.onTopRenderPass, framebuffer, swapchain.swapChainExtent, false);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.intersectionPipeline);

    CommandRecordSetter::setViewport(commandBuffer, swapchain.swapChainExtent);
    CommandRecordSetter::setScissor(commandBuffer, swapchain.swapChainExtent);

    VkBuffer vertexBuffers[] = {uniformManager.novelUnifroms.intersectionsSSBOOut[currentFrame]};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.intersectionPipelineLayout, 0, 1, &descripManager.frustumDescriptors.descriptorSets[currentFrame], 0, nullptr);

    // get the number of vertices
    vkCmdDraw(commandBuffer, uniformManager.novelUnifroms.getIntersectCount(currentFrame), 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
}

void App::recordComputeCommandBuffer(VkCommandBuffer commandBuffer) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.computePipeline);

    VkDescriptorSet descriptorSets[] = {descripManager.computeDescriptors.descriptorSets[currentFrame], descripManager.computeDescriptors.sharedDescriptorSet};
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pipelineLayout, 0, 2, descriptorSets, 0, nullptr);

    // Clear buffers
    vkCmdFillBuffer(commandBuffer, uniformManager.novelUnifroms.intersectionsSSBOOut[currentFrame], 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(commandBuffer, uniformManager.novelUnifroms.intersectCountSSBOOut[currentFrame], 0, sizeof(uint32_t), 0);
    // barier here

    std::array<VkBufferMemoryBarrier, 2> fillBarriers;
    fillBarriers[0] = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = uniformManager.novelUnifroms.intersectionsSSBOOut[currentFrame],
        .offset = 0,
        .size = VK_WHOLE_SIZE
    };
    fillBarriers[1] = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = uniformManager.novelUnifroms.intersectCountSSBOOut[currentFrame],
        .offset = 0,
        .size = sizeof(uint32_t)
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 2, fillBarriers.data(), 0, nullptr);

    int32_t groupCountX = (swapchain.swapChainExtent.width  + 15) / 16;
    uint32_t groupCountY = (swapchain.swapChainExtent.height + 15) / 16;

    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

    fillBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    fillBarriers[0].dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    fillBarriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    fillBarriers[1].dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 2, fillBarriers.data(), 0, nullptr);
    // image barrier done by trasfering layout
}

void App::recordPointCloudCommandBuffer(VkCommandBuffer commandBuffer) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pointCloudPipeline);

    VkDescriptorSet descriptorSets[] = {descripManager.pointCloudDescriptors.descriptorSets[currentFrame], descripManager.computeDescriptors.sharedDescriptorSet};
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pointCloudPipelineLayout, 0, 2, descriptorSets, 0, nullptr);

    // Clear buffers
    vkCmdFillBuffer(commandBuffer, uniformManager.pointCloudUniforms.pointCloudSSBOOut[currentFrame], 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(commandBuffer, uniformManager.pointCloudUniforms.pointCountSSBOOut[currentFrame], 0, sizeof(uint32_t), 0);
    // barier here

    std::array<VkBufferMemoryBarrier, 2> fillBarriers;
    fillBarriers[0] = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = uniformManager.pointCloudUniforms.pointCloudSSBOOut[currentFrame],
        .offset = 0,
        .size = VK_WHOLE_SIZE
    };
    fillBarriers[1] = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = uniformManager.pointCloudUniforms.pointCountSSBOOut[currentFrame],
        .offset = 0,
        .size = sizeof(uint32_t)
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 2, fillBarriers.data(), 0, nullptr);

    int32_t groupCountX = (swapchain.swapChainExtent.width  + 15) / 16;
    uint32_t groupCountY = (swapchain.swapChainExtent.height + 15) / 16;

    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

    fillBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    fillBarriers[0].dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    fillBarriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    fillBarriers[1].dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 2, fillBarriers.data(), 0, nullptr);
    // image barrier done by trasfering layout
}

void App::handleResize() {
    swapchain.recreateSwapChain(vulkanContext.getDeviceSurfaceHandle(), vulkanContext.familyIndices, graphicsPipeline.renderPass);
    camManager.updateResize(swapchain.swapChainExtent);
    inputManager.imguiResize(swapchain.swapChainImageViews, swapchain.swapChainExtent);
}

void App::drawFrame() {
    // Wait for the previous frame to finish
    // Acquire an image from the swap chain
    // Record a command buffer which draws the scene onto that image
    // Submit the recorded command buffer
    // Present the swap chain image

    vkWaitForFences(vulkanContext.device, 1, &syncManager.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);    // wait for previous frame to finish

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(vulkanContext.device, swapchain.swapChain, UINT64_MAX, syncManager.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        handleResize();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }
    // Only reset when we submitting work
    vkResetFences(vulkanContext.device, 1, &syncManager.inFlightFences[currentFrame]);   // needs to be reset manually

    uniformManager.renderUniforms.updateUniformBuffers(currentFrame, *(camManager.activeCam), inputManager.debugGrayscale); // MVP matrix, could be updated on request instead

    beginCommandBuffer(commandManager.commandBuffers[currentFrame]);
    if (inputManager.presentOfflineFlag && camManager.offlineImagesRendered) {
        recordOfflineCommandBuffer(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex]);
    }
    else if (inputManager.novelRender) {
        uniformManager.novelUnifroms.updateUniformBuffers(currentFrame, camManager, swapchain.swapChainExtent, inputManager.novelDebug);

        if (uniformManager.novelUnifroms.setCamArrayData(currentFrame, camManager)) {   // enough to do once if the views dont change, but issue with the update of the both frames
            std::cout << "update CamArray data" << std::endl;
            // if the ssbo was recreated, update the descriptors
            descripManager.computeDescriptors.updateDescriptorSets(currentFrame, uniformManager.novelUnifroms.camArraySSBOIn, uniformManager.novelUnifroms.intersectionsSSBOOut);
        }

        if (inputManager.startSynthesis) {
            std::cout << "Update image data" << std::endl;
            camManager.novelView.createNovelImage(swapchain.swapChainExtent);           // get current resolution
            camManager.createLayeredImage();
            descripManager.computeDescriptors.updateImageDescriptors(camManager);       // do only once as the views won't be able to change
            descripManager.offlineDescriptors.setUpdateFlags();                         // update descriptors for presenting as well

            inputManager.startSynthesis = false;
        }

        recordComputeCommandBuffer(commandManager.commandBuffers[currentFrame]);
        // barier 

        recordOfflineCommandBuffer(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex]);
        camManager.novelView.swapTransferLayoutRenderPresent(currentFrame, VK_IMAGE_LAYOUT_GENERAL);
    }
    else {      // debug and setup
        recordCommandBuffer(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex]);

        if (inputManager.debugFrustum) {
            debugUtil.setFrustumData(camManager, currentFrame);
            recordFrustumCommandBuffer(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex]);
        }

        if (inputManager.debugCamCube) {
            recordCamCubeCommandBuffer(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex]);
        }

        if (inputManager.debugIntersection) {
            uniformManager.novelUnifroms.updateUniformBuffers(currentFrame, camManager, swapchain.swapChainExtent, DebugCompute::INTERSECTION);
            camManager.novelView.swapTransferLayoutRenderPresent(currentFrame, VK_IMAGE_LAYOUT_GENERAL);
            if (uniformManager.novelUnifroms.setCamArrayData(currentFrame, camManager)) {   // enough to do once if the views dont change
                // if the ssbo was recreated, update the descriptors
                descripManager.computeDescriptors.updateDescriptorSets(currentFrame, uniformManager.novelUnifroms.camArraySSBOIn, uniformManager.novelUnifroms.intersectionsSSBOOut);
            }

            recordComputeCommandBuffer(commandManager.commandBuffers[currentFrame]);
            recordLineCommandBuffer(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex]);
        }

        if (inputManager.debugPointCloud) {
            if (inputManager.startSynthesis) {      // redo better
                camManager.createLayeredImage();
                descripManager.computeDescriptors.updateImageDescriptors(camManager);       // do only once as the views won't be able to change
                
                inputManager.startSynthesis = false;
            }

            recordPointCloudCommandBuffer(commandManager.commandBuffers[currentFrame]);
        }
    }

    submitCommandBuffer(commandManager.commandBuffers[currentFrame]);

    VkSemaphore waitSemaphores[] = {syncManager.imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSemaphore signalSemaphores[] = {syncManager.imageFinishedSemaphores[currentFrame]};

    VkCommandBuffer imguiCommandBuffer = inputManager.recordUI(currentFrame, imageIndex);
    std::array<VkCommandBuffer, 2> submitCommandBuffers = {commandManager.commandBuffers[currentFrame], imguiCommandBuffer};
    uint32_t cmdBSize = 1;

    if (imguiCommandBuffer != VK_NULL_HANDLE) {
        cmdBSize = 2;
    }

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = cmdBSize,
        .pCommandBuffers = submitCommandBuffers.data(),
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signalSemaphores
    };

    if (vkQueueSubmit(vulkanContext.graphicsQueue, 1, &submitInfo, syncManager.inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    if (imguiCommandBuffer == VK_NULL_HANDLE) {
        // transition the image to present
        swapchain.transitionToFinalLayout(memManager, imageIndex);  // possible need to synchronize if using separate queues
    }

    VkSwapchainKHR swapChains[] = {swapchain.swapChain};
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signalSemaphores,
        .swapchainCount = 1,
        .pSwapchains = swapChains,
        .pImageIndices = &imageIndex,
        .pResults = nullptr,
    };
    result = vkQueuePresentKHR(vulkanContext.presentQueue, &presentInfo);
    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR || appWindow.framebufferResized) {
        handleResize();
        appWindow.framebufferResized = false;
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present a swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void App::renderOfflineImages() {
    if (camManager.postponeResize) {
        camManager.updateOffline(swapchain.swapChainExtent);
        camManager.postponeResize = false;
    }

    uint32_t startIndex = camManager.getActiveIndex();
    // possible to use the push constants - future work
    do {
        camManager.nextCam(true);   // sets the activeCam

        vkWaitForFences(vulkanContext.device, 1, &syncManager.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);    // wait for previous frame to finish
        vkResetFences(vulkanContext.device, 1, &syncManager.inFlightFences[currentFrame]);   // needs to be reset manually

        OfflineCamera* renderCam = dynamic_cast<OfflineCamera*>(camManager.activeCam);
        if (renderCam == nullptr) {
            std::runtime_error("Wrong camera for offline rendering!");
        }
        beginCommandBuffer(commandManager.offlineCommandBuffers[currentFrame]);
        recordCommandBuffer(commandManager.offlineCommandBuffers[currentFrame], renderCam->framebuffer);
        submitCommandBuffer(commandManager.offlineCommandBuffers[currentFrame]);
        
        uniformManager.renderUniforms.updateUniformBuffers(currentFrame, *(camManager.activeCam), false);

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandManager.offlineCommandBuffers[currentFrame],
        };

        if (vkQueueSubmit(vulkanContext.graphicsQueue, 1, &submitInfo, syncManager.inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    } while (startIndex != camManager.getActiveIndex());

    vkDeviceWaitIdle(vulkanContext.device);
    if (camManager.novelViewToggeled()){
        camManager.toggleNovel();
        camManager.toggleNovel();
    }
    if (camManager.observerToggeled()){
        camManager.toggleObserver();
        camManager.toggleObserver();
    }

    uniformManager.pointCloudUniforms.setScreenRes(swapchain.swapChainExtent);
    uniformManager.pointCloudUniforms.updateUniformBuffers(camManager);
    descripManager.pointCloudDescriptors.updateDescriptorSets(uniformManager.pointCloudUniforms);


    camManager.offlineImagesRendered = true;
}  

void App::mainLoop() {
    while (!glfwWindowShouldClose(appWindow.window)) {
        glfwPollEvents();
        inputManager.frame();
        if (inputManager.renderOfflineFlag) {
            renderOfflineImages();
            inputManager.renderOfflineFlag = false;
        }
        drawFrame();
        // break;
    }
    vkDeviceWaitIdle(vulkanContext.device);
}