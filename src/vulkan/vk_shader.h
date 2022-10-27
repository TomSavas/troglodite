#pragma once

#include "vulkan/vulkan.h"

struct ShaderModule {
    const char* filepath;
    uint32_t* spirvCode;

    VkShaderModule* module;  
};

struct ShaderStage {
    const char* name;
    ShaderModule* module;  
    VkShaderStageFlagsBits flags;
};

struct ShaderBundleInfo {

    //std::vector<> descriptorSetLayouts;
    std::vector<DescriptorBinding> bindings;

    std::vector<ShaderStage> stages;
    VkDescriptorSetLayout descriptorSetLayout;
};

struct ShaderBundle {
    ShaderBundleInfo info;
    VkPipeline pipeline;
    VkPipelineLayout layout;

    //std::vector<> bindings;
}

bool loadShaderModule(const char* path, VkDevice device, VkShaderModule* outShaderModule);
