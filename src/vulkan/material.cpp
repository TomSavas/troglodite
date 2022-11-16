#include "vulkan/material.h"

#include "vulkan/cache.h"
#include "vulkan/vk_init_helpers.h"

MaterialBuilder& PassBuildingMaterials::endPass() {
    return this->builder;
}

PassBuildingMaterials& PassBuildingMaterials::perInFlightFramesDescriptorSets(size_t inFlightFrames, std::vector<uint8_t> descriptorSetIndices) {
    // TODO: actually implement per in flight frame descriptor sets
    assert(false);
    perInFlightFrameDescriptorSetIndices.insert(perInFlightFrameDescriptorSetIndices.begin(), descriptorSetIndices.begin(), descriptorSetIndices.end());

    return *this;
}

/*static*/ MaterialBuilder MaterialBuilder::begin(std::string name) {
    return MaterialBuilder(name);
}

MaterialBuilder& MaterialBuilder::addPass(PassType type, ShaderPassInfo* passInfo, PipelineBuilder builder, VkRenderPass renderpass, VkViewport& viewport, VkRect2D& scissor) {
    assert(passInfo != nullptr);

    for (size_t i = 0; i < passInfo->stages.size(); ++i) {
        builder.shaderStages.push_back(shaderStageCreateInfo(passInfo->stages[i].flags, passInfo->stages[i].module->module));
    }

    buildingMaterials.emplace_back(*this, type, passInfo, builder, renderpass, viewport, scissor);

    return *this;
}

PassBuildingMaterials& MaterialBuilder::beginPass(PassType type, ShaderPassInfo* passInfo, PipelineBuilder builder, VkRenderPass renderpass, VkViewport& viewport, VkRect2D& scissor) {
    assert(passInfo != nullptr);

    for (size_t i = 0; i < passInfo->stages.size(); ++i) {
        builder.shaderStages.push_back(shaderStageCreateInfo(passInfo->stages[i].flags, passInfo->stages[i].module->module));
    }

    return buildingMaterials.emplace_back(*this, type, passInfo, builder, renderpass, viewport, scissor);
}

MaterialBuilder& MaterialBuilder::addDefaultTexture(std::string name, std::string path) {
    defaultTextures[name] = path;

    return *this;
}

void Materials::enqueue(MaterialBuilder&& builder) {
    queuedBuilders.push_back(builder);
}

void Materials::buildQueued() {
    // TODO: materials and pipeline storage has to be redone to SoA to lend itself nicely
    // to vkCreateGraphicsPipelines

    for (MaterialBuilder& builder : queuedBuilders) {
        if (materials.find(builder.materialName) != materials.end()) {
            printf("Material \"%s\" found. Skipping building", builder.materialName.c_str());
            continue;
        }
        Material& material = materials[builder.materialName];
        material.name = builder.materialName;

        for (auto& textureInfo : builder.defaultTextures) {
            // TODO: load
            material.defaultTextures[textureInfo.first] = nullptr;
        }

        for (PassBuildingMaterials passBuildingMaterials : builder.buildingMaterials) {
            CacheLoadResult<ShaderPass> passResult = shaderPassCache.loadPass(passBuildingMaterials);
            if (!passResult.success) {
                continue;
            }

            material.perPassShaders[static_cast<size_t>(passBuildingMaterials.type)] = passResult.data;
            // TODO: load default textures, create descriptor sets
        }
    }
}
