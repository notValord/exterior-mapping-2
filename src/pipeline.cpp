#include <pipeline.hpp>
#include <swapchain.hpp>
#include <vertex.hpp>
#include <descriptorManager.hpp>
#include <structs.hpp>

/**
 * @brief Reads a binary file into a char buffer.
 *
 * Loads a SPIR-V shader or other binary data from disk into memory.
 * @param filename Path to the file to read.
 * @return Buffer containing the file contents.
 * @throws std::runtime_error if the file cannot be opened.
 */
static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary); // start reading at the end

    if (!file.is_open()){
        throw std::runtime_error("Failed to open file: " + filename);
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}


/**
 * @struct GraphicsPipelineWriter
 * @brief Helper for constructing Vulkan graphics pipeline state infos.
 *
 * Provides a set of static helper functions for building common pipeline state
 * create-info structures used during graphics pipeline creation.
 */
struct GraphicsPipelineWriter {
    static VkPipelineShaderStageCreateInfo shaderStageCI(const VkShaderModule shaderModule, VkShaderStageFlagBits shaderFlag) {
        return VkPipelineShaderStageCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = shaderFlag,
            .module = shaderModule,
            .pName = "main"
        };
    }

    static VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI(VkPrimitiveTopology topology, VkBool32 restartFlag = VK_FALSE) {
        return VkPipelineInputAssemblyStateCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = topology,
            .primitiveRestartEnable = restartFlag
        };
    }

    static VkPipelineDepthStencilStateCreateInfo depthStencilCI(DepthStencilFlags depthFlags) {
        return VkPipelineDepthStencilStateCreateInfo {
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
    }

    static VkPipelineRasterizationStateCreateInfo rasterizetionCI(RasterizationFlags rastFlags) {
        return VkPipelineRasterizationStateCreateInfo {
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
    }

    static VkPipelineMultisampleStateCreateInfo multisampleStatiCI() {
        return VkPipelineMultisampleStateCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE
        };
    }

    static VkPipelineColorBlendAttachmentState blendingAttachment() {
        return VkPipelineColorBlendAttachmentState {
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        };
    }

    static VkPipelineColorBlendStateCreateInfo colorBlendingCI(const VkPipelineColorBlendAttachmentState& colorBlendAttachment) {
        return VkPipelineColorBlendStateCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
            .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
        };
    }
};



GraphicPipeline::GraphicPipeline(VkDevice device,
                                 VkRenderPass renderPass,
                                 const PipelineLayoutSetup& layoutSetup,
                                 const GraphicSetup& pipelineSetup,
                                 const PipelineBuilder& builder)
                                 : deviceHandle(device), vertFragShaders(pipelineSetup.shaderFiles) {
    pipelineLayout = builder.createPipelineLayout(layoutSetup);
    pipeline = builder.createGraphicsPipeline(pipelineLayout, renderPass, pipelineSetup);
}

GraphicPipeline::~GraphicPipeline() {
    vkDestroyPipeline(deviceHandle, pipeline, nullptr);
    vkDestroyPipelineLayout(deviceHandle, pipelineLayout, nullptr);
}



ComputePipeline::ComputePipeline(VkDevice device,
                                 const PipelineLayoutSetup& layoutSetup,
                                 const ComputeShader& shaderFile,
                                 const PipelineBuilder& builder)
                                 : deviceHandle(device), computeShader(shaderFile) {
    pipelineLayout = builder.createPipelineLayout(layoutSetup);
    pipeline = builder.createComputePipeline(pipelineLayout, computeShader);
}

ComputePipeline::~ComputePipeline() {
    vkDestroyPipeline(deviceHandle, pipeline, nullptr);
    vkDestroyPipelineLayout(deviceHandle, pipelineLayout, nullptr);
}



PipelineBuilder::PipelineBuilder(VkDevice device)
    : deviceHandle(device) {
    ;
}

VkShaderModule PipelineBuilder::createShaderModule(const std::string& fileName) const {
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

VkPipelineLayout PipelineBuilder::createPipelineLayout(const PipelineLayoutSetup& layoutSetup) const {
    VkPushConstantRange pushConstantRange;

    VkPipelineLayoutCreateInfo pipelineLayoutCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(layoutSetup.descriptorVec.size()),
        .pSetLayouts = layoutSetup.descriptorVec.data(),
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };

    if (layoutSetup.pushConstants == 1) {                            // deal with later
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(glm::mat4); // model * view * proj
    }
    else if (layoutSetup.pushConstants == 2) {
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(glm::mat4) + 2*sizeof(uint32_t); // material flag
    }
    else if (layoutSetup.pushConstants > 2) {
        throw std::runtime_error("Multiple push constants not impelmented yet!");
    }

    if (layoutSetup.pushConstants > 0) {
        pipelineLayoutCI.pushConstantRangeCount = 1;
        pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
    }

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(deviceHandle, &pipelineLayoutCI, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipelineLayout!");
    };

    return pipelineLayout;
}
    
VkPipeline PipelineBuilder::createComputePipeline(VkPipelineLayout pipelineLayout, const ComputeShader& shaderFile) const {
    VkShaderModule compShaderModule = createShaderModule(shaderFile.comp);

    VkPipelineShaderStageCreateInfo shaderStageCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = compShaderModule,
        .pName = "main",
    };

    VkComputePipelineCreateInfo computePipelineCI {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shaderStageCI,
        .layout = pipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VkPipeline pipeline;
    if  (vkCreateComputePipelines(deviceHandle, VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a compute pipeline.");
    }

    vkDestroyShaderModule(deviceHandle, compShaderModule, nullptr);

    return pipeline;
}

VkRenderPass PipelineBuilder::createRenderPass(const AttachementDescription& colorAttachment, const AttachementDescription& depthAttachment) const {
    VkAttachmentDescription colorAttachmentD{
        .format = colorAttachment.imageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = colorAttachment.loadOp,
        .storeOp = colorAttachment.storeOp,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = colorAttachment.initialLayout,
        .finalLayout = colorAttachment.finalLayout // we transfer it to present state if ui is not used
    };

    VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentDescription depthAttachmentD{
        .format = depthAttachment.imageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = depthAttachment.loadOp,
        .storeOp = depthAttachment.storeOp,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = depthAttachment.initialLayout,
        .finalLayout = depthAttachment.finalLayout
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
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachmentD, depthAttachmentD};
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

VkPipeline PipelineBuilder::createGraphicsPipeline(VkPipelineLayout pipelineLayout, VkRenderPass renderPass, const GraphicSetup& pipelineSetup) const {
    VkShaderModule vertShaderModule = createShaderModule(pipelineSetup.shaderFiles.vert);
    VkShaderModule fragShaderModule = createShaderModule(pipelineSetup.shaderFiles.frag);

    std::array<VkPipelineShaderStageCreateInfo, 2> pipelineShaderStageCI{};
    pipelineShaderStageCI[0] = GraphicsPipelineWriter::shaderStageCI(vertShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
    pipelineShaderStageCI[1] = GraphicsPipelineWriter::shaderStageCI(fragShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);

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

    if (pipelineSetup.vertexInput == VertexInputFlags::POS) {
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
    else if (pipelineSetup.vertexInput == VertexInputFlags::POS_COL) {
        bindingDescription = Point::getBindingDescription();
        attributeDescription = Point::getAttributeDescriptions();
    }
    else if (pipelineSetup.vertexInput == VertexInputFlags::POS_COL_CAMID) {
        bindingDescription = CloudPoint::getBindingDescription();
        attributeDescription = CloudPoint::getAttributeDescriptions();
    }
    else if (pipelineSetup.vertexInput == VertexInputFlags::POS_COL_UV_NORM) {
        bindingDescription = Vertex::getBindingDescription();
        attributeDescription = Vertex::getAttributeDescriptions();
    }
    else if (pipelineSetup.vertexInput == VertexInputFlags::NONE) {
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

    if (pipelineSetup.vertexInput != VertexInputFlags::NONE) {
        vertexInputStateCI.vertexBindingDescriptionCount = 1;
        vertexInputStateCI.pVertexBindingDescriptions = &bindingDescription;
        vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
        vertexInputStateCI.pVertexAttributeDescriptions = attributeDescription.data();
    }

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = GraphicsPipelineWriter::inputAssemblyCI(pipelineSetup.topology);

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

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = GraphicsPipelineWriter::depthStencilCI(pipelineSetup.depthFlags);
    VkPipelineRasterizationStateCreateInfo rasterizationStateCI = GraphicsPipelineWriter::rasterizetionCI(pipelineSetup.rastFlags);
    VkPipelineMultisampleStateCreateInfo multisamplingStateCI = GraphicsPipelineWriter::multisampleStatiCI();

    // depth and stencil testing also required here
    VkPipelineColorBlendAttachmentState colorBlendAttachment = GraphicsPipelineWriter::blendingAttachment();
    VkPipelineColorBlendStateCreateInfo colorBlending = GraphicsPipelineWriter::colorBlendingCI(colorBlendAttachment);

    VkGraphicsPipelineCreateInfo graphicsPipelineCI{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = static_cast<uint32_t>(pipelineShaderStageCI.size()),
        .pStages = pipelineShaderStageCI.data(),
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

RenderPassManager::RenderPassManager(VkDevice device, AttachementsFormats attachFormats, const PipelineBuilder& builder) 
        : deviceHandle(device) {
    AttachementDescription color {
        .imageFormat = attachFormats.colorImageFormat,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
    AttachementDescription depth {
        .imageFormat = attachFormats.depthFormat,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };
    renderPass = builder.createRenderPass(color, depth);

    color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    onTopRenderPass = builder.createRenderPass(color, depth);
}

RenderPassManager::~RenderPassManager() {
    vkDestroyRenderPass(deviceHandle, renderPass, nullptr);
    vkDestroyRenderPass(deviceHandle, onTopRenderPass, nullptr);
}

PipelineManager::PipelineManager(VkDevice device, const AttachementsFormats& imageFormats, DescriptorManager& descrMan)
    : builder(device),
      renderPassMan(device, imageFormats, builder),
      intersectPipeline(device, 
                        PipelineLayoutSetup{{descrMan.computeDescriptors.descriptorSetLayout,
                                             descrMan.computeDescriptors.sharedDescriptorSetLayout}},
                        intersectFile,
                        builder),
      pointCloudPipeline(device,
                         PipelineLayoutSetup{{descrMan.pointCloudDescriptors.descriptorSetLayout,
                                              descrMan.computeDescriptors.sharedDescriptorSetLayout}},
                         pointCloudFile,
                         builder),
      rayDataPipeline(device,
                      PipelineLayoutSetup({descrMan.rayDataDescriptors.descriptorSetLayout, descrMan.rayDataDescriptors.storageDescriptorSetLayout}),
                      rayDataFile,
                      builder),
      reduceDepthPipeline(device,
                          PipelineLayoutSetup({descrMan.reduceDescriptors.descriptorSetLayout}),
                          reduceDepthFile,
                          builder),
      novelSynthPipeline(device,
                         PipelineLayoutSetup({descrMan.reduceDescriptors.descriptorSetLayout,
                                              descrMan.novelSynthDescriptors.descriptorSetLayout,
                                              descrMan.rayDataDescriptors.storageDescriptorSetLayout,
                                              descrMan.novelSynthDescriptors.debugDescriptorSetLayout}),
                         novelSynthFile,
                         builder),
      novelReconPipeline(device,
                         PipelineLayoutSetup({descrMan.novelReconDescriptors.descriptorSetLayout,
                                              descrMan.novelSynthDescriptors.debugDescriptorSetLayout,
                                              descrMan.novelReconDescriptors.resultDescriptorSetLayout}),
                         novelReconFile,
                        builder),
      renderPipeline(device,
                     renderPassMan.renderPass,
                     PipelineLayoutSetup{{descrMan.renderDescriptors.descriptorSetLayout,
                                          descrMan.renderDescriptors.samplerDescriptorSetLayout}, 2},
                     setupRenderPipeline(),
                     builder),
      renderTransparentPipeline(device,
                                renderPassMan.onTopRenderPass,
                                PipelineLayoutSetup{{descrMan.renderDescriptors.descriptorSetLayout,
                                                     descrMan.renderDescriptors.samplerDescriptorSetLayout}, 2},
                                setupRenderTransparentPipeline(),
                                builder),
      frustumPipeline(device,
                      renderPassMan.onTopRenderPass,
                      PipelineLayoutSetup{{descrMan.frustumDescriptors.descriptorSetLayout}},
                      setupFrustumPipeline(),
                      builder),
      linePipeline(device,
                   renderPassMan.onTopRenderPass,
                   PipelineLayoutSetup{{descrMan.frustumDescriptors.descriptorSetLayout}},
                   setupLinePipeline(),
                   builder),
      camCubePipeline(device,
                      renderPassMan.onTopRenderPass,
                      PipelineLayoutSetup{{descrMan.camCubeDescriptors.descriptorSetLayout}, 1},
                      setupCamCubePipeline(),
                      builder),
      offlinePipeline(device,
                      renderPassMan.renderPass,
                      PipelineLayoutSetup{{descrMan.offlineDescriptors.descriptorSetLayout,
                                           descrMan.computeDescriptors.sharedDescriptorSetLayout}},
                      setupOfflinePipeline(),
                      builder),
      pointPipeline(device,
                    renderPassMan.renderPass,
                    PipelineLayoutSetup{{descrMan.frustumDescriptors.descriptorSetLayout, descrMan.pointCloudDescriptors.imageDescriptorSetLayout}},
                    setupPointPipeline(),
                    builder) {
    ;
}

GraphicSetup PipelineManager::setupRenderPipeline(){
    DepthStencilFlags depthFlags{
        .testEnable = VK_TRUE,
        .writeEnable = VK_TRUE
    };
    RasterizationFlags rastFlags{
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
    };

    return GraphicSetup {
        .shaderFiles = renderFiles,
        .vertexInput = VertexInputFlags::POS_COL_UV_NORM,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .depthFlags = depthFlags,
        .rastFlags = rastFlags,
    };
}

GraphicSetup PipelineManager::setupRenderTransparentPipeline() {
    DepthStencilFlags depthFlags{
        .testEnable = VK_TRUE,
        .writeEnable = VK_FALSE
    };
    RasterizationFlags rastFlags{
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
    };

    return GraphicSetup {
        .shaderFiles = renderFiles,
        .vertexInput = VertexInputFlags::POS_COL_UV_NORM,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .depthFlags = depthFlags,
        .rastFlags = rastFlags,
    };
}

GraphicSetup PipelineManager::setupFrustumPipeline(){
    DepthStencilFlags depthFlags{
        .testEnable = VK_TRUE,
        .writeEnable = VK_FALSE
    };
    RasterizationFlags rastFlags{
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
    };

    return GraphicSetup {
        .shaderFiles = frustumFiles,
        .vertexInput = VertexInputFlags::POS_COL,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .depthFlags = depthFlags,
        .rastFlags = rastFlags,
    };
}

GraphicSetup PipelineManager::setupLinePipeline(){
    DepthStencilFlags depthFlags{
        .testEnable = VK_TRUE,
        .writeEnable = VK_FALSE
    };
    RasterizationFlags rastFlags{
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
    };

    return GraphicSetup {
        .shaderFiles = lineFiles,
        .vertexInput = VertexInputFlags::POS,
        .topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
        .depthFlags = depthFlags,
        .rastFlags = rastFlags,
    };
}

GraphicSetup PipelineManager::setupCamCubePipeline(){
    DepthStencilFlags depthFlags{
        .testEnable = VK_TRUE,
        .writeEnable = VK_FALSE
    };
    RasterizationFlags rastFlags{
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
    };

    return GraphicSetup {
        .shaderFiles = camCuberFiles,
        .vertexInput = VertexInputFlags::NONE,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .depthFlags = depthFlags,
        .rastFlags = rastFlags,
    };
}

GraphicSetup PipelineManager::setupOfflinePipeline(){
    DepthStencilFlags depthFlags{
        .testEnable = VK_FALSE,
        .writeEnable = VK_FALSE
    };
    RasterizationFlags rastFlags{
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
    };

    return GraphicSetup {
        .shaderFiles = offlineFiles,
        .vertexInput = VertexInputFlags::NONE,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .depthFlags = depthFlags,
        .rastFlags = rastFlags,
    };
}

GraphicSetup PipelineManager::setupPointPipeline() {
    DepthStencilFlags depthFlags{
        .testEnable = VK_TRUE,
        .writeEnable = VK_TRUE
    };
    RasterizationFlags rastFlags{
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
    };

    return GraphicSetup {
        .shaderFiles = pointRendFiles,
        .vertexInput = VertexInputFlags::POS_COL_CAMID,
        .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
        .depthFlags = depthFlags,
        .rastFlags = rastFlags,
    };
}