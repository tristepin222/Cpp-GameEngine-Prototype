#include "PipelineBuilder.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <vector>

PipelineBuilder& PipelineBuilder::addInstanceMatrixBinding(uint32_t binding, uint32_t locationStart) {
    // Each mat4 is 4 vec4 attributes at consecutive locations.
    // Binding description for per-instance data:
    VkVertexInputBindingDescription instBinding{};
    instBinding.binding = binding;
    instBinding.stride = sizeof(float) * 16; // mat4 = 16 floats
    instBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    // Attribute descriptions: locationStart..locationStart+3
    VkVertexInputAttributeDescription a0{};
    a0.location = locationStart + 0;
    a0.binding = binding;
    a0.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    a0.offset = sizeof(float) * 0;

    VkVertexInputAttributeDescription a1{};
    a1.location = locationStart + 1;
    a1.binding = binding;
    a1.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    a1.offset = sizeof(float) * 4;

    VkVertexInputAttributeDescription a2{};
    a2.location = locationStart + 2;
    a2.binding = binding;
    a2.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    a2.offset = sizeof(float) * 8;

    VkVertexInputAttributeDescription a3{};
    a3.location = locationStart + 3;
    a3.binding = binding;
    a3.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    a3.offset = sizeof(float) * 12;

    // Append to current lists
    vertexBindings.push_back(instBinding);
    vertexAttributes.push_back(a0);
    vertexAttributes.push_back(a1);
    vertexAttributes.push_back(a2);
    vertexAttributes.push_back(a3);

    return *this;
}

std::pair<VkPipeline, VkPipelineLayout> PipelineBuilder::build(VkDevice device) {
    if (layout == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || vertShader == VK_NULL_HANDLE || fragShader == VK_NULL_HANDLE) {
        throw std::runtime_error("PipelineBuilder: Missing required fields (layout, renderPass, shaders)");
    }

    // 1) Shader stages
    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertShader;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragShader;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertStage, fragStage };

    // 2) Vertex input (may be empty)
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    if (!vertexBindings.empty()) {
        vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindings.size());
        vertexInput.pVertexBindingDescriptions = vertexBindings.data();
    }
    else {
        vertexInput.vertexBindingDescriptionCount = 0;
        vertexInput.pVertexBindingDescriptions = nullptr;
    }

    if (!vertexAttributes.empty()) {
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size());
        vertexInput.pVertexAttributeDescriptions = vertexAttributes.data();
    }
    else {
        vertexInput.vertexAttributeDescriptionCount = 0;
        vertexInput.pVertexAttributeDescriptions = nullptr;
    }

    // 3) Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // 4) Viewport & scissor (we set them dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // 5) Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // 6) Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // 7) Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // 8) Depth/stencil (optional - leave disabled unless user sets up depthAttachment in renderpass)
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // 9) Dynamic state (viewport, scissor)
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // 10) Pipeline layout + render pass are expected to be provided by caller

    // 11) Vertex input and other state are assembled into pipeline create info
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = layout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    VkPipeline pipeline;
    VkResult r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    if (r != VK_SUCCESS) {
        throw std::runtime_error("PipelineBuilder: Failed to create graphics pipeline");
    }

    std::cout << "PipelineBuilder: Pipeline successfully created" << std::endl;
    return { pipeline, layout };
}

VkPipelineLayout PipelineBuilder::createPipelineLayout(
    VkDevice device,
    VkDescriptorSetLayout descriptorSet,
    VkPushConstantRange* pushConstants,
    uint32_t pushConstantCount
) {
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = 1;
    info.pSetLayouts = &descriptorSet;
    info.pushConstantRangeCount = pushConstantCount;
    info.pPushConstantRanges = pushConstants;

    VkPipelineLayout layout;
    if (vkCreatePipelineLayout(device, &info, nullptr, &layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout");

    return layout;
}
