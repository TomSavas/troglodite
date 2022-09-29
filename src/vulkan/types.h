#pragma once

#include <vk_mem_alloc.h>

struct Material {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};

struct Mesh;
struct RenderObject {
    Mesh* mesh;
    Material* material;
    glm::mat4 modelMatrix;
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
};

struct AllocatedImage {
    VkImage image;
    VmaAllocation allocation;
};
