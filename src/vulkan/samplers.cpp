#include "samplers.h"

#include "engine.h"

CacheLoadResult<VkSampler> SamplerCache::load(VkSamplerCreateInfo createInfo) {
    SamplerCreateInfo info = { createInfo };
    auto samplerFromCache = cache.find(info);
    if (samplerFromCache != cache.end()) {
        return CacheLoadResult<VkSampler>(true, &samplerFromCache->second);
    }

    VkSampler& sampler = cache[info];
    VK_CHECK(vkCreateSampler(backend.device, &info.info, nullptr, &sampler));

    return CacheLoadResult<VkSampler>(true, &sampler);
}
