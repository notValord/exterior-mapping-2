#include "application.hpp"

static unsigned int SCREEN_WIDTH = 800;
static unsigned int SCREEN_HEIGHT = 600;

static const std::string MODEL_PATH = "../resources/models/viking_room.obj";
const std::string TEXTURE_PATH = "../resources/textures/viking_room.png";

const int MAX_FRAMES_IN_FLIGHT = 2;

void App::run() {;
    initVulkan();
    mainLoop();
    cleanup();
}

App::App() 
    : appWindow(SCREEN_WIDTH, SCREEN_HEIGHT), 
       vulkanContext(appWindow.window),
       commandManager(vulkanContext.familyIndices, vulkanContext.device),
       memManager(vulkanContext.getPhysicalDeviceInstance(), commandManager.getTransferCommandPool(), vulkanContext.graphicsQueue),
       swapchain(vulkanContext.getDeviceSurfaceHandle(), vulkanContext.familyIndices, vulkanContext.device, appWindow.window, memManager),
       descripManager(vulkanContext.device),
       graphicsPipeline(vulkanContext.device, swapchain.getAttachementsFormats(), descripManager.renderDescriptors.descriptorSetLayout),
       computePipeline(vulkanContext.device, descripManager.computeDescriptors.descriptorSetLayout),
       syncManager(vulkanContext.device),
       textures(TEXTURE_PATH, vulkanContext.device, memManager, vulkanContext.getDeviceProperties()),
       mesh(MODEL_PATH, vulkanContext.device, memManager),
       camManager(vulkanContext.device, memManager, swapchain.swapChainExtent, swapchain.getAttachementsFormats(), graphicsPipeline.renderPass),
       uniforms(vulkanContext.device, memManager, swapchain.swapChainExtent, camManager.getCamCount()),
       inputManager(appWindow.window, camManager, swapchain.getAttachementsFormats(), swapchain.swapChainImageViews, vulkanContext.getPhysicalDeviceInstance(),
            vulkanContext.graphicsQueue, vulkanContext.familyIndices, swapchain.swapChainExtent),
       debugUtil(memManager, camManager.getCamCount()){
    // order of members in a class

    graphicsPipeline.setupFrustumPipeline(descripManager.frustumDescriptors.descriptorSetLayout, swapchain.getAttachementsFormats());
    graphicsPipeline.setupIntersectionPipeline(descripManager.frustumDescriptors.descriptorSetLayout, swapchain.getAttachementsFormats());

    swapchain.createFramebuffers(graphicsPipeline.renderPass);
    descripManager.createDescriptorPoolSet(uniforms.renderUniformBuffers, textures.getSamplerView(), uniforms.novelUniformBuffers, uniforms.camArraySSBOIn, uniforms.intersectionsSSBOOut, uniforms.vertexCountSSBOOut);

    inputManager.setCallbacks();
}

App::~App() {
    // reverse order of constructors
}

void App::initVulkan() {

}

void App::recordCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    std::array<VkClearValue, 2> clearColor{};
    clearColor[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearColor[1].depthStencil = {1.0f, 0};
    const VkExtent2D& swapChainExtent = swapchain.swapChainExtent;

    VkRenderPassBeginInfo renderPassBI{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = graphicsPipeline.renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0,0}, swapChainExtent},  // offset, extent
        .clearValueCount = static_cast<uint32_t>(clearColor.size()),
        .pClearValues = clearColor.data()
    };

    vkCmdBeginRenderPass(commandBuffer, &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.graphicsPipeline);
    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(swapChainExtent.width),
        .height = static_cast<float>(swapChainExtent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{
        .offset = {0, 0},
        .extent = swapChainExtent
    };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkBuffer vertexBuffers[] = {mesh.vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.pipelineLayout, 0, 1, &descripManager.renderDescriptors.descriptorSets[currentFrame], 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, mesh.getIndicesSize(), 1, 0, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
}

void App::recordFrustumCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    const VkExtent2D& swapChainExtent = swapchain.swapChainExtent;
    VkRenderPassBeginInfo renderPassBI{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = graphicsPipeline.onTopRenderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0,0}, swapChainExtent},  // offset, extent
        .clearValueCount = 0,
        .pClearValues = VK_NULL_HANDLE
    };

    vkCmdBeginRenderPass(commandBuffer, &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.frustumPipeline);
    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(swapChainExtent.width),
        .height = static_cast<float>(swapChainExtent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{
        .offset = {0, 0},
        .extent = swapChainExtent
    };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkBuffer vertexBuffers[] = {debugUtil.frustumVertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, debugUtil.frustumIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.frustumPipelineLayout, 0, 1, &descripManager.frustumDescriptors.descriptorSets[currentFrame], 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, 36 * camManager.getCamCount(), 1, 0, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
}

void App::recordLineCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    const VkExtent2D& swapChainExtent = swapchain.swapChainExtent;
    VkRenderPassBeginInfo renderPassBI{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = graphicsPipeline.onTopRenderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0,0}, swapChainExtent},  // offset, extent
        .clearValueCount = 0,
        .pClearValues = VK_NULL_HANDLE
    };

    vkCmdBeginRenderPass(commandBuffer, &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.intersectionPipeline);
    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(swapChainExtent.width),
        .height = static_cast<float>(swapChainExtent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{
        .offset = {0, 0},
        .extent = swapChainExtent
    };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkBuffer vertexBuffers[] = {uniforms.intersectionsSSBOOut[currentFrame]};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.intersectionPipelineLayout, 0, 1, &descripManager.frustumDescriptors.descriptorSets[currentFrame], 0, nullptr);

    // get the number of vertices
    vkCmdDraw(commandBuffer, uniforms.getVertexCount(currentFrame), 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
}

void App::recordComputeCommandBuffer(VkCommandBuffer commandBuffer) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.computePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pipelineLayout, 0, 1, &descripManager.computeDescriptors.descriptorSets[currentFrame], 0, nullptr);

    vkCmdFillBuffer(commandBuffer, uniforms.intersectionsSSBOOut[currentFrame], 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(commandBuffer, uniforms.vertexCountSSBOOut[currentFrame], 0, sizeof(uint32_t), 0);

    int32_t groupCountX = (swapchain.swapChainExtent.width  + 15) / 16;
    uint32_t groupCountY = (swapchain.swapChainExtent.height + 15) / 16;

    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

    VkBufferMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = uniforms.intersectionsSSBOOut[currentFrame],
        .offset = 0,
        .size = VK_WHOLE_SIZE
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
    }

void App::handleResize() {
    swapchain.recreateSwapChain(vulkanContext.getDeviceSurfaceHandle(), vulkanContext.familyIndices, graphicsPipeline.renderPass);
    camManager.updateResolution(swapchain.swapChainExtent);
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

    beginCommandBuffer(commandManager.commandBuffers[currentFrame]);
    recordCommandBuffer(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex]);
    recordFrustumCommandBuffer(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex]);
    recordComputeCommandBuffer(commandManager.commandBuffers[currentFrame]);
    recordLineCommandBuffer(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex]);
    submitCommandBuffer(commandManager.commandBuffers[currentFrame]);
    
    uniforms.updateRenderUniformBuffers(currentFrame, *(camManager.activeCam));
    uniforms.updateComputeUniformBuffers(currentFrame, camManager, swapchain.swapChainExtent);
    uniforms.setCamArrayData(camManager);
    debugUtil.setFrustumData(camManager);

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
    uint32_t startIndex = camManager.getActiveIndex();
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
        
        uniforms.updateRenderUniformBuffers(currentFrame, *(camManager.activeCam));

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

    camManager.saveOfflineImages(memManager);
}  

void App::presentOfflineImages() {
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(vulkanContext.device, swapchain.swapChain, UINT64_MAX, syncManager.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        handleResize();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    OfflineCamera* renderCam = dynamic_cast<OfflineCamera*>(camManager.activeCam);
    if (renderCam == nullptr) {
        std::runtime_error("Wrong camera for offline rendering!");
    }

    // redo
    // memManager.transitionImageLayout(swapchain.swapChainImages[imageIndex], swapchain.getAttachementsFormats().colorImageFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    // memManager.copyImage(renderCam->colorImage, swapchain.getAttachementsFormats().colorImageFormat, swapchain.swapChainImages[imageIndex], swapchain.getAttachementsFormats().colorImageFormat, {swapchain.swapChainExtent.width, swapchain.swapChainExtent.height, 1});
    // memManager.transitionImageLayout(swapchain.swapChainImages[imageIndex], swapchain.getAttachementsFormats().colorImageFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    //copy data to swapchainImage to show

    VkSwapchainKHR swapChains[] = {swapchain.swapChain};
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
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

void App::mainLoop() {
    while (!glfwWindowShouldClose(appWindow.window)) {
        glfwPollEvents();
        inputManager.frame();
        if (inputManager.renderOfflineFlag) {
            renderOfflineImages();
            inputManager.renderOfflineFlag = false;
        }
        if (inputManager.presentOfflineFlag && false) {
            presentOfflineImages();
        }
        else {
            drawFrame();
        }
        // break;
    }
    vkDeviceWaitIdle(vulkanContext.device);
}

void App::cleanup() {

}