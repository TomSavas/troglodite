#pragma once 

#include <functional>
#include <stdint.h>
#include <string>
#include <unordered_map>

#include <vulkan/vulkan.h>

#include "cache.h"

struct VulkanBackend;

struct SamplerCache {
    struct SamplerCreateInfo {
        VkSamplerCreateInfo info;

        size_t hash() const {
            // I wonder if this works...
            size_t h = 0;
            for (size_t i = 0; i < sizeof(VkSamplerCreateInfo); ++i) {
                h ^= std::hash<uint8_t>{}(((uint8_t*)&info)[i]);
            }

            return h;
        }

        bool operator==(const SamplerCreateInfo& other) const {
            return hash() == other.hash();
        }

        struct Hash {
            size_t operator() (const SamplerCreateInfo& createInfo) const {
                return createInfo.hash();
            }
        };
    };

    VulkanBackend& backend;
    std::unordered_map<SamplerCreateInfo, VkSampler, SamplerCreateInfo::Hash> cache;

    SamplerCache(VulkanBackend& backend) : backend(backend) {}

    CacheLoadResult<VkSampler> load(VkSamplerCreateInfo createInfo);
};
