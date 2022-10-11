#pragma once

#include "vulkan/types.h"

struct VulkanBackend;
bool loadFromFile(VulkanBackend& backend, const char* path, AllocatedImage& image);

struct Texture {
    AllocatedImage image;
    VkImageView view;
};
