#include "ui.hpp"
#include "swapchain.hpp"
#include "vulkanContext.hpp"
#include "util.hpp"

#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

static void imguiCheck(VkResult result) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Imgui checker fail!");
    }
}

ImguiProxy::ImguiProxy(const AttachementsFormats& imageFormats, const std::vector<VkImageView>& swapChainImageViews,
     const PhysicalDeviceInstance& physicalDeviceInstance, GLFWwindow* window, const VkQueue& graphicsQueue, const QueueFamilyIndices& familyIndices,
     VkExtent2D& swapChainExtent)
     : deviceHandle(physicalDeviceInstance.device), swapChainExtentHandle(swapChainExtent) {
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

void ImguiProxy::rebuildUI(float fps, uint32_t camNum) {
    // Tell ImGui to start a new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // ImGui::ShowDemoWindow();
    drawUI(fps, camNum);

    ImGui::Render();
}

void ImguiProxy::drawUI(float fps, uint32_t camNum) {
    // Draw the UI
    ImGui::Begin("Project Info");
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Current cam: %d", camNum);
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

// todo refresh swapchain after resize, and redo framebuffers
// check cureentFrame and image Index
// problem with the final layout, change based whether using imgui or not?

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