#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "types.h"
#include "texture.h"

struct Camera {
    glm::vec3 pos;
    glm::mat4 rotation = glm::mat4(1.0);

    float fov;
    float aspectRatio = 4.f / 3.f;
    
    float nearClippingPlaneDist;
    float farClippingPlaneDist;

    float moveSpeed = 50.f;
    float horizontalRotationSpeed = 0.005f;
};

struct VulkanBackend;
struct FrameData;
struct Scene {
    Camera mainCamera;

    std::vector<RenderObject> renderables;

    std::unordered_map<std::string, Material> materials;
    std::unordered_map<std::string, Mesh> meshes;
    std::unordered_map<std::string, Texture> textures;

    Material* createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

    void initTestScene();
    void update(VulkanBackend& backend, float dt);
    void draw(VulkanBackend& backend, VkCommandBuffer cmd, FrameData& frameData);
};
