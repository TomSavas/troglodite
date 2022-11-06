#pragma once

#include <vector>

#include <vulkan/vulkan.h>

struct PipelineBuilder {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo multisampling;
    VkPipelineLayout pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo depthStencil;

    VkPipeline build(VkDevice device, VkRenderPass pass, VkViewport* viewport, VkRect2D* scissor, VkPipelineLayout& pipelineLayout );
};
