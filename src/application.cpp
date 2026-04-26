#include <application.hpp>

#include <fstream>

static unsigned int SCREEN_WIDTH = 800;
static unsigned int SCREEN_HEIGHT = 600;

static const std::string CUBE_TEXTURE_PATH = "camera.jpg";

const size_t MAX_FRAMES_IN_FLIGHT = 2;
const std::string JSON_DIR = "../setups/";

void App::run() {
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
       textureManager(CUBE_TEXTURE_PATH, vulkanContext.device, memManager, vulkanContext.getDeviceProperties()),
       mesh(vulkanContext.device, memManager, vulkanContext.getDeviceProperties()),
       camManager(vulkanContext.device,
                  memManager,
                  swapchain.swapChainExtent,
                  swapchain.getAttachementsFormats(),
                  pipelineManager.renderPassMan.renderPass,
                  vulkanContext.getDeviceProperties(),
                  swapchain.swapChainImageViews),
       uniformManager(memManager, vulkanContext.device, swapchain.swapChainExtent, camManager.getCamCount(), vulkanContext.getDeviceProperties()),
       inputManager(appWindow, camManager, swapchain.getAttachementsFormats(), swapchain.swapChainImageViews, vulkanContext.getPhysicalDeviceInstance(),
            vulkanContext.graphicsQueue, vulkanContext.familyIndices, swapchain.swapChainExtent, memManager, mesh),
       debugUtil(vulkanContext.device, memManager, camManager.getCamCount(), vulkanContext.getDeviceProperties().limits.timestampPeriod),
       commandRecorder(){
    // order of members in a class
    
    swapchain.createFramebuffers(pipelineManager.renderPassMan.renderPass);
    descripManager.createDescriptorPoolSet(uniformManager, mesh.getMeshUniforms(), textureManager.getSamplerView(), camManager);

    inputManager.setCallbacks();
}

App::~App() {
    // reverse order of constructors
}

void App::handleResize() {
    swapchain.recreateSwapChain(vulkanContext.getDeviceSurfaceHandle(), vulkanContext.familyIndices, pipelineManager.renderPassMan.renderPass);
    camManager.updateResize(swapchain.swapChainExtent, swapchain.swapChainImageViews);
    descripManager.pointCloudDescriptors.updateImageDescriptorSets(camManager.getNovelStorageViews());      // not sure where to update
    inputManager.imguiResize(swapchain.swapChainImageViews, swapchain.swapChainExtent);
}

void App::drawScene(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, const Camera& renderView) {
    // update both MVP and RFO, can get rid of the MVP in the future fot push constans
    uniformManager.renderUniforms.updateUniformBuffers(currentFrame, *(camManager.activeCam), inputManager.debugGrayscale, mesh.getLight(), mesh.scale); // MVP matrix

    commandRecorder.setPipeline(pipelineManager.renderPipeline, pipelineManager.renderPassMan.renderPass, swapchain.swapChainExtent);
    commandRecorder.setRenderBuffers(mesh.vertexBuffer, mesh.getIndicesSize(), mesh.indexBuffer);

    std::vector<VkDescriptorSet> descriptorSets = { descripManager.renderDescriptors.descriptorSets[currentFrame] };
    glm::mat4 model = renderView.getProjectionMatrix() * renderView.getViewMatrix() * glm::scale(glm::mat4(1.0f), glm::vec3(mesh.scale));

    std::vector<glm::mat4> pushMats = { model };
    commandRecorder.recordMultiScene(commandBuffer, descriptorSets, framebuffer, pushMats, mesh.getSubMeshes(), descripManager.renderDescriptors.samplerDescriptorSets);

    // draw transparent objects separatly
    commandRecorder.setPipeline(pipelineManager.renderTransparentPipeline, pipelineManager.renderPassMan.onTopRenderPass, swapchain.swapChainExtent);
    commandRecorder.setRenderBuffers(mesh.vertexBuffer, mesh.getIndicesSize(), mesh.indexBuffer);
    commandRecorder.recordMultiScene(commandBuffer, descriptorSets, framebuffer, pushMats, mesh.getTransparentSubMeshes(), descripManager.renderDescriptors.samplerDescriptorSets, false);
}

void App::drawOffline(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    if (!camManager.imagesRendered()) {
        throw std::runtime_error("Working with images that are not rendered!");
    }

    camManager.transferLayeredLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);      // update the layered image

    PresentationMode mode = PresentationMode::OFFLINE_RENDER;
    if (inputManager.novelRender | inputManager.newNovelRender) {
        mode = PresentationMode::NOVEL_RENDER;
    }

    uniformManager.offlineUniforms.updateUniformBuffers(currentFrame, camManager, mode, inputManager.presentType);    // always update the uniform buffer
    if (inputManager.novelRender) { // update set between two novel views
        camManager.novelView.swapTransferLayoutRenderPresent(currentFrame, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
        descripManager.offlineDescriptors.updateDescriptorSets(currentFrame, camManager);
    }
    else if (inputManager.newNovelRender) {
        uniformManager.novelReconUniforms.transferResultLayout(currentFrame, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
        descripManager.offlineDescriptors.updateDescriptorSets(currentFrame, uniformManager.novelReconUniforms);
    }

    commandRecorder.setPipeline(pipelineManager.offlinePipeline, pipelineManager.renderPassMan.renderPass, swapchain.swapChainExtent);
    commandRecorder.setRenderBuffers(VK_NULL_HANDLE, 6, VK_NULL_HANDLE);           // draw a quad

    std::vector<VkDescriptorSet> descriptorSets = { descripManager.offlineDescriptors.descriptorSets[currentFrame],
                                                    descripManager.computeDescriptors.sharedDescriptorSet };
    commandRecorder.recordRenderScene(commandBuffer, descriptorSets, framebuffer);
}

void App::computeNovel(VkCommandBuffer commandBuffer, const DebugCompute debug) {
    if (!camManager.imagesRendered() && !inputManager.debugIntersection) {
        throw std::runtime_error("Working with images that are not rendered!");
    }

    uniformManager.novelUniforms.updateUniformBuffers(currentFrame, camManager, swapchain.swapChainExtent, inputManager, debug);

    if (uniformManager.novelUniforms.setCamArrayData(currentFrame, camManager)) {   // enough to do once if the views dont change, but issue with the update of the both frames
        // if the ssbo was recreated, update the descriptors
        descripManager.computeDescriptors.updateDescriptorSets(currentFrame, uniformManager.novelUniforms.camArraySSBOIn, uniformManager.novelUniforms.intersectionsSSBOOut);
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

    NovelUniforms& uniform = uniformManager.novelUniforms;
    commandRecorder.setComputeBuffer(uniform.intersectionsSSBOOut[currentFrame], uniform.intersectCountSSBOOut[currentFrame]);

    uint32_t groupCountX = (swapchain.swapChainExtent.width  + 15) / 16;
    uint32_t groupCountY = (swapchain.swapChainExtent.height + 15) / 16;

    std::vector<VkDescriptorSet> descriptorSets = { descripManager.computeDescriptors.descriptorSets[currentFrame],
                                                    descripManager.computeDescriptors.sharedDescriptorSet };
    if (inputManager.timingStarted[currentFrame]) {
        commandRecorder.recordCompute(commandBuffer, descriptorSets, std::make_pair(groupCountX, groupCountY), debugUtil.timestampQueryPool, currentFrame * debugUtil.timestampPerFrame);
    }
    else {
        commandRecorder.recordCompute(commandBuffer, descriptorSets, std::make_pair(groupCountX, groupCountY), VK_NULL_HANDLE);
    }
}

void App::computeReducedDepthPyramid(VkCommandBuffer commandBuffer) {
    camManager.createMipMapImages(commandBuffer);
    descripManager.reduceDescriptors.updateDescriptorSets(camManager.getSampler(ImageViewType::DEPTH), camManager.getReduceDepthViews());

    commandRecorder.setPipeline(pipelineManager.reduceDepthPipeline);
    std::vector<VkDescriptorSet> descriptorSets = { descripManager.reduceDescriptors.descriptorSets[0] };        // static single desriptor set

    commandRecorder.recordCompute(commandBuffer, descriptorSets, std::make_pair(camManager.getCamCount(), 32/8), VK_NULL_HANDLE);     // 8 tiles worked per workgroup, 32 in the image

    commandRecorder.barrierStorageImage(commandBuffer, camManager.getReduceStorageImages(), camManager.getCamCount());
}

void App::computeNewNovel(VkCommandBuffer commandBuffer) {
    if (inputManager.startSynthesis) {
        std::cout << "Starting novel synthesis..." << std::endl;
        computeReducedDepthPyramid(commandBuffer);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (uniformManager.novelUniforms.setCamArrayData(i, camManager)) {   // resolve differently
                // if the ssbo was recreated, update the descriptors
                descripManager.computeDescriptors.updateDescriptorSets(i, uniformManager.novelUniforms.camArraySSBOIn, uniformManager.novelUniforms.intersectionsSSBOOut);
            }
        }

        uniformManager.novelSynthUniforms.setCamCount(camManager.getCamCount());        // should be updated per camera count change
        descripManager.novelSynthDescriptors.updatePerCamDescriptorSets(uniformManager.novelUniforms.camArraySSBOIn, uniformManager.novelSynthUniforms);

        descripManager.novelReconDescriptors.updateRefImageDescritporSets(camManager);
        camManager.transferLayeredLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);

        inputManager.startSynthesis = false;
    }

    if (inputManager.timingStarted[currentFrame]) {     // timing for debug
        commandRecorder.setPipeline(pipelineManager.reduceDepthPipeline);
        std::vector<VkDescriptorSet> descriptorSets = { descripManager.reduceDescriptors.descriptorSets[0] };        // static single desriptor set

        commandRecorder.recordCompute(commandBuffer, descriptorSets, std::make_pair(camManager.getCamCount(), 32/8), debugUtil.timestampQueryPool, currentFrame * debugUtil.timestampPerFrame);
        commandRecorder.barrierStorageImage(commandBuffer, camManager.getReduceStorageImages(), camManager.getCamCount());
    }

    if (uniformManager.rayDataUniforms.updateUniformBuffers(currentFrame, *camManager.activeCam, swapchain.swapChainExtent)) {
        // storage buffer was recreated, update descritpors
        descripManager.rayDataDescriptors.updateStorageDescriptorSets(uniformManager.rayDataUniforms, currentFrame);
    }

    if (uniformManager.novelSynthUniforms.setResolution(swapchain.swapChainExtent, currentFrame)) {
        descripManager.novelSynthDescriptors.updatePerResizeDescriptorSets(uniformManager.novelSynthUniforms.resultImageView, currentFrame);
    }

    uniformManager.novelReconUniforms.updateUniformBuffers(currentFrame, camManager.getCamCount(), swapchain.swapChainExtent);
    if (uniformManager.novelReconUniforms.setResolution(swapchain.swapChainExtent, currentFrame)) {
        descripManager.novelReconDescriptors.updateResultDescriptorSets(uniformManager.novelReconUniforms.resultImageView, currentFrame);
    }
    uniformManager.novelReconUniforms.transferResultLayout(currentFrame, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);

    uint32_t groupCountX = (swapchain.swapChainExtent.width  + 15) / 16;
    uint32_t groupCountY = (swapchain.swapChainExtent.height + 15) / 16;

    commandRecorder.setPipeline(pipelineManager.rayDataPipeline);
    std::vector<VkDescriptorSet> descriptorSets = { descripManager.rayDataDescriptors.descriptorSets[currentFrame],
                                                    descripManager.rayDataDescriptors.storageDescriptorSets[currentFrame] };
    commandRecorder.setComputeBuffer(uniformManager.rayDataUniforms.rayDataSSBOIn[currentFrame], uniformManager.rayDataUniforms.rayCountSSBOIn[currentFrame]);
    if (inputManager.timingStarted[currentFrame]) {
        commandRecorder.recordCompute(commandBuffer, descriptorSets, std::make_pair(groupCountX, groupCountY), debugUtil.timestampQueryPool, currentFrame * debugUtil.timestampPerFrame + 2);
    }
    else {
        commandRecorder.recordCompute(commandBuffer, descriptorSets, std::make_pair(groupCountX, groupCountY), VK_NULL_HANDLE);
    }

    // image barrier
    // rayData barrier
    commandRecorder.barrierStorageBuffer(commandBuffer, 
                                         uniformManager.rayDataUniforms.rayDataSSBOIn[currentFrame],
                                         VK_ACCESS_SHADER_WRITE_BIT,
                                         VK_ACCESS_SHADER_READ_BIT,
                                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    groupCountX = (swapchain.swapChainExtent.width * swapchain.swapChainExtent.height + 7) / 8;      // every eitght thread
    groupCountY = camManager.getCamCount();
    commandRecorder.setPipeline(pipelineManager.novelSynthPipeline);
    descriptorSets = { descripManager.reduceDescriptors.descriptorSets[0],
                       descripManager.novelSynthDescriptors.descriptorSets[currentFrame],
                       descripManager.rayDataDescriptors.storageDescriptorSets[currentFrame],
                       descripManager.novelSynthDescriptors.debugDescriptorSets[currentFrame]};
    if (inputManager.timingStarted[currentFrame]) {
        commandRecorder.recordCompute(commandBuffer, descriptorSets, std::make_pair(groupCountX, groupCountY), debugUtil.timestampQueryPool, currentFrame * debugUtil.timestampPerFrame + 4);
    }
    else {
        commandRecorder.recordCompute(commandBuffer, descriptorSets, std::make_pair(groupCountX, groupCountY), VK_NULL_HANDLE);
    }

    //image barrier
    commandRecorder.barrierStorageImage(commandBuffer, {uniformManager.novelSynthUniforms.getResultImages()[currentFrame]}, camManager.getCamCount());

    groupCountX = (swapchain.swapChainExtent.width  + 15) / 16;
    groupCountY = (swapchain.swapChainExtent.height + 15) / 16;

    commandRecorder.setPipeline(pipelineManager.novelReconPipeline);
    descriptorSets = { descripManager.novelReconDescriptors.descriptorSets[currentFrame],
                       descripManager.novelSynthDescriptors.debugDescriptorSets[currentFrame],
                       descripManager.novelReconDescriptors.resultDescriptorSets[currentFrame]};
    if (inputManager.timingStarted[currentFrame]) {
        commandRecorder.recordCompute(commandBuffer, descriptorSets, std::make_pair(groupCountX, groupCountY), debugUtil.timestampQueryPool, currentFrame * debugUtil.timestampPerFrame + 6);
    }
    else {
        commandRecorder.recordCompute(commandBuffer, descriptorSets, std::make_pair(groupCountX, groupCountY), VK_NULL_HANDLE);
    }

    // Ensure compute shader writes to result image are complete and visible before layout transition
    commandRecorder.barrierStorageImage(commandBuffer, {uniformManager.novelReconUniforms.getResultImage(currentFrame)}, 1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void App::computePointCloud(VkCommandBuffer commandBuffer) {
    if (!camManager.imagesRendered()) {
        throw std::runtime_error("Working with images that are not rendered!");
    }
    camManager.transferLayeredLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);

    uniformManager.renderUniforms.updateUniformBuffers(currentFrame, *(camManager.activeCam), false, mesh.getLight(), mesh.scale);       // need to update the MVP
    // buffers are updated when the images are taken


    commandRecorder.setPipeline(pipelineManager.pointCloudPipeline);

    PointCloudUniforms& uniform = uniformManager.pointCloudUniforms;
    commandRecorder.setComputeBuffer(uniform.pointCloudSSBOOut[currentFrame], uniform.pointCountSSBOOut[currentFrame]);

    uint32_t groupCountX = (swapchain.swapChainExtent.width  + 15) / 16;
    uint32_t groupCountY = (swapchain.swapChainExtent.height + 15) / 16;

    std::vector<VkDescriptorSet> descriptorSets = { descripManager.pointCloudDescriptors.descriptorSets[currentFrame],
                                                    descripManager.computeDescriptors.sharedDescriptorSet };
    commandRecorder.recordCompute(commandBuffer, descriptorSets, std::make_pair(groupCountX, groupCountY), VK_NULL_HANDLE);
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

    std::vector<VkDescriptorSet> descriptorSets = { descripManager.camCubeDescriptors.descriptorSets[currentFrame] };
    commandRecorder.recordRenderCamCube(commandBuffer, descriptorSets, framebuffer, camManager);
}

void App::drawIntersectionDebug(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    commandRecorder.setPipeline(pipelineManager.linePipeline, pipelineManager.renderPassMan.onTopRenderPass, swapchain.swapChainExtent);
    commandRecorder.setRenderBuffers(uniformManager.novelUniforms.intersectionsSSBOOut[currentFrame],
                                     uniformManager.novelUniforms.getIntersectCount(currentFrame),
                                     VK_NULL_HANDLE);

    std::vector<VkDescriptorSet> descriptorSets = { descripManager.frustumDescriptors.descriptorSets[currentFrame] };
    commandRecorder.recordRenderScene(commandBuffer, descriptorSets, framebuffer);
}

void App::drawPointCloudDebug(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer) {
    commandRecorder.clearStorageImage(commandBuffer, camManager.getNovelStorageImage(currentFrame));                 // clear the image beforehand

    commandRecorder.setPipeline(pipelineManager.pointPipeline, pipelineManager.renderPassMan.renderPass, swapchain.swapChainExtent);
    commandRecorder.setRenderBuffers(uniformManager.pointCloudUniforms.pointCloudSSBOOut[currentFrame],
                                     uniformManager.pointCloudUniforms.getPointCount(currentFrame),
                                     VK_NULL_HANDLE);

    std::vector<VkDescriptorSet> descriptorSets = { descripManager.frustumDescriptors.descriptorSets[currentFrame], descripManager.pointCloudDescriptors.imageDescriptorSets[currentFrame] };
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
        computeNovel(commandManager.commandBuffers[currentFrame], DebugCompute::INTERSECTION);
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
    
    if (inputManager.timingStarted[currentFrame]) {
        if (inputManager.novelRender) {
            debugUtil.getTimeStamp(currentFrame * debugUtil.timestampPerFrame);     // read 0-1
        }

        if (inputManager.newNovelRender) {
            debugUtil.getTimeStamp(currentFrame * debugUtil.timestampPerFrame);     // read 0-1 pyramidCOnstruct
            debugUtil.getTimeStamp(currentFrame * debugUtil.timestampPerFrame + 2); // read 2-3 rayData
            debugUtil.getTimeStamp(currentFrame * debugUtil.timestampPerFrame + 4); // read 4-5 novelCompute
            debugUtil.getTimeStamp(currentFrame * debugUtil.timestampPerFrame + 6); // read 4-5 novelReconstruct
        }

        if (!inputManager.timeRender) {
            debugUtil.recordedCount = 0;   // reset count
            inputManager.timingStarted[currentFrame] = false;

            if (inputManager.novelRender) {
                std::cout << "Novel Render Timestamps" << std::endl;
                debugUtil.printTimestamp(0);
            }
            else if (inputManager.newNovelRender) {
                std::cout << "Analytic Novel Render Timestamps" << std::endl;
                std::cout << "Construct depth pyramid" << std::endl;
                debugUtil.printTimestamp(0);
                std::cout << "Get ray data" << std::endl;
                debugUtil.printTimestamp(1);
                std::cout << "Compute sample data" << std::endl;
                debugUtil.printTimestamp(2);
                std::cout << "Image reconstruct" << std::endl;
                debugUtil.printTimestamp(3);
            }
        }
        else if (++debugUtil.recordedCount >= debugUtil.recordTimeCount-1) {       // stop
            inputManager.timeRender = false;
            inputManager.timingStarted[currentFrame] = false;
        }
    }

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

    if (inputManager.timeRender) {
        vkCmdResetQueryPool(commandManager.commandBuffers[currentFrame], debugUtil.timestampQueryPool, currentFrame * debugUtil.timestampPerFrame, debugUtil.timestampPerFrame);
        inputManager.timingStarted[currentFrame] = true;
        std::cout << "Capture timestamp" << std::endl;
    }
    
    if (inputManager.presentOfflineFlag) {
        drawOffline(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex]);
    }
    else if (inputManager.novelRender) {
        computeNovel(commandManager.commandBuffers[currentFrame], inputManager.novelDebug);
        drawOffline(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex]);
    }
    else if (inputManager.newNovelRender) {
        computeNewNovel(commandManager.commandBuffers[currentFrame]);
        // drawScene(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex], *(camManager.activeCam));
        drawOffline(commandManager.commandBuffers[currentFrame], swapchain.swapChainFramebuffers[imageIndex]);
    }
    else if (inputManager.debugPointCloud) {        // debug without scene rendering
        computePointCloud(commandManager.commandBuffers[currentFrame]);
        drawPointCloudDebug(commandManager.commandBuffers[currentFrame], camManager.getNovelFramebuffer(imageIndex));
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

    if (inputManager.saveCamSnapshots) {
        vkDeviceWaitIdle(vulkanContext.device);     // need to wait for the rendering to finish before saving
        swapchain.saveImageToPNG(imageIndex, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, inputManager.snapshotCamFile);
        inputManager.saveCamSnapshots = false;
    }

    if (inputManager.saveGT) {        // save for GT
        vkDeviceWaitIdle(vulkanContext.device);
        swapchain.saveGTImage(imageIndex, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        inputManager.saveGT = false;
    }
    if (inputManager.compareToGT) {        // compare image with GT
        if (!swapchain.isGTvalid()) {
            std::cout << "GT image is not valid, cannot compare." << std::endl;
        } else {
            vkDeviceWaitIdle(vulkanContext.device);
            std::cout << "MSE precision: ";
            std::cout << swapchain.renderPrecision(imageIndex, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1) << std::endl;  // 0 for MSE
        }
        inputManager.compareToGT = false;
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
    if (camManager.imagesInvalid()) {       // resolution or cam count changed
        camManager.createLayeredImages();

        // update all descriptors to to the valid images
        descripManager.computeDescriptors.updateSharedImageDescriptor(camManager);

        uniformManager.pointCloudUniforms.setScreenRes(swapchain.swapChainExtent);          // update the cam count and resolution as it changed
        uniformManager.pointCloudUniforms.updateUniformBuffers(camManager);                 // recreate the buffer to the cam count and resolution
        descripManager.pointCloudDescriptors.updateDescriptorSets(uniformManager.pointCloudUniforms);       // update the descriptors to the created buffers
    }
    uniformManager.pointCloudUniforms.updateStorageBuffers(camManager);                 // update the buffer data to the current camera matrices

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

void App::changeScene() {
    vkDeviceWaitIdle(vulkanContext.device);

    mesh.changeModel(inputManager.sceneSelected);
    descripManager.renderDescriptors.updateMeshData(mesh.getMeshUniforms());
}

void App::saveSetup(const std::string jsonFile) {
    json j;

    SceneJson scene = {mesh.getLoadedModels()[inputManager.sceneSelected], mesh.scale};
    j["scene"] = scene;

    j["lightPos"] = {mesh.getLight().x, mesh.getLight().y, mesh.getLight().z};
    j["resolution"] = {appWindow.getWidth(), appWindow.getHeight()};


    j["cameras"] = camManager.jsonSetup();

    std::ofstream file(JSON_DIR + jsonFile);
    file << j.dump(4); // pretty print with 4 spaces

    file.close();
}

void App::loadSetup(const std::string jsonFile) {
    std::ifstream file(JSON_DIR + jsonFile);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open setup file!");
    }

    json j;
    file >> j;

    SceneJson scene = j["scene"].get<SceneJson>();
    uint32_t sceneID = mesh.getSceneID(scene.name);
    if (sceneID != inputManager.sceneSelected) {
        inputManager.sceneSelected = sceneID;
        changeScene();
    }
    mesh.scale = scene.scale;

    std::vector<float> lightPos = j["lightPos"].get<std::vector<float>>();
    mesh.setLight(glm::vec3(lightPos[0], lightPos[1], lightPos[2]));

    std::vector<int> resolution = j["resolution"].get<std::vector<int>>();
    glfwSetWindowSize(appWindow.window, resolution[0], resolution[1]);

    camManager.loadFromJson(j["cameras"], memManager);

    inputManager.bestNCount = camManager.getCamCount();     // reset the best n count for the novel render

    file.close();
}

void App::printTestHeadline(std::ofstream& filename) {
    filename << "Scene;Cameras;FOV;Algorithm;SampleCount;Width;Height;Timestamps;MSE;SSIM" << std::endl;      
}

void App::printTestLine(std::ofstream& filename) {
    std::string sceneName = mesh.getLoadedModels()[inputManager.sceneSelected];
    uint32_t camCount = camManager.getCamCount();
    float fov = camManager.camArray[0].getFOV();
    std::string algorithm;
    if (inputManager.novelRender) {
        if (inputManager.novelHeuristic == NovelHeuristic::COLOR_HEURISTIC) {
            algorithm = "NovelColor";
        }
        else if (inputManager.novelHeuristic == NovelHeuristic::DEPTH_HEURISTIC) {
            algorithm = "NovelDepth";
        }
        else {
            algorithm = "NovelAngle";
        }
    } else if (inputManager.newNovelRender) {
        algorithm = "NovelAnalytic";
    } else if (inputManager.debugPointCloud) {
        algorithm = "PointCloud";
    }
    else {
        algorithm = "Unknown";
    }
    uint32_t sampleCount = (inputManager.novelRender) ? camManager.sampleCount : 0;
    uint32_t width = swapchain.swapChainExtent.width;
    uint32_t height = swapchain.swapChainExtent.height;

    filename << sceneName << ";" << camCount << ";" << glm::degrees(fov) << ";" << algorithm << ";" << sampleCount << ";" << width << ";" << height << ";";
}

void App::setupTest(std::ofstream& filename, std::string setup, bool precision) {
    printTestHeadline(filename);
    
    loadSetup(setup + ".json");
    vkDeviceWaitIdle(vulkanContext.device);

    // Resize callbacks are asynchronous; sync swapchain extent to current framebuffer size.
    for (uint32_t attempt = 0; attempt < 8; attempt++) {
        glfwPollEvents();

        int fbWidth = 0;
        int fbHeight = 0;
        glfwGetFramebufferSize(appWindow.window, &fbWidth, &fbHeight);

        if (fbWidth <= 0 || fbHeight <= 0) {
            continue;
        }

        if (!appWindow.framebufferResized &&
            swapchain.swapChainExtent.width == static_cast<uint32_t>(fbWidth) &&
            swapchain.swapChainExtent.height == static_cast<uint32_t>(fbHeight)) {
            break;
        }

        handleResize();
        appWindow.framebufferResized = false;
    }
    std::cout << swapchain.swapChainExtent.width << "x" << swapchain.swapChainExtent.height << std::endl;

    renderOfflineImages();     // render the images for the current setup

    testPrecision = precision;

    inputManager.turnUIoff();    // make sure not to save during the test

    if (testPrecision) {
        // Capture baseline frame as ground truth for this setup.
        inputManager.novelRender = false;
        inputManager.newNovelRender = false;
        inputManager.debugPointCloud = false;
        inputManager.saveGT = true;

        drawFrame();
        vkDeviceWaitIdle(vulkanContext.device);


        if (inputManager.saveGT) {
            throw std::runtime_error("Failed to capture GT image during setup.");
        }
    }

    while (!inputManager.testStep) {
        glfwPollEvents();
        inputManager.frame();
    }
    inputManager.testStep = false;

    std::cout << "Test setup done, starting tests..." << std::endl;
    vkDeviceWaitIdle(vulkanContext.device);
}

void App::runTest(std::ofstream& filename, TestInfo testInfo) {
    // cleanup
    inputManager.novelRender = false;
    inputManager.newNovelRender = false;
    inputManager.debugPointCloud = false;
    inputManager.presentOfflineFlag = false;

    bool fovChanged = false;
    bool resolutionChanged = false;

    std::cout << "Current FOV: " << camManager.getCamArrayFOV() << std::endl;
    if (testInfo.fov >= 0.0f && camManager.getCamArrayFOV() != testInfo.fov) {
        std::cout << "Changing FOV to " << testInfo.fov << "..." << std::endl;
        camManager.setCamArrayFOV(testInfo.fov);
        fovChanged = true;
        // descripManager.renderDescriptors.updateMeshData(mesh.getMeshUniforms());     // update the descriptors to update the push constants
    }

    if (testInfo.width > 0 && testInfo.height > 0 && (swapchain.swapChainExtent.width != testInfo.width || swapchain.swapChainExtent.height != testInfo.height)) {
        std::cout << "Changing resolution to " << testInfo.width << "x" << testInfo.height << "..." << std::endl;
        glfwSetWindowSize(appWindow.window, testInfo.width, testInfo.height);
        resolutionChanged = true;
    }

    if (resolutionChanged) {
        // Resize callbacks are asynchronous; sync swapchain extent to current framebuffer size.
        for (uint32_t attempt = 0; attempt < 8; attempt++) {
            glfwPollEvents();

            int fbWidth = 0;
            int fbHeight = 0;
            glfwGetFramebufferSize(appWindow.window, &fbWidth, &fbHeight);

            if (fbWidth <= 0 || fbHeight <= 0) {
                continue;
            }

            if (!appWindow.framebufferResized &&
                swapchain.swapChainExtent.width == static_cast<uint32_t>(fbWidth) &&
                swapchain.swapChainExtent.height == static_cast<uint32_t>(fbHeight)) {
                break;
            }

            handleResize();
            appWindow.framebufferResized = false;
        }
    }

    if (fovChanged || resolutionChanged) {
        std::cout << "Re-rendering offline images for the new setup..." << std::endl;
        vkDeviceWaitIdle(vulkanContext.device);
        renderOfflineImages();

        if (testPrecision && resolutionChanged) {
            // Capture new baseline frame as ground truth for this setup.
            inputManager.saveGT = true;
            drawFrame();
            vkDeviceWaitIdle(vulkanContext.device);

            if (inputManager.saveGT) {
                throw std::runtime_error("Failed to recapture GT image after resize.");
            }
        }
    }

    if (testInfo.sampleCount >= 0) {
        std::cout << "Setting sample count to " << testInfo.sampleCount << "..." << std::endl;
        camManager.sampleCount = testInfo.sampleCount;
    }

    if (testInfo.algorithm == "NovelColor") {
        inputManager.novelRender = true;
        inputManager.novelHeuristic = NovelHeuristic::COLOR_HEURISTIC;
        inputManager.startSynthesis = true;
        std::cout << "Running Novel Color Heuristic Test..." << std::endl;
    }
    else if (testInfo.algorithm == "NovelDepth") {
        inputManager.novelRender = true;
        inputManager.novelHeuristic = NovelHeuristic::DEPTH_HEURISTIC;
        inputManager.startSynthesis = true;
        std::cout << "Running Novel Depth Heuristic Test..." << std::endl;
    }
    else if (testInfo.algorithm == "NovelAngle") {
        inputManager.novelRender = true;
        inputManager.novelHeuristic = NovelHeuristic::ANGLE_HEURISTIC;
        inputManager.startSynthesis = true;
        std::cout << "Running Novel Angle Heuristic Test..." << std::endl;
    }
    else if(testInfo.algorithm == "NovelAnalytic") {
        inputManager.newNovelRender = true;
        inputManager.startSynthesis = true;
        std::cout << "Running Novel Analytic Test..." << std::endl;
    }
    else if (testInfo.algorithm == "PointCloud") {
        inputManager.debugPointCloud = true;
        std::cout << "Running Point Cloud Test..." << std::endl;
    }
    else {
        throw std::runtime_error("Unknown algorithm in test info!");
    }

    printTestLine(filename);

    const uint32_t warmupFrames = 2;
    const uint32_t measuredFrames = 10;

    inputManager.timeRender = true;

    for (uint32_t i = 0; i < warmupFrames + measuredFrames; i++) {
        inputManager.compareToGT = (testPrecision && i >= warmupFrames);     // compare to GT after the warmup frames
        drawFrame();
        vkDeviceWaitIdle(vulkanContext.device);
        while (!inputManager.testStep) {
            glfwPollEvents();
            inputManager.frame();
        }
        inputManager.testStep = false;
    }

    filename << std::endl;

}

void App::mainLoop() {
    while (!glfwWindowShouldClose(appWindow.window)) {
        glfwPollEvents();
        inputManager.frame();
        if (inputManager.saveSetupFlag) {
            saveSetup(inputManager.setupName + ".json");
            inputManager.saveSetupFlag = false;
        }
        if (inputManager.loadSetupFlag) {
            loadSetup(inputManager.setupNameLoad + ".json");
            inputManager.loadSetupFlag = false;
        }

        if (inputManager.sceneChanged) {
            changeScene();
            inputManager.sceneChanged = false;
        }
        if (inputManager.renderOfflineFlag) {
            renderOfflineImages();
            inputManager.renderOfflineFlag = false;
        }
        drawFrame();
        // break;
    }
    vkDeviceWaitIdle(vulkanContext.device);
}