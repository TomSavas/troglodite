#pragma once 

#include <string>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

#include "vulkan/mesh.h"
#include "vulkan/pipeline_builder.h"
#include "vulkan/texture.h"
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

    COMPOSIT               ,
    POST_UI_PPFX           ,

    PRESENT                ,

    PASS_COUNT
};

struct MaterialBuilder;
struct PassBuildingMaterials {
    MaterialBuilder& builder;

    PassType type;
    ShaderPassInfo* info;
    PipelineBuilder pipelineBuilder;
    VkRenderPass renderpass;

    //TEMP: 
    VkViewport* viewport;
    VkRect2D* scissor;

    std::vector<uint8_t> perInFlightFrameDescriptorSetIndices;

    PassBuildingMaterials(MaterialBuilder& builder) : builder(builder) {}

    PassBuildingMaterials(MaterialBuilder& builder, PassType type, ShaderPassInfo* info,
        PipelineBuilder pipelineBuilder, VkRenderPass renderpass, VkViewport* viewport,
        VkRect2D* scissor) : builder(builder), type(type), info(info), pipelineBuilder(pipelineBuilder),
            renderpass(renderpass), viewport(viewport), scissor(scissor) {}

    MaterialBuilder& endPass();
    //PassBuildingMaterials& setDescriptorSetBindingExpectation(uint8_t setIndex, std::vector<VkDescriptorSetLayoutBinding> bindingExpectations);
    PassBuildingMaterials& perInFlightFramesDescriptorSets(size_t inFlightFrames, std::vector<uint8_t> descriptorSetIndices);
};

struct MaterialBuilder {
    std::string materialName;
    std::vector<PassBuildingMaterials> buildingMaterials;
    std::unordered_map<std::string, std::string> defaultTextures;

    PassBuildingMaterials dummyBuildingMaterials;

    static MaterialBuilder begin(std::string name);
    MaterialBuilder& addPass(PassType type, ShaderPassInfo* pass, PipelineBuilder builder, VkRenderPass renderpass, VkViewport* viewport, VkRect2D* scissor);

    MaterialBuilder& addDefaultTexture(std::string name, std::string path);

    PassBuildingMaterials& beginPass(PassType type, ShaderPassInfo* pass, PipelineBuilder builder, VkRenderPass renderpass, VkViewport* viewport, VkRect2D* scissor);
    PassBuildingMaterials& beginPass(PassType type, CacheLoadResult<ShaderPassInfo> pass, PipelineBuilder builder, VkRenderPass renderpass, VkViewport* viewport, VkRect2D* scissor);

private:
    MaterialBuilder(std::string materialName) : materialName(materialName), dummyBuildingMaterials(*this) {}
};

struct MaterialInstance {
    std::unordered_map<std::string, SampledTexture*> textures;
    VkDescriptorSet textureDescriptorSet;
    //settings;
    std::vector<uint32_t> meshInstanceIndices;
};

struct Material {
    std::string name;

    // TODO: we might want to move viewport/scissor here

    // Redo to SoA
    ShaderPass* perPassShaders[static_cast<size_t>(PassType::PASS_COUNT)];
    std::vector<VkDescriptorSet> perPassDescriptorSets[static_cast<size_t>(PassType::PASS_COUNT)];

    std::unordered_map<std::string, SampledTexture*> defaultTextures;
    //defaultSettings; //No clue how to implement type safely

    void bindDescriptorSets(PassType type);

    // TODO: move away into scene
    std::vector<MaterialInstance> instances;
};

struct ShaderPassCache;
struct Materials {
    static constexpr const char* DEFAULT_LIT = "default_lit";

    ShaderPassCache& shaderPassCache;
    std::vector<MaterialBuilder> queuedBuilders;

    std::unordered_map<std::string, Material> materials;
    //std::unordered_map<std::string, MaterialInstance> instances;

    Materials(ShaderPassCache& shaderPassCache) : shaderPassCache{shaderPassCache} {}

    void enqueue(MaterialBuilder&& builder);
    void buildQueued();
};
