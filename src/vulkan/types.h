#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>

struct TEMPMaterial {
    VkDescriptorSet textureSet {VK_NULL_HANDLE};
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};

struct Mesh;
struct MaterialInstance;
struct RenderObject {
    Mesh* mesh;
    TEMPMaterial* material;
    glm::mat4 modelMatrix;

    MaterialInstance* tempInstance;
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
};

struct AllocatedImage {
    VkImage image;
    VmaAllocation allocation;
    VkExtent3D extent;
};
