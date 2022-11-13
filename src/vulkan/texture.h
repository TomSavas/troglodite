#pragma once

#include <string>
#include <unordered_map>

#include "vulkan/types.h"
#include "vulkan/cache.h"

struct VulkanBackend;
bool loadFromFile(VulkanBackend& backend, const char* path, AllocatedImage& image);

struct Texture {
    AllocatedImage image;
    VkImageView view;
};

struct SampledTexture {
    Texture* texture;
    VkSampler sampler;
};

struct TextureCache {
    VulkanBackend& backend;
    std::unordered_map<std::string, Texture> cache;

    TextureCache(VulkanBackend& backend) : backend(backend) {}

    // TODO: separately cache AllocatedImages, VkImageViews and VkSamplers and combine them
    // as needed.
    CacheLoadResult<Texture> load(std::string path);
    CacheLoadResult<SampledTexture> load(std::string path, VkSamplerCreateInfo sampler);
};
