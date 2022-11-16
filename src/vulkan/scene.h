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

struct MeshInstances {
    Mesh mesh;
    std::vector<uint32_t> objectDataIndices;
};

struct ObjectData {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> scales;
    std::vector<glm::mat4> rotations;

    std::vector<bool> isValidModelMatrixCache;
    std::vector<glm::mat4> modelMatrixCache;
    bool anyModelMatricesInvalid = true;

    size_t pushBackDefaults();
};

struct VulkanBackend;
struct FrameData;
struct Material;
struct Scene {
    VulkanBackend* backend;
    Camera mainCamera;

    std::vector<Material*> passMaterials[static_cast<size_t>(PassType::PASS_COUNT)];
    std::vector<MeshInstances> meshInstances;
    ObjectData objectData;

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
