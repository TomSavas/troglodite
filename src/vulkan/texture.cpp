#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "vulkan/texture.h"
#include "vulkan/vk_init_helpers.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "vulkan/engine.h"

bool loadFromFile(VulkanBackend& backend, const char* path, AllocatedImage& image) {
    int width;
    int height;
    int channels;

    stbi_uc* pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        printf("Failed to load texture file %s\n", path);
        return false;
    }

    VkDeviceSize imageSize = width * height * channels * 1; // 1 byte per channel
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;

    AllocatedBuffer cpuImageBuffer = backend.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    backend.uploadData((void*)pixels, imageSize, 0, cpuImageBuffer.allocation);

    stbi_image_free(pixels);

    VkExtent3D imageExtent;
    imageExtent.width = width;
    imageExtent.height = height;
    imageExtent.depth = 1;

    VkImageCreateInfo imgCreateInfo = imageCreateInfo(imageFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);

    VmaAllocationCreateInfo imgAllocInfo = {};
    imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    AllocatedImage newImage;
    vmaCreateImage(backend.allocator, &imgCreateInfo, &imgAllocInfo, &newImage.image, &newImage.allocation, nullptr);

    backend.immediateBlockingSubmit([&](VkCommandBuffer cmd) {
        VkImageSubresourceRange range = {};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        VkImageMemoryBarrier imageMemoryBarrierForTransfer = {};
        imageMemoryBarrierForTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrierForTransfer.pNext = nullptr;

        imageMemoryBarrierForTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrierForTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrierForTransfer.image = newImage.image;
        imageMemoryBarrierForTransfer.subresourceRange = range;

        imageMemoryBarrierForTransfer.srcAccessMask = 0;
        imageMemoryBarrierForTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrierForTransfer);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = imageExtent;

        vkCmdCopyBufferToImage(cmd, cpuImageBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        VkImageMemoryBarrier imageMemoryBarrierForLayoutChange = {};
        imageMemoryBarrierForLayoutChange.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrierForLayoutChange.pNext = nullptr;

        imageMemoryBarrierForLayoutChange.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrierForLayoutChange.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageMemoryBarrierForLayoutChange.image = newImage.image;
        imageMemoryBarrierForLayoutChange.subresourceRange = range;

        imageMemoryBarrierForLayoutChange.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrierForLayoutChange.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrierForLayoutChange);
    });

    backend.deinitQueue.enqueue([=]() {
        //LOG_CALL(vmaDestroyImage(backend.allocator, newImage.image, newImage.allocation));
        vmaDestroyImage(backend.allocator, newImage.image, newImage.allocation);
    });
    vmaDestroyBuffer(backend.allocator, cpuImageBuffer.buffer, cpuImageBuffer.allocation);

    image = newImage;
    return true;
};
