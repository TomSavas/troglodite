#pragma once 

#include <string>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

#include "vulkan/mesh.h"
#include "vulkan/pipeline_builder.h"
#include "vulkan/vk_shader.h"

enum class PassType : uint8_t {
    DIRECTIONAL_SHADOW  = 0,
    POINT_SHADOW           ,

    FORWARD_OPAQUE         ,
    FORWARD_TRANSPARENT    ,

    VFX                    ,

    PARTICLE               ,

    UI                     ,

    PRE_UI_PPFX            ,
    UI_PPFX                ,
    POST_UI_PPFX           ,

    PASS_COUNT
};

struct PassBuildingMaterials {
    PassType type;
    ShaderPassInfo* info;
    PipelineBuilder pipelineBuilder;
    VkRenderPass renderpass;

    //TEMP: 
    VkViewport& viewport;
    VkRect2D& scissor;
};

struct MaterialBuilder {
    std::string materialName;
    std::vector<PassBuildingMaterials> buildingMaterials;

    static MaterialBuilder begin(std::string name);
    MaterialBuilder& addPass(PassType type, ShaderPassInfo* pass, PipelineBuilder builder, VkRenderPass renderpass, VkViewport& viewport, VkRect2D& scissor);

private:
    MaterialBuilder(std::string materialName) : materialName(materialName) {}
};

struct Material {
    std::string name;

    // TODO: we might want to move viewport/scissor here

    // Redo to SoA
    ShaderPass* perPassShaders[static_cast<size_t>(PassType::PASS_COUNT)];
    std::vector<VkDescriptorSet> perPassDescriptorSets[static_cast<size_t>(PassType::PASS_COUNT)];

    //std::unordered_map<std::string, SampledTexture*> defaultTextures;
    //defaultSettings; //No clue how to implement type safely
};

struct MaterialInstance {
    Material* material;
    //std::unordered_map<std::string, SampledTexture*> textures;
    //settings;
};

struct ShaderPassCache;
struct Materials {
    static constexpr char* DEFAULT_LIT = "default_lit";

    ShaderPassCache& shaderPassCache;
    std::vector<MaterialBuilder> queuedBuilders;

    std::unordered_map<std::string, Material> materials;
    std::unordered_map<std::string, MaterialInstance> instances;

    Materials(ShaderPassCache& shaderPassCache) : shaderPassCache{shaderPassCache} {}

    void enqueue(MaterialBuilder&& builder);
    void buildQueued();
};
