#include <pipeline.hpp>
#include <swapchain.hpp>
#include <vertex.hpp>

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary); // start reading at the end

    if (!file.is_open()){
        throw std::runtime_error("Failed to open file!");
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

static VkShaderModule createShaderModule(VkDevice deviceHandle, const std::string& fileName) {
    auto code = readFile(fileName);

    VkShaderModuleCreateInfo shaderModuleCI{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t*>(code.data()),
    };

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(deviceHandle, &shaderModuleCI, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Cannot create shader module!");
    }

    // thin wrappers around shader code, are compiled and linked when creating graphics pipeline and then can be destroyed
    return shaderModule;
}

GraphicsPipeline::GraphicsPipeline(const VkDevice device, const AttachementsFormats& imageFormats, const VkDescriptorSetLayout descriptorSL,
     const std::string& vertexFile, const std::string& fragFile)
    : deviceHandle(device), vertexShader(vertexFile), fragmentShader(fragFile) {
    renderPass = createRenderPass(imageFormats);

    std::vector<VkDescriptorSetLayout>descriptorVec{descriptorSL};
    createPipelineLayout(descriptorVec, pipelineLayout);

    DepthStencilFlags depthFlags{
        .testEnable = VK_TRUE,
        .writeEnable = VK_TRUE
    };
    RasterizationFlags rastFlags{
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
    };
    graphicsPipeline = createGraphicsPipeline(vertexFile, fragFile, pipelineLayout, renderPass, VertexInputFlags::POS_COL_UV, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    depthFlags, rastFlags);
}

GraphicsPipeline::~GraphicsPipeline() {
    vkDestroyPipeline(deviceHandle, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(deviceHandle, pipelineLayout, nullptr);

    if (frustumPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(deviceHandle, frustumPipeline, nullptr);
        vkDestroyPipelineLayout(deviceHandle, frustumPipelineLayout, nullptr);
    }

    if (intersectionPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(deviceHandle, intersectionPipeline, nullptr);
        vkDestroyPipelineLayout(deviceHandle, intersectionPipelineLayout, nullptr);
    }

    if (camCubePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(deviceHandle, camCubePipeline, nullptr);
        vkDestroyPipelineLayout(deviceHandle, camCubePipelineLayout, nullptr);
    }

    if (offlineRenderPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(deviceHandle, offlineRenderPipeline, nullptr);
        vkDestroyPipelineLayout(deviceHandle, offlineRenderPipelineLayout, nullptr);
    }

    vkDestroyRenderPass(deviceHandle, renderPass, nullptr);
    if (onTopRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(deviceHandle, onTopRenderPass, nullptr);
    }
}

void GraphicsPipeline::setupFrustumPipeline(const VkDescriptorSetLayout descriptorSL, const AttachementsFormats& imageFormats, const std::string& vertexFile, const std::string& fragFile) {
    onTopRenderPass = createOnTopRenderPass(imageFormats);

    std::vector<VkDescriptorSetLayout>descriptorVec{descriptorSL};
    createPipelineLayout(descriptorVec, frustumPipelineLayout);

    DepthStencilFlags depthFlags{
        .testEnable = VK_TRUE,
        .writeEnable = VK_FALSE
    };
    RasterizationFlags rastFlags{
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
    };
    frustumPipeline = createGraphicsPipeline(vertexFile, fragFile, frustumPipelineLayout, onTopRenderPass, VertexInputFlags::POS_COL_UV, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    depthFlags, rastFlags);
}

void GraphicsPipeline::setupIntersectionPipeline(const VkDescriptorSetLayout descriptorSL, const AttachementsFormats& imageFormats, const std::string& vertexFile, const std::string& fragFile) {
    if (onTopRenderPass == VK_NULL_HANDLE) {
        onTopRenderPass = createOnTopRenderPass(imageFormats);
    }

    std::vector<VkDescriptorSetLayout>descriptorVec{descriptorSL};
    createPipelineLayout(descriptorVec, intersectionPipelineLayout);

    DepthStencilFlags depthFlags{
        .testEnable = VK_TRUE,
        .writeEnable = VK_FALSE
    };
    RasterizationFlags rastFlags{
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
    };
    intersectionPipeline = createGraphicsPipeline(vertexFile, fragFile, intersectionPipelineLayout, onTopRenderPass, VertexInputFlags::POS, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
         depthFlags, rastFlags);
}

void GraphicsPipeline::setupCamCubePipeline(const VkDescriptorSetLayout descriptorSL, const AttachementsFormats& imageFormats, const std::string& vertexFile, const std::string& fragFile) {
    if (onTopRenderPass == VK_NULL_HANDLE) {
        onTopRenderPass = createOnTopRenderPass(imageFormats);
    }

    std::vector<VkDescriptorSetLayout>descriptorVec{descriptorSL};
    createPipelineLayout(descriptorVec, camCubePipelineLayout, 1);
    
    DepthStencilFlags depthFlags{
        .testEnable = VK_TRUE,
        .writeEnable = VK_FALSE
    };
    RasterizationFlags rastFlags{
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
    };

    camCubePipeline = createGraphicsPipeline(vertexFile, fragFile, camCubePipelineLayout, onTopRenderPass, VertexInputFlags::NONE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
         depthFlags, rastFlags);
}

void GraphicsPipeline::setupOfflinePipeline(const VkDescriptorSetLayout descriptorSL, const std::string& vertexFile, const std::string& fragFile) {
    std::vector<VkDescriptorSetLayout>descriptorVec{descriptorSL};
    createPipelineLayout(descriptorVec, offlineRenderPipelineLayout);
    
    DepthStencilFlags depthFlags{
        .testEnable = VK_FALSE,
        .writeEnable = VK_FALSE
    };
    RasterizationFlags rastFlags{
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
    };

    offlineRenderPipeline = createGraphicsPipeline(vertexFile, fragFile, offlineRenderPipelineLayout, renderPass, VertexInputFlags::NONE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
         depthFlags, rastFlags);
}

void GraphicsPipeline::createPipelineLayout(std::vector<VkDescriptorSetLayout>& descriptorSL, VkPipelineLayout& pipelineLayout, uint32_t pushConstant) {
    VkPushConstantRange pushConstantRange{};

    VkPipelineLayoutCreateInfo pipelineLayoutCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(descriptorSL.size()),
        .pSetLayouts = descriptorSL.data(),
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };

    if (pushConstant == 1) {
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(glm::mat4) * 2; // view + proj

        pipelineLayoutCI.pushConstantRangeCount = 1;
        pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
    }

    if (vkCreatePipelineLayout(deviceHandle, &pipelineLayoutCI, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipelineLayout!");
    };
}

static VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI(VkPrimitiveTopology topology) {
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = topology,
        .primitiveRestartEnable = VK_FALSE
    };

    return inputAssemblyStateCI;
}

static VkPipelineDepthStencilStateCreateInfo depthStencilCI(DepthStencilFlags depthFlags) {
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = depthFlags.testEnable,
        .depthWriteEnable = depthFlags.writeEnable,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    return depthStencilStateCI;
}

static VkPipelineRasterizationStateCreateInfo rasterizetionCI(RasterizationFlags rastFlags) {
    VkPipelineRasterizationStateCreateInfo rasterizationStateCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = rastFlags.cullMode,
        .frontFace = rastFlags.frontFace,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f
    };

    return rasterizationStateCI;
}

VkPipeline GraphicsPipeline::createGraphicsPipeline(const std::string& vertexShader, const std::string& fragmentShader, VkPipelineLayout& pipelineLayout,
     VkRenderPass& renderPass, VertexInputFlags vertexInput, VkPrimitiveTopology topology, DepthStencilFlags depthFlags, RasterizationFlags rastFlags) {
    VkShaderModule vertShaderModule = createShaderModule(deviceHandle, vertexShader);
    VkShaderModule fragShaderModule = createShaderModule(deviceHandle, fragmentShader);

    VkPipelineShaderStageCreateInfo pipelineShaderStageVertCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertShaderModule,
        .pName = "main"
    };
    VkPipelineShaderStageCreateInfo pipelineShaderStageFragCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragShaderModule,
        .pName = "main"
    };
    VkPipelineShaderStageCreateInfo pipelineShaderStageCI[] = {pipelineShaderStageVertCI, pipelineShaderStageFragCI};

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo pipelineDynamicStateCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    VkVertexInputBindingDescription bindingDescription{};
    std::vector<VkVertexInputAttributeDescription> attributeDescription;

    if (vertexInput == VertexInputFlags::POS) {
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(glm::vec4);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributeDescription.push_back({
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0
        });
    }
    else if (vertexInput == VertexInputFlags::POS_COL_UV) {
        bindingDescription = Vertex::getBindingDescription();
        attributeDescription = Vertex::getAttribureDescriptions();
    }
    else if (vertexInput == VertexInputFlags::NONE) {
        ;
    }
    else {
        throw std::runtime_error("Unknown vertex input!");
    }

   VkPipelineVertexInputStateCreateInfo vertexInputStateCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr
    };

    if (vertexInput != VertexInputFlags::NONE) {
        vertexInputStateCI.vertexBindingDescriptionCount = 1;
        vertexInputStateCI.pVertexBindingDescriptions = &bindingDescription;
        vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
        vertexInputStateCI.pVertexAttributeDescriptions = attributeDescription.data();
    }

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = inputAssemblyCI(topology);

    // we set those as a dynamic state
    // VkViewport viewport{
    //     .x = 0.0f,
    //     .y = 0.0f,
    //     .width = (float) swapChainExtent.width,
    //     .height = (float) swapChainExtent.height,
    //     .minDepth = 0.0f,
    //     .maxDepth = 1.0f
    // };

    // VkRect2D scissor{
    //     .offset = {0, 0},
    //     .extent = swapChainExtent
    // };

    VkPipelineViewportStateCreateInfo viewportStateCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = depthStencilCI(depthFlags);

    VkPipelineRasterizationStateCreateInfo rasterizationStateCI = rasterizetionCI(rastFlags);

    VkPipelineMultisampleStateCreateInfo multisamplingStateCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };

    // depth and stencil testing also required here

    VkPipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
    };

    VkGraphicsPipelineCreateInfo graphicsPipelineCI{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = pipelineShaderStageCI,
        .pVertexInputState = &vertexInputStateCI,
        .pInputAssemblyState = &inputAssemblyStateCI,
        .pViewportState = &viewportStateCI,
        .pRasterizationState = &rasterizationStateCI,
        .pMultisampleState = &multisamplingStateCI,
        .pDepthStencilState = &depthStencilStateCI,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &pipelineDynamicStateCI,

        .layout = pipelineLayout,
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    VkPipeline graphicsPipeline;
    if (vkCreateGraphicsPipelines(deviceHandle, VK_NULL_HANDLE, 1, &graphicsPipelineCI, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create the graphics pipeline!");
    }

    vkDestroyShaderModule(deviceHandle, vertShaderModule, nullptr);
    vkDestroyShaderModule(deviceHandle, fragShaderModule, nullptr);

    return graphicsPipeline;
}

VkRenderPass GraphicsPipeline::createRenderPass(const AttachementsFormats& imageFormats) {
    VkAttachmentDescription colorAttachment{
        .format = imageFormats.colorImageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // we transfer it to present state if ui is not used
    };

    VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentDescription depthAttachment{
        .format = imageFormats.depthFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference depthAttachmentRef{
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pDepthStencilAttachment = &depthAttachmentRef
    };
    VkSubpassDependency dependency {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassCI{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    VkRenderPass renderPass;
    if (vkCreateRenderPass(deviceHandle, &renderPassCI, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }

    return renderPass;
}

VkRenderPass GraphicsPipeline::createOnTopRenderPass(const AttachementsFormats& imageFormats) {
    VkAttachmentDescription colorAttachment{
        .format = imageFormats.colorImageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // we transfer it to present state if ui is not used
    };

    VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentDescription depthAttachment{
        .format = imageFormats.depthFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference depthAttachmentRef{
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pDepthStencilAttachment = &depthAttachmentRef
    };
    VkSubpassDependency dependency {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassCI{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    VkRenderPass renderPass;
    if (vkCreateRenderPass(deviceHandle, &renderPassCI, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }

    return renderPass;
}

ComputePipeline::ComputePipeline(const VkDevice device, const VkDescriptorSetLayout descriptorSL, const VkDescriptorSetLayout sharedDescriptorSL, const std::string& computeFile)
    : deviceHandle(device), computeShader(computeFile) {
        createComputePipeline(descriptorSL, sharedDescriptorSL);
}

ComputePipeline::~ComputePipeline() {
    vkDestroyPipeline(deviceHandle, computePipeline, nullptr);
    vkDestroyPipelineLayout(deviceHandle, pipelineLayout, nullptr);
}

void ComputePipeline::createComputePipeline(const VkDescriptorSetLayout descriptorSL, const VkDescriptorSetLayout sharedDescriptorSL) {
    VkShaderModule compShaderModule = createShaderModule(deviceHandle, computeShader);

    VkPipelineShaderStageCreateInfo shaderStageCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = compShaderModule,
        .pName = "main",
    };

    VkDescriptorSetLayout setLayouts[] = {descriptorSL, sharedDescriptorSL};

    VkPipelineLayoutCreateInfo pipelineLayoutCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 2,
        .pSetLayouts = setLayouts,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };

    if (vkCreatePipelineLayout(deviceHandle, &pipelineLayoutCI, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipelineLayout!");
    };

    VkComputePipelineCreateInfo computePipelineCI {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shaderStageCI,
        .layout = pipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    vkCreateComputePipelines(deviceHandle, VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &computePipeline);

    vkDestroyShaderModule(deviceHandle, compShaderModule, nullptr);
}