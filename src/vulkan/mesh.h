#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <vk_mem_alloc.h>

#include "vulkan/types.h"

struct MeshPushConstants {
    glm::vec4 data;
    glm::mat4 MVP;
};

struct VertexInputDescription {
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;

    static VertexInputDescription getVertexDescription();
};

struct Mesh {
    std::vector<Vertex> vertices;
    AllocatedBuffer vertexBuffer;

    void loadFromObj(const char* filename);
};
