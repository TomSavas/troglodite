#include <cassert>
#include <fstream>
#include <optional>

#include "spirv_reflect.h"

#include "vulkan/descriptors.h"
#include "vulkan/material.h"
#include "vulkan/vk_shader.h"
#include "vulkan/vk_init_helpers.h"

std::vector<uint32_t> readFile(std::string path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return std::vector<uint32_t>();
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read((char*)buffer.data(), fileSize);
    file.close();

    return buffer;
}

CacheLoadResult<ShaderModule> ShaderModuleCache::load(ShaderPath path) {
    auto moduleFromCache = cache.find(path);
    if (moduleFromCache != cache.end()) {
        printf("Loading cached module %s - 0x%lx\n", path.filename.c_str(), path.hash());
        return CacheLoadResult<ShaderModule>(true, &moduleFromCache->second);
    }
    printf("Loading new module %s - 0x%lx\n", path.filename.c_str(), path.hash());

    cache.emplace(path, ShaderModule(path));
    ShaderModule& module = cache.at(path);
    module.spirvCode = readFile(path.spirvPath);
    // TODO: add source code here for debugging
    
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;

    createInfo.codeSize = module.spirvCode.size() * sizeof(uint32_t);
    createInfo.pCode = module.spirvCode.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        printf("Failed creating a shader module for %s\n", path.filename.c_str());
        // TODO: remove from cache
        return CacheLoadResult<ShaderModule>(false, nullptr);
    }
    module.module = shaderModule;

    return CacheLoadResult<ShaderModule>(true, &module);
}

CacheLoadResult<ShaderPassInfo> ShaderPassCache::loadInfo(ShaderStageCreateInfos stageInfos) {
    // We only support up to 4 (TODO: check if only 4, potentially make this dynamic)
    static constexpr uint32_t MAX_ALLOWED_DESCRIPTOR_SETS = 4;

    auto passInfoFromCache = infoCache.find(stageInfos);
    if (passInfoFromCache != infoCache.end()) {
        printf("Loading cached pass info 0x%lx\n", stageInfos.hash());
        return CacheLoadResult<ShaderPassInfo>(true, &passInfoFromCache->second);
    }
    printf("Loading new pass info 0x%lx\n", stageInfos.hash());

    ShaderPassInfo& passInfo = infoCache[stageInfos];
    for (size_t i = 0; i < stageInfos.stages.size(); ++i) {
        auto moduleResult = moduleCache.load(stageInfos.stages[i].path);
        if (!moduleResult.success) {
            printf("Failed retrieving shader module %s\n", stageInfos.stages[i].path.filename.c_str());
            // TODO: remove from cache
            return CacheLoadResult<ShaderPassInfo>(false, nullptr);
        }

        passInfo.stages.push_back(ShaderStage(moduleResult.data, stageInfos.stages[i].stage));
    }

    // Generate pipeline layout from reflected shader info
    struct ReflectedSetLayout {
        std::vector<ReflectedBinding> bindings;
        uint8_t setIndex;
    };
    std::vector<ReflectedSetLayout> reflectedSetLayouts;
    for (size_t i = 0; i < passInfo.stages.size(); ++i) {
        ShaderStage& stage = passInfo.stages[i];

        SpvReflectShaderModule reflection;
        SpvReflectResult result = spvReflectCreateShaderModule(stage.module->spirvCode.size() * sizeof(uint32_t),
            stage.module->spirvCode.data(), &reflection);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        uint32_t reflectedSetCount = 0;
        result = spvReflectEnumerateDescriptorSets(&reflection, &reflectedSetCount, nullptr);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::vector<SpvReflectDescriptorSet*> reflectedSets(reflectedSetCount);
        result = spvReflectEnumerateDescriptorSets(&reflection, &reflectedSetCount, reflectedSets.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        for (SpvReflectDescriptorSet* reflectedSet : reflectedSets) {
            if (reflectedSet == nullptr) {
                continue;
            }

            assert(reflectedSet->set < MAX_ALLOWED_DESCRIPTOR_SETS);

            ReflectedSetLayout setLayout;
            setLayout.setIndex = reflectedSet->set;

            // Bindings
            for (uint32_t bindingIdx = 0; bindingIdx < reflectedSet->binding_count; ++bindingIdx) {
                SpvReflectDescriptorBinding* reflectedBinding = reflectedSet->bindings[bindingIdx];

                VkDescriptorSetLayoutBinding binding = descriptorSetLayoutBinding(static_cast<VkDescriptorType>(reflectedBinding->descriptor_type),
                    reflection.shader_stage, reflectedBinding->binding);
                // TODO: don't quite understand this yet... Multiple descriptors for arrays?
                binding.descriptorCount = 1;
                for (uint32_t dimIdx = 0; dimIdx < reflectedBinding->array.dims_count; ++dimIdx) {
                    binding.descriptorCount *= reflectedBinding->array.dims[dimIdx];
                }
                setLayout.bindings.push_back(ReflectedBinding(std::string(reflectedBinding->name), binding));
            }

            // TODO: Push constants

            // no move?
            reflectedSetLayouts.push_back(std::move(setLayout));
        }
    }

    // We retrieved different set layout descriptions from different stages for the same set -- let's merge 
    // sets with matching indices into a single set description
    std::vector<VkDescriptorSetLayout> mergedSetLayouts;
    for (uint32_t setIdx = 0; setIdx < MAX_ALLOWED_DESCRIPTOR_SETS; ++setIdx) {
        std::vector<ReflectedBinding> mergedBindings;
        for (ReflectedSetLayout& reflectedLayout : reflectedSetLayouts) {
            if (setIdx != reflectedLayout.setIndex) {
                continue;
            }

            // No need for any binary search or anything. Unlikely to have many bindings anyways
            for (ReflectedBinding& reflectedBinding : reflectedLayout.bindings) {
                bool alreadyMerged = false;
                for (ReflectedBinding& mergedBinding : mergedBindings) {
                    if (reflectedBinding.binding.binding != mergedBinding.binding.binding) {
                        continue;
                    }
                    
                    alreadyMerged = true;
                    mergedBinding.binding.stageFlags |= static_cast<VkShaderStageFlagBits>(reflectedBinding.binding.stageFlags);
                    mergedBinding.names.insert(mergedBinding.names.end(),
                        std::make_move_iterator(reflectedBinding.names.begin()),
                        std::make_move_iterator(reflectedBinding.names.end()));
                }

                if (!alreadyMerged) {
                    mergedBindings.push_back(reflectedBinding);
                }
            }
        }

        for (auto& descriptorOverride : stageInfos.overrides) {
            for (auto& mergedBinding : mergedBindings) {
                bool matchesOverride = false;
                for (auto& bindingName : mergedBinding.names) {
                    if (bindingName.compare(descriptorOverride.bindingName) == 0) {
                        matchesOverride = true;
                        break;
                    }
                }

                if (!matchesOverride) {
                    continue;
                }

                printf("Overriding %d descriptor type to %d for binding \"%s\"\n",
                    mergedBinding.binding.descriptorType, descriptorOverride.type, descriptorOverride.bindingName.c_str());
                mergedBinding.binding.descriptorType = descriptorOverride.type;
            }
        }

        VkDescriptorSetLayoutBinding* linearBindings = (VkDescriptorSetLayoutBinding*) alloca(sizeof(VkDescriptorSetLayoutBinding) * mergedBindings.size());
        for (size_t i = 0; i < mergedBindings.size(); ++i) {
            linearBindings[i] = mergedBindings[i].binding;
        }

        // TODO: add flags?
        std::optional<VkDescriptorSetLayout> setLayout = descriptorSetLayoutCache.getLayout(linearBindings, mergedBindings.size());
        if (setLayout && setLayout.value() != VK_NULL_HANDLE) {
            mergedSetLayouts.push_back(setLayout.value());
        }
        passInfo.descriptorSetLayouts.push_back(ShaderPassInfo::DescriptorSetLayout{ setIdx, setLayout.value(), std::move(mergedBindings) });
    }

    // TODO: there should be a pipeline cache. I believe VK has a built-in one?
    VkPipelineLayoutCreateInfo layoutInfo = layoutCreateInfo(mergedSetLayouts.data(), mergedSetLayouts.size());
    vkCreatePipelineLayout(device, &layoutInfo, nullptr, &passInfo.layout);

    return CacheLoadResult<ShaderPassInfo>(true, &passInfo);
}

CacheLoadResult<ShaderPass> ShaderPassCache::loadPass(PassBuildingMaterials& passBuildingMaterials) {
    auto passFromCache = passCache.find(*passBuildingMaterials.info);
    if (passFromCache != passCache.end()) {
        printf("Loading cached pass 0x%lx\n", passBuildingMaterials.info->hash());
        return CacheLoadResult<ShaderPass>(true, &passFromCache->second);
    }
    printf("Loading new pass 0x%lx\n", passBuildingMaterials.info->hash());

    ShaderPass& pass = passCache[*passBuildingMaterials.info];
    pass.info = passBuildingMaterials.info;
    pass.pipeline = passBuildingMaterials.pipelineBuilder.build(device, passBuildingMaterials.renderpass,
        &passBuildingMaterials.viewport, &passBuildingMaterials.scissor, passBuildingMaterials.info->layout);

    return CacheLoadResult<ShaderPass>(true, &pass);
}
