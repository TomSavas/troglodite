#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "vulkan/texture.h"
#include "vulkan/vk_init_helpers.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "vulkan/engine.h"

CacheLoadResult<Texture> TextureCache::load(std::string path, bool generateMips) {
    auto textureFromCache = cache.find(path);
    if (textureFromCache != cache.end()) {
        //printf("Loading cached texture %s\n", path.c_str());
        return CacheLoadResult<Texture>(true, &textureFromCache->second);
    } else {
        //printf("Loading new texture %s\n", path.c_str());
    }

    int width;
    int height;
    int channels;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    int mipCount = floor(log2((double)std::min(width, height))) + 1;

    if (!pixels) {
        printf("Failed to load texture file %s\n", path.c_str());
        return CacheLoadResult<Texture>(false, nullptr);
    }

    // TODO: different image formats depending on how many channels
    VkDeviceSize imageSize = width * height * 4 * 1; // 1 byte per channel
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;

    AllocatedBuffer cpuImageBuffer = backend.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    backend.uploadData((void*)pixels, imageSize, 0, cpuImageBuffer.allocation);

    stbi_image_free(pixels);

    Texture& texture = cache[path];
    texture.mipCount = mipCount;

    texture.image.extent.width = width;
    texture.image.extent.height = height;
    texture.image.extent.depth = 1;

    VkImageCreateInfo imgCreateInfo = imageCreateInfo(imageFormat,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        texture.image.extent, mipCount);

    VmaAllocationCreateInfo imgAllocInfo = {};
    imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(backend.allocator, &imgCreateInfo, &imgAllocInfo, &texture.image.image,
        &texture.image.allocation, nullptr);

    backend.immediateBlockingSubmit([&](VkCommandBuffer cmd) {
        VkImageMemoryBarrier imageMemoryBarrierForTransfer = imageMemoryBarrier(VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.image.image, 0, VK_ACCESS_TRANSFER_WRITE_BIT, mipCount);

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
            0, nullptr, 1, &imageMemoryBarrierForTransfer);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = texture.image.extent;

        vkCmdCopyBufferToImage(cmd, cpuImageBuffer.buffer, texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        // Mip generation
        VkImageMemoryBarrier finalFormatTransitionBarrier = imageMemoryBarrier(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture.image.image, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT, 1);
        VkImageMemoryBarrier mipIntermediateTransitionBarrier = imageMemoryBarrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture.image.image, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT, 1);
        int32_t mipWidth = width;
        int32_t mipHeight = height;
        for (int i = 1; i < mipCount; ++i) {
            int32_t lastMipWidth = mipWidth;
            int32_t lastMipHeight = mipHeight;
            mipWidth /= 2;
            mipHeight /= 2;

            // Transition the last mip to SRC_OPTIMAL
            mipIntermediateTransitionBarrier.subresourceRange.baseMipLevel = i - 1;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &mipIntermediateTransitionBarrier);

            // Blit last mip to downsized current one
            VkImageBlit blit = imageBlit(i - 1, { lastMipWidth, lastMipHeight, 1 }, i, { mipWidth, mipHeight, 1 });
            vkCmdBlitImage(cmd, texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &blit, VK_FILTER_LINEAR);
            
            // Finally transition last mip to SHADER_READ_ONLY_OPTIMAL
            finalFormatTransitionBarrier.subresourceRange.baseMipLevel = i - 1;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &finalFormatTransitionBarrier);
        }

        // Transition the highest mip directly to SHADER_READ_ONLY_OPTIMAL
        finalFormatTransitionBarrier.subresourceRange.baseMipLevel = mipCount - 1;
        finalFormatTransitionBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        finalFormatTransitionBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &finalFormatTransitionBarrier);
    });

    backend.deinitQueue.enqueue([=]() {
        //LOG_CALL(vmaDestroyImage(backend.allocator, texture.image.image, texture.image.allocation));
        vmaDestroyImage(backend.allocator, texture.image.image, texture.image.allocation);
    });
    vmaDestroyBuffer(backend.allocator, cpuImageBuffer.buffer, cpuImageBuffer.allocation);

    VkImageViewCreateInfo imageViewInfo = imageViewCreateInfo(VK_FORMAT_R8G8B8A8_SRGB, texture.image.image, VK_IMAGE_ASPECT_COLOR_BIT, mipCount);
    vkCreateImageView(backend.device, &imageViewInfo, nullptr, &texture.view);
    //scene->textures["lost_empire_diffuse"] = *textureResult.texture;

    //deinitQueue.enqueue([&]() {
    //    LOG_CALL(vkDestroyImageView(device, scene->textures["lost_empire_diffuse"].view, nullptr));
    //});

    return CacheLoadResult<Texture>(true, &texture);
}

CacheLoadResult<SampledTexture> TextureCache::load(std::string path, VkSamplerCreateInfo _) {
    CacheLoadResult<Texture> texture = load(path);
    if (!texture.success) {
        return CacheLoadResult<SampledTexture>(false, nullptr);
    }

    // TODO: memleak, but it's fine for now
    CacheLoadResult<SampledTexture> result(true, nullptr);
    result.data = new SampledTexture();
    result.data->texture = texture.data;

    // TODO: cache samplers
    VkSamplerCreateInfo samplerInfo = samplerCreateInfo(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, (float)texture.data->mipCount - 1);
    vkCreateSampler(backend.device, &samplerInfo, nullptr, &result.data->sampler);

    backend.deinitQueue.enqueue([&]() {
        vkDestroySampler(backend.device, result.data->sampler, nullptr);
        delete result.data;
    });

    return result;
}
