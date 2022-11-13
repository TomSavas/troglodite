#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "types.h"
#include "texture.h"
#include "material.h"

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

struct MeshInstance {
    Mesh mesh;
    std::vector<uint32_t> objectDataIndices;
};

struct ObjectData {
    glm::vec3 position;
    glm::vec3 scale;
    glm::mat4 rotation;
};

struct VulkanBackend;
struct FrameData;
struct Material;
struct Scene {
    VulkanBackend* backend;
    Camera mainCamera;

    std::vector<Material*> passMaterials[static_cast<size_t>(PassType::PASS_COUNT)];
    std::vector<MeshInstance> meshInstances;
    std::vector<ObjectData> objectData;

    std::vector<RenderObject> renderables;

    std::unordered_map<std::string, TEMPMaterial> materials;
    std::unordered_map<std::string, Mesh> meshes;
    std::unordered_map<std::string, Texture> textures;

    TEMPMaterial* createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

    Scene(VulkanBackend* backend = nullptr) : backend(backend) {}
    void initTestScene();

    struct Object {
        std::string materialName;
        std::vector<uint32_t> materialInstanceIndices;
        std::vector<uint32_t> meshInstanceIndices;
        uint32_t objectDataIndex;
    };
    Object addObject(std::string meshName, std::string materialDir, bool separateMaterialInstances = false);
    
    void update(float dt);
    void draw(VkCommandBuffer cmd, FrameData& frameData);
};
