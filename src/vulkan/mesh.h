#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <vk_mem_alloc.h>

#include "vulkan/types.h"
#include "tiny_obj_loader.h"

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

struct Model {
    //std::vector<Vertex> vertices;
    //AllocatedBuffer vertexBuffer;
    std::string name;

    std::vector<Mesh> meshes;

    void loadFromObj(const char* filename, const char* materialDir);
};

struct Mesh {
    std::string name;

    // TODO: have all vertices under the model and only have indices here.
    std::vector<Vertex> vertices;
    AllocatedBuffer vertexBuffer;

    tinyobj::material_t loaderMaterial;

    void loadFromObj(const char* filename, const char* materialDir);
};
