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
       memManager(vulkanContext.getPhysicalDeviceInstance(), commandManager.graphicsCommandPool, vulkanContext.graphicsQueue),
       swapchain(vulkanContext.getDeviceSurfaceHandle(), vulkanContext.familyIndices, vulkanContext.device, appWindow.window, memManager),
       descripManager(vulkanContext.device),
       graphicsPipeline(vulkanContext.device, swapchain.getAttachementsFormats(), descripManager.descriptorSetLayout),
       syncManager(vulkanContext.device),
       textures(TEXTURE_PATH, vulkanContext.device, memManager, vulkanContext.getDeviceProperties()),
       mesh(MODEL_PATH, vulkanContext.device, memManager),
       uniforms(vulkanContext.device, memManager),
       camManager(swapchain.getExtentRatio()),
       inputManager(appWindow.window, camManager, swapchain.getAttachementsFormats(), swapchain.swapChainImageViews, vulkanContext.getPhysicalDeviceInstance(),
         vulkanContext.graphicsQueue, vulkanContext.familyIndices, swapchain.swapChainExtent){
    // order of members in a class

    swapchain.createFramebuffers(graphicsPipeline.renderPass);
    descripManager.createDescriptorPoolSet(uniforms.uniformBuffers, textures.getSamplerView());

    inputManager.setCallbacks();
}

App::~App() {
    // reverse order of constructors
}

void App::initVulkan() {
}

void App::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo commandBufferBI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };

    if (vkBeginCommandBuffer(commandBuffer, &commandBufferBI) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    std::array<VkClearValue, 2> clearColor{};
    clearColor[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearColor[1].depthStencil = {1.0f, 0};
    const VkExtent2D& swapChainExtent = swapchain.swapChainExtent;
    VkRenderPassBeginInfo renderPassBI{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = graphicsPipeline.renderPass,
        .framebuffer = swapchain.swapChainFramebuffers[imageIndex],
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

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.pipelineLayout, 0, 1, &descripManager.descriptorSets[currentFrame], 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, mesh.getIndicesSize(), 1, 0, 0, 0);
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
}

void App::handleResize() {
    swapchain.recreateSwapChain(vulkanContext.getDeviceSurfaceHandle(), vulkanContext.familyIndices, graphicsPipeline.renderPass);
    camManager.updateRatios(swapchain.getExtentRatio());
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

    vkResetCommandBuffer(commandManager.commandBuffers[currentFrame], 0);
    recordCommandBuffer(commandManager.commandBuffers[currentFrame], imageIndex);
    
    uniforms.updateUniformBuffers(currentFrame, *(camManager.activeCam));

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

void App::mainLoop() {
    while (!glfwWindowShouldClose(appWindow.window)) {
        glfwPollEvents();
        inputManager.frame();
        drawFrame();
    }

    vkDeviceWaitIdle(vulkanContext.device);
}

void App::cleanup() {

}