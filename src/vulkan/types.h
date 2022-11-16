#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
};

struct AllocatedImage {
    VkImage image;
    VmaAllocation allocation;
    VkExtent3D extent;
};
