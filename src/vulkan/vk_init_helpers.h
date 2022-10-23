#pragma once

#include <vulkan/vulkan.h>

VkPipelineShaderStageCreateInfo shaderStageCreateInfo(VkShaderStageFlagBits stageFlags, VkShaderModule shaderModule);
VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo();
VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo(VkPrimitiveTopology topology);
VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo(VkPolygonMode polygonMode);
VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo();
VkPipelineColorBlendAttachmentState colorBlendAttachmentState();
VkPipelineLayoutCreateInfo layoutCreateInfo();

VkImageCreateInfo imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
VkImageViewCreateInfo imageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);

VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo(bool depthTest, bool depthWrite, VkCompareOp compareOp);

VkDescriptorSetLayoutBinding descriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);
VkWriteDescriptorSet writeDescriptorBuffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding);

VkBufferCreateInfo bufferCreateInfo(size_t size, VkBufferUsageFlags flags);

VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags);
VkSubmitInfo submitInfo(VkCommandBuffer* cmd);

VkSamplerCreateInfo samplerCreateInfo(VkFilter filters, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
VkWriteDescriptorSet writeDescriptorImage(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding);

VkPipelineViewportStateCreateInfo pipelineViewportState(size_t viewportCount, VkViewport* viewports, size_t scissorCount, VkRect2D* scissor);
VkPipelineColorBlendStateCreateInfo pipelineColorBlendState(bool logicOpEnable, VkLogicOp logicOp, size_t attachmentCount, VkPipelineColorBlendAttachmentState* colorBlendAttachmentStates);

VkDescriptorSetAllocateInfo descriptorSetAllocate(VkDescriptorPool descriptorPool, size_t descriptorSetCount, VkDescriptorSetLayout* setLayouts);

VkCommandPoolCreateInfo commandPoolCreateInfo(uint32_t graphicsQueueFamily, VkCommandPoolCreateFlags flags);
VkCommandBufferAllocateInfo commandBufferAllocateInfo(uint32_t commandBufferCount, VkCommandBufferLevel level, VkCommandPool cmdPool);

VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags);
VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags);

VkDescriptorBufferInfo descriptorBufferInfo(VkBuffer buffer, uint64_t offse, uint64_t range);
VkDescriptorImageInfo descriptorImageInfo(VkBuffer buffer, uint64_t offse, uint64_t range);
