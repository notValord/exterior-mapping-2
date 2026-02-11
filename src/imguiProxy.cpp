#include <imguiProxy.hpp>
#include <swapchain.hpp>
#include <vulkanContext.hpp>
#include <util.hpp>
#include <camManager.hpp>
#include <inputManager.hpp>
#include <memManager.hpp>
#include <uniforms.hpp>

#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

static void imguiCheck(VkResult result) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Imgui checker fail!");
    }
}

ImguiProxy::ImguiProxy(const AttachementsFormats& imageFormats, const std::vector<VkImageView>& swapChainImageViews,
     const PhysicalDeviceInstance& physicalDeviceInstance, GLFWwindow* window, const VkQueue graphicsQueue, const QueueFamilyIndices& familyIndices,
     VkExtent2D& swapChainExtent, MemoryManager& memMan)
     : deviceHandle(physicalDeviceInstance.device), swapChainExtentHandle(swapChainExtent), memManager(memMan) {
    createDescriptorPool();
    createRenderPass(imageFormats);
    createFramebuffers(swapChainImageViews);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;

    io.ConfigWindowsResizeFromEdges = true;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_PipelineInfo pipelineInfo{
        .RenderPass = renderPass,
        .Subpass = 0,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
    };
    
    ImGui_ImplVulkan_InitInfo init_info = {
        .Instance = physicalDeviceInstance.instance,
        .PhysicalDevice = physicalDeviceInstance.physicalDevice,
        .Device = physicalDeviceInstance.device,
        .QueueFamily = familyIndices.graphicsFamily.value(),
        .Queue = graphicsQueue,
        .DescriptorPool = descriptorPool,
        .MinImageCount = static_cast<uint32_t>(swapChainImageViews.size()),
        .ImageCount = static_cast<uint32_t>(swapChainImageViews.size()),
        .PipelineInfoMain = pipelineInfo,
        .CheckVkResultFn = imguiCheck,
    };

    ImGui_ImplVulkan_Init(&init_info);

    createCommandBuffers(familyIndices);
}

ImguiProxy::~ImguiProxy() {
    for (auto framebuffer : framebuffers) {
        vkDestroyFramebuffer(deviceHandle, framebuffer, nullptr);
    }
    vkDestroyRenderPass(deviceHandle, renderPass, nullptr);
    vkDestroyCommandPool(deviceHandle, commandPool, nullptr);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(deviceHandle, descriptorPool, nullptr);
}

void ImguiProxy::rebuildUI(float fps, CamerasManager& camManager, InputManager* inputManager) {
    // Tell ImGui to start a new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // ImGui::ShowDemoWindow();
    drawUI(fps, camManager, inputManager);

    ImGui::Render();
}

void ImguiProxy::uiActiveCam(CamerasManager& camManager) {
    if (ImGui::CollapsingHeader("Active camera")) {
        float& yaw = camManager.activeCam->getYawRef();

        ImGui::Text("Current cam: "); ImGui::SameLine(0, 7);
        if (camManager.novelViewToggeled()){
            ImGui::TextColored(ImVec4(1, 0.85f, 0.0f, 1.0f), "Novel View");
        }
        else if (camManager.observerToggeled()) {
            ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "Observer");
        }
        else {
            ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.0f, 1.0f), "Cam array %d", camManager.getActiveIndex());
        }
        
        ImGui::SeparatorText("Cam Orientation");
        ImGui::DragFloat3("position##Active_cam", glm::value_ptr(camManager.activeCam->getPositionRef()), 0.1f, -100.0f, 100.0f, "%0.1f");

        if (ImGui::DragFloat("yaw##Active_cam", &yaw, 1.0f, -181.0f, 181.0f, "%0.1f")) {
            if (yaw >= 180.0f) yaw -= 360.0f;
            if (yaw <= -180.0f) yaw += 360.0f;

            camManager.activeCam->recalculateVectors();
        }
        if (ImGui::SliderFloat("pitch##Active_cam", &camManager.activeCam->getPitchRef(), -89.0f, 89.0f, "%.1f")) {
            camManager.activeCam->recalculateVectors();
        }

        ImGui::SeparatorText("Cam Speed");
        ImGui::SliderFloat("speed", &camManager.activeCam->getSpeedRef(), CAM_MIN_SPEED, CAM_MAX_SPEED, "%.1f");
        ImGui::DragFloat("speed step", &camManager.activeCam->getSpeedStepRef(), 0.05f , 0.0f, 10.0f, "%0.1f");
    }
}

void ImguiProxy::uiNovelCam(CamerasManager& camManager, InputManager* inputManager) {
    if (ImGui::CollapsingHeader("Novel camera")) {
        bool novelToggle = camManager.novelViewToggeled();

        if (ImGui::Checkbox("Novel View toggeled", &novelToggle)) {
            camManager.toggleNovel();
        }

        ImGui::SeparatorText("Cam Orientation");
        ImGui::BeginDisabled();     // read only

        ImGui::DragFloat3("position##Novel_view", glm::value_ptr(camManager.novelView.getPositionRef()), 0.1f, -100.0f, 100.0f, "%0.1f");
        ImGui::DragFloat("yaw##Novel_view", &camManager.novelView.getYawRef(), 1.0f, -180.0f, 180.0f, "%0.1f");
        ImGui::SliderFloat("pitch##Novel_view", &camManager.novelView.getPitchRef(), -180.0f, 180.0f);

        ImGui::EndDisabled();
    }
}

void ImguiProxy::uiObserver(CamerasManager& camManager) {
    if (ImGui::CollapsingHeader("Observer")) {
        bool observerToggle = camManager.observerToggeled();

        if (ImGui::Checkbox("Observer toggeled", &observerToggle)) {
            camManager.toggleObserver();
        }

        ImGui::SeparatorText("Cam Orientation");
        ImGui::BeginDisabled();     // read only

        ImGui::DragFloat3("position##Observer", glm::value_ptr(camManager.observer.getPositionRef()), 0.1f, -100.0f, 100.0f, "%0.1f");
        ImGui::DragFloat("yaw##Observer", &camManager.observer.getYawRef(), 1.0f, -180.0f, 180.0f, "%0.1f");
        ImGui::SliderFloat("pitch##Observer", &camManager.observer.getPitchRef(), -180.0f, 180.0f);

        ImGui::EndDisabled();
    }
}

void ImguiProxy::uiCamArray(CamerasManager& camManager, InputManager* inputManager) {
    int tmpIndex = camManager.getActiveIndex();    

    if (ImGui::CollapsingHeader("Reference cam array")) {
        const uint32_t index_small_step = 1;
        const uint32_t index_big_step = 10;
        uint32_t activeIndex = camManager.getActiveIndex();

        ImGui::Text("Cam count: %d", camManager.getCamCount());

        ImGui::BeginDisabled(inputManager->presentOfflineFlag);
        // Add a new camera button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.412f, 0.102f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.34f, 0.500f, 0.123f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.323f, 0.080f, 1.0f));

        if (ImGui::Button("Add cam")) {
            camManager.addCam(memManager);
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        // Delete a camera button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.483f, 0.0084f, 0.0245f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.587f, 0.010f, 0.030f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.380f, 0.007f, 0.019f, 1.0f));

        if (ImGui::Button("Delete cam")) {
            camManager.deleteCam(memManager);
        }
        ImGui::PopStyleColor(3);
        ImGui::EndDisabled();

        // Switch to a different cam in the array input
        ImGui::BeginDisabled(camManager.novelViewToggeled());
        if (ImGui::InputScalar("current index", ImGuiDataType_S32, &tmpIndex, &index_small_step, &index_big_step)) {
            if (tmpIndex < 0) {
                activeIndex = camManager.getCamCount() - 1;
            }
            else if (tmpIndex >= camManager.getCamCount()) {
                activeIndex = tmpIndex % camManager.getCamCount();
            }
            else {
                activeIndex = tmpIndex;
            }
            camManager.setActiveCam(activeIndex);
        }

        ImGui::EndDisabled();
    }
}

void ImguiProxy::uiOfflineRender(CamerasManager& camManager, InputManager* inputManager) {
    if (ImGui::CollapsingHeader("Offline render")) {
        static int depthFormat = 0;
        static int presentFormat = 0;

        // Take offline render snapshots button
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.522f, 0.031f, 0.306f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.650f, 0.070f, 0.380f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.420f, 0.020f, 0.240f, 1.0f));

        ImGui::BeginDisabled(inputManager->presentOfflineFlag);
        if (ImGui::Button("Take snapshot")) {
            inputManager->renderOfflineFlag = true;
        }
        ImGui::EndDisabled();

        if (camManager.offlineImagesRendered) {
            ImGui::SameLine(0, 30.0f);
            ImGui::Text("Snapshots taken!");
        }
        ImGui::PopStyleColor(3);

        ImGui::SeparatorText("Saving snapshots");
        ImGui::InputText("filename", &snapshotsFiles);

        ImGui::RadioButton("depth format HDR", &depthFormat, 0); ImGui::SameLine();
        ImGui::RadioButton("depth format EXR", &depthFormat, 1);

        // Save the offline render snapshots to a file button
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.380f, 0.020f, 0.430f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.480f, 0.045f, 0.540f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.300f, 0.015f, 0.340f, 1.0f));

        ImGui::BeginDisabled(!camManager.offlineImagesRendered);
        ImGui::BeginDisabled(inputManager->presentOfflineFlag);
        if (ImGui::Button("Save snapshots")) {
            if (depthFormat == 0) {
                camManager.saveOfflineImages(memManager, snapshotsFiles, SaveImageFormat::HDR);
            }
            else {
                camManager.saveOfflineImages(memManager, snapshotsFiles, SaveImageFormat::EXR);
            }
        }
        ImGui::EndDisabled();

        ImGui::SeparatorText("Viewing snapshots");
        if (ImGui::RadioButton("color image", &presentFormat, 0)) {
            inputManager->presentType = ImageViewType::COLOR;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("depth image", &presentFormat, 1)) {
            inputManager->presentType = ImageViewType::DEPTH;
        }

        // todo novel view special case
        if (ImGui::Checkbox("Present shapshots", &inputManager->presentOfflineFlag) && inputManager->presentOfflineFlag) {
            inputManager->setupOfflineImage = true;   // update the descriptor set
        }
        
        ImGui::EndDisabled();
        ImGui::PopStyleColor(3);
    }
}

void ImguiProxy::uiNovelRender(CamerasManager& camManager, InputManager* inputManager) {
    static int novelDebug = 0;

    if (ImGui::CollapsingHeader("Novel render")) {
        const uint32_t minSample = 1;
        const uint32_t maxSample = 128;

        ImGui::BeginDisabled(!camManager.offlineImagesRendered);
        if (ImGui::Checkbox("Render novel view", &inputManager->novelRender) && inputManager->novelRender) {
            inputManager->startSynthesis = true;
        }
        ImGui::EndDisabled();
        ImGui::SliderScalar("Sample count", ImGuiDataType_U32, &camManager.sampleCount, &minSample, &maxSample);

        if (ImGui::RadioButton("No debug", &novelDebug, 0)) {
        inputManager->novelDebug = DebugCompute::NO_DEBUG;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Cam count", &novelDebug, 2)) {
            inputManager->novelDebug = DebugCompute::CAM_COUNT;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Cam ID", &novelDebug, 3)) {
            inputManager->novelDebug = DebugCompute::CAM_ID;
        }
    }

    // todo later to input images from blender
}

void ImguiProxy::uiDebugInfo(float fps, InputManager* inputManager) {
    if (ImGui::CollapsingHeader("Debug info")) {
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Checkbox("Grayscale", &inputManager->debugGrayscale);        // TODO
        ImGui::Checkbox("Show Cam-cubes", &inputManager->debugCamCube);
        ImGui::Checkbox("Show frustum", &inputManager->debugFrustum);
        ImGui::Checkbox("Show intersections", &inputManager->debugIntersection);
        if (ImGui::Checkbox("Point cloud", &inputManager->debugPointCloud) && inputManager->debugPointCloud) {
            inputManager->startSynthesis = true;
        }
    }
}

void ImguiProxy::drawUI(float fps, CamerasManager& camManager, InputManager* inputManager) {
    // Draw the UI
    ImGui::Begin("Info");
    uiActiveCam(camManager);

    uiNovelCam(camManager, inputManager);
    uiObserver(camManager);
    uiCamArray(camManager, inputManager);
    
    uiOfflineRender(camManager, inputManager);
    uiNovelRender(camManager, inputManager);
    uiDebugInfo(fps, inputManager);
    
    ImGui::End();
}

void ImguiProxy::recreateFramebuffers(const std::vector<VkImageView>& swapChainImageViews, const VkExtent2D& swapChainExtent) {
    for (auto framebuffer : framebuffers) {
        vkDestroyFramebuffer(deviceHandle, framebuffer, nullptr);
    }

    swapChainExtentHandle = swapChainExtent;
    createFramebuffers(swapChainImageViews);
}

VkCommandBuffer ImguiProxy::recordCommandBuffer(uint32_t currentFrame, uint32_t imageIndex) {
    VkCommandBufferBeginInfo commandBufferBI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    if (vkBeginCommandBuffer(commandBuffers[currentFrame], &commandBufferBI) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer at imgui!");
    }

    // Begin ImGui render pass
    VkRenderPassBeginInfo renderPassBI{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffers[imageIndex],
        .renderArea = {{0,0}, swapChainExtentHandle},  // offset, extent
        .clearValueCount = 0,   // Usually load existing color
    };
    vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);

    // Render the ImGui draw data
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffers[currentFrame]);

    // End render pass
    vkCmdEndRenderPass(commandBuffers[currentFrame]);
    if (vkEndCommandBuffer(commandBuffers[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }

    return commandBuffers[currentFrame];
}

// Basic imgui setup for Vulakn taken from https://vkguide.dev/docs/extra-chapter/implementing_imgui/

void ImguiProxy::createDescriptorPool() {
    VkDescriptorPoolSize poolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

    VkDescriptorPoolCreateInfo descriptorPoolCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = std::size(poolSizes),
        .pPoolSizes = poolSizes,
    };

    
    if (vkCreateDescriptorPool(deviceHandle, &descriptorPoolCI, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

void ImguiProxy::createRenderPass(const AttachementsFormats& imageFormats) {
    VkAttachmentDescription colorAttachment{
        .format = imageFormats.colorImageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,   // keep postprocessed image
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pDepthStencilAttachment = nullptr
    };
    VkSubpassDependency dependency {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo renderPassCI{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    if (vkCreateRenderPass(deviceHandle, &renderPassCI, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }
}

void ImguiProxy::createFramebuffers(const std::vector<VkImageView>& swapChainImageViews) {
    framebuffers.resize(swapChainImageViews.size());
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        VkImageView attachements = {
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferCI{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderPass,
            .attachmentCount = 1,
            .pAttachments = &attachements,
            .width = swapChainExtentHandle.width,
            .height = swapChainExtentHandle.height,
            .layers = 1
        };

        if (vkCreateFramebuffer(deviceHandle, &framebufferCI, nullptr, &framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create frambutters!");
        }
    }
}

void ImguiProxy::createCommandBuffers(const QueueFamilyIndices& familyIndices) {
    VkCommandPoolCreateInfo graphicsCommandPoolCI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = familyIndices.graphicsFamily.value()
    };

    if (vkCreateCommandPool(deviceHandle, &graphicsCommandPoolCI, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics command pool!");
    }

    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo commandBufferAI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())
    };

    if (vkAllocateCommandBuffers(deviceHandle, &commandBufferAI, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}