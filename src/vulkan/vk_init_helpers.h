#pragma once

#include <vulkan/vulkan.h>

struct VertexInputDescription;
struct ShaderStage;

VkPipelineShaderStageCreateInfo shaderStageCreateInfo(VkShaderStageFlagBits stageFlags, VkShaderModule shaderModule);
VkPipelineShaderStageCreateInfo shaderStageCreateInfo(ShaderStage& shaderStage);
VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo();
VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo(VertexInputDescription& description);
VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo(VkPrimitiveTopology topology);
VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo(VkPolygonMode polygonMode, VkCullModeFlagBits cullMode = VK_CULL_MODE_NONE, VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE);
VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo();
VkPipelineColorBlendAttachmentState colorBlendAttachmentState();
VkPipelineLayoutCreateInfo layoutCreateInfo();
VkPipelineLayoutCreateInfo layoutCreateInfo(VkDescriptorSetLayout* descriptorSetLayouts, uint32_t descriptorSetLayoutCount);

VkImageCreateInfo imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent, uint32_t mipLevels = 1);
VkImageViewCreateInfo imageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags, uint32_t mipLevels = 1);

VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo(bool depthTest, bool depthWrite, VkCompareOp compareOp = VK_COMPARE_OP_LESS_OR_EQUAL);

VkDescriptorSetLayoutBinding descriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);
VkWriteDescriptorSet writeDescriptorBuffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding);

VkBufferCreateInfo bufferCreateInfo(size_t size, VkBufferUsageFlags flags);

VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags);
VkSubmitInfo submitInfo(VkCommandBuffer* cmd);

VkSamplerCreateInfo samplerCreateInfo(VkFilter filters, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT, float maxMip = 0);
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

VkImageMemoryBarrier imageMemoryBarrier(VkImageLayout oldLayout, VkImageLayout newLayout, VkImage image, VkPipelineStageFlags srcAccessMask, VkPipelineStageFlags dstAccessMask, uint32_t mipLevels = 1);
VkImageBlit imageBlit(uint32_t srcMip, VkOffset3D srcMipSize, uint32_t dstMip, VkOffset3D dstMipSize);

VkClearValue defaultColorClearValue();
VkClearValue defaultDepthClearValue();
