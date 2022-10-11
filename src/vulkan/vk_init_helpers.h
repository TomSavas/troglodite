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
