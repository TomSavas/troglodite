#pragma once
#include <optional>
#include <stdint.h>
#include <vector>

#include <vulkan/vulkan.h>

struct DescriptorSetAllocator {
    VkDevice device;
    // TODO: temporary, needs to allocate on itself, or by relying on some descriptor pool
    VkDescriptorPool& pool;

    DescriptorSetAllocator(VkDevice device, VkDescriptorPool& pool) : device(device), pool(pool) {}
    void alloc(VkDescriptorSet* descriptorSets, size_t setCount, VkDescriptorSetLayout layout);
    //DescriptorSetAllocation alloc(VkDescriptorSet* descriptorSet, VkDescriptorSetLayout layout);

    //void resetPools();
};

struct DescriptorSetLayoutCache {
    VkDevice device;

    //struct DescriptorSetLayoutInfoForHashing {
    //    //VkDescriptorSetLayoutBinding* bindings;
    //    //size_t count;
    //    std::vector<VkDescriptorSetLayoutBinding> bindings;
    //    VkDescriptorSetLayoutCreateFlags flags;

    //    // TODO: give an allocator
    //    DescriptorSetLayoutInfoForHashing(VkDescriptorSetLayoutInfo& info);

    //    bool operator==(const DescriptorSetLayoutInfoForHashing& other) const;
    //    size_t hash() const;
    //}

    //struct DescriptorLayoutHash {
    //    size_t operator()(const DescriptorSetLayoutInfoForHashing& info) const {
    //        return info.hash();
    //    }
    //};
    //std::unordered_map<DescriptorSetLayoutInfoForHashing, VkDescriptorSetLayout, DescriptorLayoutHash> cache;

    // TODO: Contains all generated layouts. Need to move to a legit caching system
    std::vector<VkDescriptorSetLayout> layouts;

    DescriptorSetLayoutCache(VkDevice device) : device(device) {}
    std::optional<VkDescriptorSetLayout> getLayout(VkDescriptorSetLayoutCreateInfo* info);
    std::optional<VkDescriptorSetLayout> getLayout(VkDescriptorSetLayoutBinding* bindings, size_t bindingCount, VkDescriptorSetLayoutCreateFlags flags = (VkDescriptorSetLayoutCreateFlags)0);

    void deinit();
};

struct DescriptorSetBuilder {
    VkDevice device;
    DescriptorSetLayoutCache& layoutCache;
    DescriptorSetAllocator& allocator;

    size_t setCount;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<VkWriteDescriptorSet> writes; // set0Write0, set1Write0... setNWrite0, set0Write1, set1Write1... set1WriteN, etc

    static DescriptorSetBuilder begin(VkDevice device, DescriptorSetLayoutCache& layoutCache, DescriptorSetAllocator& allocator, size_t setCount = 1);

    // TODO: implement all the missing ones if need be
    // Reuses the same info for all sets
    DescriptorSetBuilder& bindDuplicateBuffer(VkDescriptorBufferInfo* info, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);
    DescriptorSetBuilder& bindDuplicateImage(VkDescriptorImageInfo* info, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);
    //DescriptorSetBuilder& bindDuplicateBufferView(VkBufferView* info, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);

    DescriptorSetBuilder& bindBuffers(VkDescriptorBufferInfo* info, size_t infoCount, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);
    DescriptorSetBuilder& bindImages(VkDescriptorImageInfo* info, size_t infoCount, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);
    //DescriptorSetBuilder& bindBufferViews(VkBufferView* info, size_t infoCount, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);
    //DescriptorSetBuilder& bindBufferAutoBinding(VkDescriptorBufferInfo* info, VkDescriptorType type, VkShaderStageFlags stageFlags);
    //DescriptorSetBuilder& bindImageAutoBinding(VkDescriptorImageInfo* info, VkDescriptorType type, VkShaderStageFlags stageFlags);
    //DescriptorSetBuilder& bindBufferViewAutoBinding(VkBufferView* info, VkDescriptorType type, VkShaderStageFlags stageFlags);

    // Per-set infos
    //DescriptorSetBuilder& bindBuffers();
    //DescriptorSetBuilder& bindImages();
    //DescriptorSetBuilder& bindViews();

    VkDescriptorSetLayout build(VkDescriptorSet* descriptorSets, VkDescriptorSetLayoutCreateFlagBits flags = (VkDescriptorSetLayoutCreateFlagBits) 0);

private:
    DescriptorSetBuilder(VkDevice device, DescriptorSetLayoutCache& layoutCache, DescriptorSetAllocator& allocator, size_t setCount) 
        : device(device), layoutCache(layoutCache), allocator(allocator), setCount(setCount) {}
};
