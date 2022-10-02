#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "types.h"

struct Camera {
    glm::vec3 pos;
    glm::mat4 rotation;

    float fov;
    float aspectRatio;
    
    float nearClippingPlaneDist;
    float farClippingPlaneDist;
};

struct VulkanBackend;
struct FrameData;
struct Scene {
    Camera mainCamera;

    std::vector<RenderObject> renderables;

    std::unordered_map<std::string, Material> materials;
    std::unordered_map<std::string, Mesh> meshes;

    Material* createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

    void initTestScene();
    void draw(VulkanBackend& backend, VkCommandBuffer cmd, FrameData& frameData);
};
