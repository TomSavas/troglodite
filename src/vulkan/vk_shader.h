#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "vulkan/vulkan.h"
#include "vulkan/cache.h"

#define SHADER_SRC_PATH "../src/shaders/"
#define SHADER_SPIRV_PATH "./shaders/"
#define SHADER_SPIRV_EXTENSION ".spv"

#define SHADER_FILENAME_AND_SPIRV_AND_SRC_PATH(filename) filename, SHADER_SPIRV_PATH filename SHADER_SPIRV_EXTENSION, SHADER_SRC_PATH filename

struct ShaderPath {
    std::string filename;
    std::string spirvPath;
    std::string sourcePath;

    // File timestamps? -> mv ShaderFileInfo

    ShaderPath(std::string filename, std::string spirvPath, std::string sourcePath) : filename(filename), spirvPath(spirvPath), sourcePath(sourcePath) {}

    bool operator==(const ShaderPath& other) const {
        return spirvPath.compare(other.spirvPath) == 0;
    }

    size_t hash() const {
        return std::hash<std::string>{}(spirvPath);
    }

    struct Hash {
        size_t operator() (const ShaderPath& shaderPath) const {
            return shaderPath.hash();
        }
    };
};
#define SHADER_PATH(filename) ShaderPath(SHADER_FILENAME_AND_SPIRV_AND_SRC_PATH(filename))

struct ShaderModule {
    ShaderPath path;

    const char* sourceCode;
    std::vector<uint32_t> spirvCode;

    VkShaderModule module;  

    ShaderModule(ShaderPath path) : path(path) {}
};

struct ShaderStage {
    ShaderModule* module;  
    VkShaderStageFlagBits flags;

    ShaderStage(ShaderModule* module, VkShaderStageFlagBits flags) : module(module), flags(flags) {}
};

struct ShaderModuleCache {
    VkDevice device;
    std::unordered_map<ShaderPath, ShaderModule, ShaderPath::Hash> cache;

    ShaderModuleCache(VkDevice device) : device(device) {}
    CacheLoadResult<ShaderModule> load(ShaderPath path);
};


struct ReflectedBinding {
    // Bindings might have multiple names if merged from different shaders
    std::vector<std::string> names;
    VkDescriptorSetLayoutBinding binding;

    ReflectedBinding(std::string name, VkDescriptorSetLayoutBinding binding) : names({ name }), binding(binding) {}
    ReflectedBinding(std::vector<std::string> names, VkDescriptorSetLayoutBinding binding) : names(names), binding(binding) {}
};

struct ShaderPassInfo {
    std::vector<ShaderStage> stages;

    struct DescriptorSetLayout {
        uint32_t setIndex; // Redundant, but helps debugging
        VkDescriptorSetLayout layout;
        std::vector<ReflectedBinding> bindings;
    };
    std::vector<DescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkPushConstantRange> pushConstants;

    VkPipelineLayout layout;

    bool operator==(const ShaderPassInfo& other) const {
        return layout == other.layout;
    }

    size_t hash() const {
        // ShaderPassInfo should be uniquely defined by a layout : different layout - different pass
        // TODO: this does fuck all until pipeline layouts are cached!!!
        return (size_t)layout;
    }

    struct Hash {
        size_t operator() (const ShaderPassInfo& info) const {
            return info.hash();
        }
    };
};

struct ShaderPass {
    ShaderPassInfo* info;
    VkPipeline pipeline;

    //std::vector<> bindings;
};

struct DescriptorSetLayoutCache;
struct PassBuildingMaterials;
struct ShaderPassCache {
    VkDevice device;
    ShaderModuleCache& moduleCache;
    DescriptorSetLayoutCache& descriptorSetLayoutCache;

    struct ShaderStageCreateInfo {
        ShaderPath path;
        VkShaderStageFlagBits stage;

        // TODO: add specialization constants

        ShaderStageCreateInfo(ShaderPath path, VkShaderStageFlagBits stage) : path(path), stage(stage) {}

        bool operator==(const ShaderStageCreateInfo& other) const {
            return path == other.path && stage == other.stage;
        }

        size_t hash() const {
            // Not great but works for now
            return path.hash() ^ std::hash<size_t>{}(stage);
        }
    };

    struct ShaderStageCreateInfos {
        std::vector<ShaderStageCreateInfo> stages;

        struct DescriptorTypeOverride {
            std::string bindingName;
            VkDescriptorType type;
        };
        std::vector<DescriptorTypeOverride> overrides;

        ShaderStageCreateInfos(std::vector<ShaderStageCreateInfo> unsortedStages,
            std::vector<DescriptorTypeOverride> unsortedOverrides = {}) {
            std::sort(unsortedStages.begin(), unsortedStages.end(), [](ShaderStageCreateInfo a, ShaderStageCreateInfo b) {
                return a.stage < b.stage;
            });
            stages = unsortedStages;

            std::sort(unsortedOverrides.begin(), unsortedOverrides.end(), [](DescriptorTypeOverride a, DescriptorTypeOverride b) {
                return a.bindingName.compare(b.bindingName) < 0;
            });
            overrides = unsortedOverrides;
        }

        bool operator==(const ShaderStageCreateInfos& other) const {
            if (stages.size() != other.stages.size() || overrides.size() != other.overrides.size()) {
                return false;
            }
            for (size_t i = 0; i < stages.size(); ++i) {
                if (!(stages[i] == other.stages[i])) {
                    return false;
                }
            }

            for (size_t i = 0; i < overrides.size(); ++i) {
                bool nameMatches = overrides[i].bindingName.compare(other.overrides[i].bindingName) != 0;
                bool typeMatches = overrides[i].type != other.overrides[i].type;
                if (!nameMatches || !typeMatches) {
                    return false;
                }
            }

            return true;
        }

        size_t hash() const {
            size_t hash = 0;
            for (size_t i = 0; i < stages.size(); ++i) {
                // Again, not great, but will work somewhat
                hash ^= stages[i].hash();
            }
            for (size_t i = 0; i < overrides.size(); ++i) {
                // Again, not great, but will work somewhat
                hash ^= std::hash<std::string>{}(overrides[i].bindingName);
                hash ^= std::hash<size_t>{}(overrides[i].type);
            }

            return hash;
        }

        struct Hash {
            size_t operator() (const ShaderStageCreateInfos& createInfos) const {
                return createInfos.hash();
            }
        };
    };

    std::unordered_map<ShaderStageCreateInfos, ShaderPassInfo, ShaderStageCreateInfos::Hash> infoCache;
    std::unordered_map<ShaderPassInfo, ShaderPass, ShaderPassInfo::Hash> passCache;

    ShaderPassCache(VkDevice device, ShaderModuleCache& moduleCache, DescriptorSetLayoutCache& descriptorSetLayoutCache) : device(device), moduleCache(moduleCache), descriptorSetLayoutCache(descriptorSetLayoutCache) {}

    CacheLoadResult<ShaderPassInfo> loadInfo(ShaderStageCreateInfos stageInfos);
    //CacheLoadResult<ShaderPass> loadPass(ShaderStageCreateInfos stageInfos);
    CacheLoadResult<ShaderPass> loadPass(PassBuildingMaterials& passBuildingMaterials);
};
