#include <imguiProxy.hpp>
#include <swapchain.hpp>
#include <vulkanContext.hpp>
#include <util.hpp>
#include <camManager.hpp>
#include <inputManager.hpp>
#include <memManager.hpp>
#include <uniforms.hpp>
#include <mesh.hpp>
#include <swapchain.hpp>

#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

static void getSetup(std::vector<std::string>& filenames) {
    for (const auto& entry : std::filesystem::directory_iterator(JSON_DIR)) {
        if (entry.is_regular_file()) {       // skip directories
            filenames.push_back(entry.path().stem().string());
        }
    }
}

static void deleteSetup(const std::string& setupName) {
    std::error_code ec;                  // avoid exceptions
    bool removed = std::filesystem::remove(JSON_DIR + setupName + ".json", ec);

    if (!removed) {
        std::cerr << "Failed to delete " << JSON_DIR + setupName + ".json";
        if (ec) std::cerr << ": " << ec.message();
        std::cerr << "\n";
    }
}

static void imguiCheck(VkResult result) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Imgui checker fail!");
    }
}

ImguiProxy::ImguiProxy(const AttachementsFormats& imageFormats, const std::vector<VkImageView>& swapChainImageViews,
     const PhysicalDeviceInstance& physicalDeviceInstance, GLFWwindow* window, const VkQueue graphicsQueue, const QueueFamilyIndices& familyIndices,
     VkExtent2D& swapChainExtent, MemoryManager& memMan)
     : deviceHandle(physicalDeviceInstance.device), swapChainExtentHandle(swapChainExtent), memManager(memMan) {
    getSetup(setupNames);

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

void ImguiProxy::rebuildUI(float fps, CamerasManager& camManager, InputManager* inputManager, Mesh& mesh, const int scrWidth, const int scrHeight) {
    // Tell ImGui to start a new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // ImGui::ShowDemoWindow();
    drawUI(fps, camManager, inputManager, mesh, scrWidth, scrHeight);

    ImGui::Render();
}

void ImguiProxy::uiSetup(InputManager* inputManager) {
    static int listboxSelected = -1;
    static bool buttonTriggered = false;

    if (ImGui::CollapsingHeader("Setup")) {
        ImGui::Indent();

        ImGui::InputText("setup name", &inputManager->setupName);
        bool setupExists = std::find(setupNames.begin(), setupNames.end(), inputManager->setupName) != setupNames.end();

        if (ImGui::Button("Save current setup")) {
            if (setupExists) {
                buttonTriggered = true;
            } else {
                inputManager->saveSetupFlag = true;
                setupNames.push_back(inputManager->setupName);

                buttonTriggered = false;
            }
        }

        if (buttonTriggered) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.851, 0.008, 0.008, 1.0f), "Setup already exists!");// already exists, do not add
        }

        ImGui::PopStyleColor(3);
        ImGui::Spacing();

        ImGui::ListBox("Saved setups", &listboxSelected,
                        [](void* data, int idx, const char** out_text) {
                            auto& vec = *static_cast<std::vector<std::string>*>(data);
                            *out_text = vec[idx].c_str();
                            return true;
                        },
                        static_cast<void*>(&setupNames), static_cast<int>(setupNames.size()), 2);


        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0.337, 0.91, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0, 0.470, 0.982, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0, 0.270, 0.728, 1.00f));

        if (ImGui::Button("Load setup") && listboxSelected >= 0) {
            // load the selected setup
            inputManager->loadSetupFlag = true;
            inputManager->setupNameLoad = setupNames[listboxSelected];
        }

        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.483f, 0.0084f, 0.0245f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.587f, 0.010f, 0.030f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.380f, 0.007f, 0.019f, 1.0f));

        if (ImGui::Button("Delete config") && listboxSelected >= 0) {
            deleteSetup(setupNames[listboxSelected]);

            setupNames.erase(setupNames.begin() + listboxSelected);
            listboxSelected = -1; // reset selection
        }

        ImGui::Unindent();
    }
    ImGui::PopStyleColor(3);
}

void ImguiProxy::uiGeneral(InputManager* inputManager, Mesh& mesh, const int scrWidth, const int scrHeight) {
    const char* sceneFiles[] = { "porsche", "city", "vikingRoom"};

    if (ImGui::CollapsingHeader("General")) {
        ImGui::Indent();

        ImGui::SeparatorText("Scene");
        if (ImGui::Combo("selected", &inputManager->sceneSelected, sceneFiles, IM_ARRAYSIZE(sceneFiles))) {
            inputManager->sceneChanged = true;
        }
        ImGui::SliderFloat("scale", &mesh.scale, 0, 20, "%.1f");


        ImGui::SeparatorText("Light");
        ImGui::DragFloat3("position##Light", glm::value_ptr(mesh.getLightRef()), 0.1f, -100.0f, 100.0f, "%0.1f");


        ImGui::SeparatorText("Resolution");
        ImGui::Text("Current resolution:"); ImGui::SameLine(0, 7);
        ImGui::TextColored(ImVec4(1, 0.85f, 0.0f, 1.0f), "%dx%d", scrWidth, scrHeight);
        if (ImGui::InputInt2("", inputManager->screenRes)) {
            if (inputManager->screenRes[0] < 50) {
                inputManager->screenRes[0] = 50;
            }
            if (inputManager->screenRes[1] < 50) {
                inputManager->screenRes[1] = 50;
            }
            ; // nothing while editing
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.639f, 0.376f, 0.047f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.751f, 0.476f, 0.136f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.511f, 0.301f, 0.038f, 1.00f));

        if (ImGui::Button("Apply##Resolution")) {
            inputManager->changeRes = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SeparatorText("Snapshots");
        ImGui::InputText("filename###general", &inputManager->snapshotCamFile);

        // Save the offline render snapshots to a file button
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.380f, 0.020f, 0.430f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.480f, 0.045f, 0.540f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.300f, 0.015f, 0.340f, 1.0f));

        if (ImGui::Button("Take a snapshot")) {
            inputManager->saveCamSnapshots = true;
        }

        uiSetup(inputManager);
        ImGui::Unindent();
    }
}

void ImguiProxy::uiCamera(CamerasManager& camManager, InputManager* inputManager) {
    if (ImGui::CollapsingHeader("Cameras")) {
        float& yaw = camManager.activeCam->getYawRef();

        ImGui::Indent();

        ImGui::Text("Current cam: "); ImGui::SameLine(0, 7);
        if (camManager.novelViewToggled()){
            ImGui::TextColored(ImVec4(1, 0.85f, 0.0f, 1.0f), "Novel View");
        }
        else if (camManager.observerToggled()) {
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

        uiNovelCam(camManager, inputManager);
        uiCamArray(camManager, inputManager);

        ImGui::Unindent();
    }
}

void ImguiProxy::uiNovelCam(CamerasManager& camManager, InputManager* inputManager) {
    if (ImGui::CollapsingHeader("Novel camera")) {
        bool novelToggle = camManager.novelViewToggled();

        ImGui::Indent();

        if (ImGui::Checkbox("Novel view toggeled", &novelToggle)) {
            camManager.toggleNovel();
        }

        ImGui::SeparatorText("Cam Orientation");
        ImGui::BeginDisabled();     // read only

        ImGui::DragFloat3("position##Novel_view", glm::value_ptr(camManager.novelView.getPositionRef()), 0.1f, -100.0f, 100.0f, "%0.1f");
        ImGui::DragFloat("yaw##Novel_view", &camManager.novelView.getYawRef(), 1.0f, -180.0f, 180.0f, "%0.1f");
        ImGui::SliderFloat("pitch##Novel_view", &camManager.novelView.getPitchRef(), -180.0f, 180.0f);

        ImGui::EndDisabled();

        ImGui::SeparatorText("Observer cam");
        bool observerToggle = camManager.observerToggled();

        if (ImGui::Checkbox("Observer toggeled", &observerToggle)) {
            camManager.toggleObserver();
        }

        ImGui::Unindent();
    }
}

void ImguiProxy::uiCamArray(CamerasManager& camManager, InputManager* inputManager) {
    int tmpIndex = camManager.getActiveIndex();    

    if (ImGui::CollapsingHeader("Reference cam array")) {
        const uint32_t index_small_step = 1;
        const uint32_t index_big_step = 10;
        uint32_t activeIndex = camManager.getActiveIndex();

        ImGui::Indent();

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
        ImGui::BeginDisabled(camManager.novelViewToggled());
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

        ImGui::Unindent();
    }
}

void ImguiProxy::uiOfflineRender(CamerasManager& camManager, InputManager* inputManager) {
    if (ImGui::CollapsingHeader("Offline render")) {
        static int depthFormat = 0;
        static int presentFormat = 0;

        ImGui::Indent();

        // Take offline render snapshots button
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.522f, 0.031f, 0.306f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.650f, 0.070f, 0.380f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.420f, 0.020f, 0.240f, 1.0f));

        ImGui::BeginDisabled(inputManager->presentOfflineFlag);
        if (ImGui::Button("Take snapshot")) {
            inputManager->renderOfflineFlag = true;
        }
        ImGui::EndDisabled();

        if (camManager.imagesRendered()) {
            ImGui::SameLine(0, 30.0f);
            ImGui::Text("Snapshots taken!");
        }
        ImGui::PopStyleColor(3);

        ImGui::SeparatorText("Saving snapshots");
        ImGui::InputText("filename", &offlineSnapshotsFiles);

        ImGui::RadioButton("depth format HDR", &depthFormat, 0); ImGui::SameLine();
        ImGui::RadioButton("depth format EXR", &depthFormat, 1);

        // Save the offline render snapshots to a file button
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.380f, 0.020f, 0.430f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.480f, 0.045f, 0.540f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.300f, 0.015f, 0.340f, 1.0f));

        ImGui::BeginDisabled(!camManager.imagesRendered());
        ImGui::BeginDisabled(inputManager->presentOfflineFlag);
        if (ImGui::Button("Save snapshots")) {
            if (depthFormat == 0) {
                camManager.saveImages(offlineSnapshotsFiles, SaveImageFormat::HDR);
            }
            else {
                camManager.saveImages(offlineSnapshotsFiles, SaveImageFormat::EXR);
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
        if (ImGui::Checkbox("Present snapshots", &inputManager->presentOfflineFlag) && inputManager->presentOfflineFlag) {
            inputManager->setupOfflineImage = true;   // update the descriptor set
        }
        
        ImGui::EndDisabled();
        ImGui::PopStyleColor(3);

        ImGui::Unindent();
    }
}

void ImguiProxy::uiNovelRender(CamerasManager& camManager, InputManager* inputManager) {
    static int novelDebug = 0;
    static int novelHeuristic = 0;
    static int distanceType = 0;
    static int coneType = 0;

    if (ImGui::CollapsingHeader("Novel render")) {
        const uint32_t minDebugSample = 0;
        const uint32_t minSample = 1;
        const uint32_t maxSample = 128;
        const uint32_t maxNeighbour = 100;

        ImGui::Indent();

        ImGui::BeginDisabled(!camManager.imagesRendered());
        if (ImGui::Checkbox("Render novel view", &inputManager->novelRender) && inputManager->novelRender) {
            inputManager->startSynthesis = true;
        }

        if (ImGui::Checkbox("Novel analytical synthesis", &inputManager->newNovelRender) && inputManager->newNovelRender) {
            inputManager->startSynthesis = true;
        }
        ImGui::EndDisabled();

        ImGui::SeparatorText("Heuristic");
        if (ImGui::RadioButton("color", &novelHeuristic, 0)) {
            inputManager->novelHeuristic = NovelHeuristic::COLOR_HEURISTIC;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("depth", &novelHeuristic, 1)) {
            inputManager->novelHeuristic = NovelHeuristic::DEPTH_HEURISTIC;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("angle", &novelHeuristic, 2)) {
            inputManager->novelHeuristic = NovelHeuristic::ANGLE_HEURISTIC;
        }

        uint32_t oldSampleCount = camManager.sampleCount;
        if (ImGui::SliderScalar("Sample count", ImGuiDataType_U32, &camManager.sampleCount, &minSample, &maxSample)) {
            if (camManager.sampleDebug == oldSampleCount) {
                camManager.sampleDebug = camManager.sampleCount;
            }
        }
        ImGui::SliderScalar("Curr sample", ImGuiDataType_U32, &camManager.sampleDebug, &minDebugSample, &camManager.sampleCount);

        if (ImGui::CollapsingHeader("Depth settings")) {
            ImGui::Indent(); // add left spacing

            ImGui::SeparatorText("Distance");
            if (ImGui::RadioButton("point - point", &distanceType, 0)) {
                inputManager->distance = DistanceType::POINT;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("point - ray", &distanceType, 1)) {
                inputManager->distance = DistanceType::POINT_RAY;
            }

            ImGui::SeparatorText("Cone Marching");
            if (ImGui::RadioButton("No cone", &coneType, 0)) {
                inputManager->coneMarching = ConeMarching::NO_CONE;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Only in cone", &coneType, 1)) {
                inputManager->coneMarching = ConeMarching::ONLY_CONE;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Conditional in cone", &coneType, 2)) {
                inputManager->coneMarching = ConeMarching::PARTIAL_CONE;
            }

            ImGui::SliderScalar("Neighbourhood", ImGuiDataType_U32, &inputManager->neighbourCount, &minSample, &maxNeighbour);
            ImGui::SliderFloat("In cone percentage", &inputManager->inConePercentage, 0.0f, 1.0f, "%0.1f%");

            ImGui::Unindent();
        }

        ImGui::SeparatorText("Debug");
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
        ImGui::SameLine();
        if (ImGui::RadioButton("Sample depth", &novelDebug, 4)) {
            inputManager->novelDebug = DebugCompute::SAMPLE_DEPTH;
        }

        ImGui::Unindent();
    }

    // todo later to input images from blender
}

void ImguiProxy::uiDebugInfo(float fps, InputManager* inputManager, bool offlineRendred) {
    if (ImGui::CollapsingHeader("Debug info")) {
        ImGui::Indent();

        ImGui::Text("FPS: %.1f", fps);
        ImGui::Checkbox("Grayscale", &inputManager->debugGrayscale);
        ImGui::Checkbox("Show Cam-cubes", &inputManager->debugCamCube);
        ImGui::Checkbox("Show frustum", &inputManager->debugFrustum);
        ImGui::Checkbox("Show intersections", &inputManager->debugIntersection);
        

        ImGui::BeginDisabled(!offlineRendred);
        if (ImGui::Checkbox("Point cloud", &inputManager->debugPointCloud) && inputManager->debugPointCloud) {
            inputManager->startSynthesis = true;
        }
        ImGui::EndDisabled();

        ImGui::Unindent();
    }
}

void ImguiProxy::drawUI(float fps, CamerasManager& camManager, InputManager* inputManager, Mesh& mesh, const int scrWidth, const int scrHeight) {
    // Draw the UI
    ImGui::Begin("Info");
    uiGeneral(inputManager, mesh, scrWidth, scrHeight);

    uiCamera(camManager, inputManager);
    
    uiOfflineRender(camManager, inputManager);
    uiNovelRender(camManager, inputManager);
    uiDebugInfo(fps, inputManager,camManager.imagesRendered());
    
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
            throw std::runtime_error("Failed to create framebuffers!");
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