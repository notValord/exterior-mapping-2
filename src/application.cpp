#include <application.hpp>

static unsigned int SCREEN_WIDTH = 800;
static unsigned int SCREEN_HEIGHT = 600;

static const std::string MODEL_PATH = "../resources/models/viking_room.obj";
static const std::string TEXTURE_PATH = "../resources/textures/viking_room.png";
static const std::string DAVID_TEXTURE_PATH = "../resources/textures/david.jpg";

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
       pipelineManager(vulkanContext.device, swapchain.getAttachementsFormats(), descripManager),
       syncManager(vulkanContext.device),
       textureManager(TEXTURE_PATH, CUBE_TEXTURE_PATH, vulkanContext.device, memManager, vulkanContext.getDeviceProperties()),
       mesh(MODEL_PATH, vulkanContext.device, memManager),
       camManager(vulkanContext.device, memManager, swapchain.swapChainExtent, swapchain.getAttachementsFormats(), pipelineManager.renderPassMan.renderPass, vulkanContext.getDeviceProperties()),
       uniformManager(memManager, swapchain.swapChainExtent, camManager.getCamCount()),
       inputManager(appWindow.window, camManager, swapchain.getAttachementsFormats(), swapchain.swapChainImageViews, vulkanContext.getPhysicalDeviceInstance(),
            vulkanContext.graphicsQueue, vulkanContext.familyIndices, swapchain.swapChainExtent, memManager),
       debugUtil(memManager, camManager.getCamCount()),
       commandRecorder(){
    // order of members in a class
    
    swapchain.createFramebuffers(pipelineManager.renderPassMan.renderPass);
    descripManager.createDescriptorPoolSet(uniformManager, textureManager.modelTexture.getSamplerView(), textureManager.cubeTexture.getSamplerView(), camManager);

    inputManager.setCallbacks();
}

App::~App() {
    // reverse order of constructors
}

void App::handleResize() {
    swapchain.recreateSwapChain(vulkanContext.getDeviceSurfaceHandle(), vulkanContext.familyIndices, pipelineManager.renderPassMan.renderPass);
    camManager.updateResize(swapchain.swapChainExtent);
    inputManager.imguiResize(swapchain.swapChainImageViews, swapchain.swapChainExtent);
}

void App::drawScene(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, const Camera& renderView) {
    // update both MVP and RFO, can get rid of the MVP in the future fot push constans
    uniformManager.renderUniforms.updateUniformBuffers(currentFrame, *(camManager.activeCam), inputManager.debugGrayscale); // MVP matrix

    commandRecorder.setPipeline(pipelineManager.renderPipeline, pipelineManager.renderPassMan.renderPass, swapchain.swapChainExtent);
    commandRecorder.setRenderBuffers(mesh.vertexBuffer, mesh.getIndicesSize(), mesh.indexBuffer);

    std::vector<VkDescriptorSet> descriptorSets = { descripManager.renderDescriptors.descriptorSets[currentFrame] };
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(5.0f));

    std::vector<glm::mat4> pushMats = { model, renderView.getProjectionMatrix() * renderView.getViewMatrix() };
    commandRecorder.recordRenderScene(commandBuffer, descriptorSets, framebuffer, pushMats);
}

void App::drawOffline(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    if (!camManager.imagesRendered()) {
        throw std::runtime_error("Working with images that are not rendered!");
    }

    camManager.novelView.swapTransferLayoutRenderPresent(currentFrame, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
    camManager.transferLayeredLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);      // update the layered image

    PresentationMode mode = PresentationMode::OFFLINE_RENDER;
    if (inputManager.novelRender) {
        mode = PresentationMode::NOVEL_RENDER;
    }

    uniformManager.offlineUniforms.updateUniformBuffers(currentFrame, camManager, mode, inputManager.presentType);    // always update the uniform buffer


    commandRecorder.setPipeline(pipelineManager.offlinePipeline, pipelineManager.renderPassMan.renderPass, swapchain.swapChainExtent);
    commandRecorder.setRenderBuffers(VK_NULL_HANDLE, 6, VK_NULL_HANDLE);           // draw a quad

    std::vector<VkDescriptorSet> descriptorSets = { descripManager.offlineDescriptors.descriptorSets[currentFrame],
                                                    descripManager.computeDescriptors.sharedDescriptorSet };
    commandRecorder.recordRenderScene(commandBuffer, descriptorSets, framebuffer);
}

void App::computeNovel(VkCommandBuffer commandBuffer) {
    if (!camManager.imagesRendered()) {
        throw std::runtime_error("Working with images that are not rendered!");
    }

    uniformManager.novelUnifroms.updateUniformBuffers(currentFrame, camManager, swapchain.swapChainExtent, inputManager.novelDebug, inputManager.novelHeuristic);

    if (uniformManager.novelUnifroms.setCamArrayData(currentFrame, camManager)) {   // enough to do once if the views dont change, but issue with the update of the both frames
        // if the ssbo was recreated, update the descriptors
        descripManager.computeDescriptors.updateDescriptorSets(currentFrame, uniformManager.novelUnifroms.camArraySSBOIn, uniformManager.novelUnifroms.intersectionsSSBOOut);
    }

    if (inputManager.startSynthesis) {
        camManager.novelView.createNovelImage(swapchain.swapChainExtent);           // get current resolution
        descripManager.computeDescriptors.updateImageDescriptors(camManager);       // do only once as the views won't be able to change
        descripManager.offlineDescriptors.updateDescriptorSets(camManager);         // update descriptors for presenting as well

        inputManager.startSynthesis = false;
    }

    camManager.transferLayeredLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
    camManager.novelView.swapTransferLayoutRenderPresent(currentFrame, VK_IMAGE_LAYOUT_GENERAL, commandManager.commandBuffers[currentFrame]);


    commandRecorder.setPipeline(pipelineManager.intersectPipeline);

    NovelUniforms& uniform = uniformManager.novelUnifroms;
    commandRecorder.setComputeBuffer(uniform.intersectionsSSBOOut[currentFrame], uniform.intersectCountSSBOOut[currentFrame]);

    uint32_t groupCountX = (swapchain.swapChainExtent.width  + 15) / 16;
    uint32_t groupCountY = (swapchain.swapChainExtent.height + 15) / 16;

    std::vector<VkDescriptorSet> descriptorSets = { descripManager.computeDescriptors.descriptorSets[currentFrame],
                                                    descripManager.computeDescriptors.sharedDescriptorSet };
    commandRecorder.recordCompute(commandBuffer, descriptorSets, std::make_pair(groupCountX, groupCountY));
}

void App::computePointCloud(VkCommandBuffer commandBuffer) {
    if (!camManager.imagesRendered()) {
        throw std::runtime_error("Working with images that are not rendered!");
    }
    camManager.transferLayeredLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);

    uniformManager.renderUniforms.updateUniformBuffers(currentFrame, *(camManager.activeCam), false);       // need to update the MVP
    // buffers are updated when the images are taken


    commandRecorder.setPipeline(pipelineManager.pointCloudPipeline);

    PointCloudUniforms& uniform = uniformManager.pointCloudUniforms;
    commandRecorder.setComputeBuffer(uniform.pointCloudSSBOOut[currentFrame], uniform.pointCountSSBOOut[currentFrame]);

    uint32_t groupCountX = (swapchain.swapChainExtent.width  + 15) / 16;
    uint32_t groupCountY = (swapchain.swapChainExtent.height + 15) / 16;

    std::vector<VkDescriptorSet> descriptorSets = { descripManager.pointCloudDescriptors.descriptorSets[currentFrame],
                                                    descripManager.computeDescriptors.sharedDescriptorSet };
    commandRecorder.recordCompute(commandBuffer, descriptorSets, std::make_pair(groupCountX, groupCountY));
}

void App::drawFrustumDebug(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    debugUtil.setFrustumData(camManager, currentFrame);

    commandRecorder.setPipeline(pipelineManager.frustumPipeline, pipelineManager.renderPassMan.onTopRenderPass, swapchain.swapChainExtent);
    commandRecorder.setRenderBuffers(debugUtil.frustumVertexBuffers[currentFrame], debugUtil.getFrustumIndexCount(currentFrame), debugUtil.frustumIndexBuffers[currentFrame]);

    std::vector<VkDescriptorSet> descriptorSets = { descripManager.frustumDescriptors.descriptorSets[currentFrame] };
    commandRecorder.recordRenderScene(commandBuffer, descriptorSets, framebuffer);
}

void App::drawCamCubeDebug(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    commandRecorder.setPipeline(pipelineManager.camCubePipeline, pipelineManager.renderPassMan.onTopRenderPass, swapchain.swapChainExtent);
    commandRecorder.setRenderBuffers(VK_NULL_HANDLE, 0, VK_NULL_HANDLE);

    std::vector<VkDescriptorSet> descriptorSets = { descripManager.camCubeDestriptors.descriptorSets[currentFrame] };
    commandRecorder.recordRenderCamCube(commandBuffer, descriptorSets, framebuffer, camManager);
}

void App::drawIntersectionDebug(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    commandRecorder.setPipeline(pipelineManager.linePipeline, pipelineManager.renderPassMan.onTopRenderPass, swapchain.swapChainExtent);
    commandRecorder.setRenderBuffers(uniformManager.novelUnifroms.intersectionsSSBOOut[currentFrame],
                                     uniformManager.novelUnifroms.getIntersectCount(currentFrame),
                                     VK_NULL_HANDLE);

    std::vector<VkDescriptorSet> descriptorSets = { descripManager.frustumDescriptors.descriptorSets[currentFrame] };
    commandRecorder.recordRenderScene(commandBuffer, descriptorSets, framebuffer);
}

void App::drawPointCloudDebug(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    commandRecorder.setPipeline(pipelineManager.pointPipeline, pipelineManager.renderPassMan.renderPass, swapchain.swapChainExtent);
    commandRecorder.setRenderBuffers(uniformManager.pointCloudUniforms.pointCloudSSBOOut[currentFrame],
                                     uniformManager.pointCloudUniforms.getPointCount(currentFrame),
                                     VK_NULL_HANDLE);

    std::vector<VkDescriptorSet> descriptorSets = { descripManager.frustumDescriptors.descriptorSets[currentFrame] };
    commandRecorder.recordRenderScene(commandBuffer, descriptorSets, framebuffer);
}

void App::drawDebug(VkFramebuffer framebuffer) {
    if (inputManager.debugFrustum) {
        drawFrustumDebug(commandManager.commandBuffers[currentFrame], framebuffer);
    }

    if (inputManager.debugCamCube) {
        drawCamCubeDebug(commandManager.commandBuffers[currentFrame], framebuffer);
    }

    if (inputManager.debugIntersection) {
        computeNovel(commandManager.commandBuffers[currentFrame]);
        drawIntersectionDebug(commandManager.commandBuffers[currentFrame], framebuffer);
    }
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

    if (inputManager.presentOfflineFlag) {
        drawOffline(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex]);
    }
    else if (inputManager.novelRender) {
        computeNovel(commandManager.commandBuffers[currentFrame]);
        drawOffline(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex]);
    }
    else if (inputManager.debugPointCloud) {        // debug without scene rendering
        computePointCloud(commandManager.commandBuffers[currentFrame]);
        drawPointCloudDebug(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex]);
    }
    else {      // debug and setup
        drawScene(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex], *(camManager.activeCam));
        drawDebug(swapchain.swapChainFramebuffers[imageIndex]);
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
    if (camManager.imagesInvalid()) {
        camManager.createLayeredImages();

        // update all descriptors to to the valid images
        descripManager.computeDescriptors.updateSharedImageDescriptor(camManager);

        uniformManager.pointCloudUniforms.setScreenRes(swapchain.swapChainExtent);          // update the cam count and resolution as it changed
        uniformManager.pointCloudUniforms.updateUniformBuffers(camManager);                 // recreate the buffer to the cam count and resolution
        descripManager.pointCloudDescriptors.updateDescriptorSets(uniformManager.pointCloudUniforms);       // update the descriptors to the created buffers
    }


    vkWaitForFences(vulkanContext.device, 1, &syncManager.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);    // wait for previous frame to finish
    vkResetFences(vulkanContext.device, 1, &syncManager.inFlightFences[currentFrame]);   // needs to be reset manually

    beginCommandBuffer(commandManager.offlineCommandBuffers[currentFrame]);
    camManager.transferLayeredLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, commandManager.offlineCommandBuffers[currentFrame]);

    for (const auto& cam: camManager.camArray) {
        // deal with the push constants
        drawScene(commandManager.offlineCommandBuffers[currentFrame], cam.framebuffer, cam);
    }

    submitCommandBuffer(commandManager.offlineCommandBuffers[currentFrame]);

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandManager.offlineCommandBuffers[currentFrame],
    };

    if (vkQueueSubmit(vulkanContext.graphicsQueue, 1, &submitInfo, syncManager.inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    vkDeviceWaitIdle(vulkanContext.device);

    camManager.setImagesRendered();
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