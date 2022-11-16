#include <assert.h>
#include <alloca.h>
#include <stdio.h>

#include "engine.h"
#include "descriptors.h"
#include "vk_init_helpers.h"

void DescriptorSetAllocator::alloc(VkDescriptorSet* descriptorSets, size_t setCount, VkDescriptorSetLayout layout) {
    assert(setCount > 0);

    // TODO: change alloca with allocation from arena?
    VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*) alloca(setCount * sizeof(VkDescriptorSetLayout));
    for (size_t i = 0; i < setCount; ++i) {
        layouts[i] = layout;
    }

    VkDescriptorSetAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.pNext = nullptr;

    info.descriptorPool = pool;
    info.descriptorSetCount = setCount;
    info.pSetLayouts = layouts;

    VK_CHECK(vkAllocateDescriptorSets(device, &info, descriptorSets));
}

std::optional<VkDescriptorSetLayout> DescriptorSetLayoutCache::getLayout(VkDescriptorSetLayoutCreateInfo* info) {
    // TODO: actually cache it, fake it for the time being
    VkDescriptorSetLayout& layout = layouts.emplace_back();
    VK_CHECK(vkCreateDescriptorSetLayout(device, info, nullptr, &layout));
    return std::optional(layout);
}

std::optional<VkDescriptorSetLayout> DescriptorSetLayoutCache::getLayout(VkDescriptorSetLayoutBinding* bindings, size_t bindingCount, VkDescriptorSetLayoutCreateFlags flags) {
    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext = nullptr;

    info.flags = flags;
    info.bindingCount = bindingCount;
    info.pBindings = bindings;

    return getLayout(&info);
}

void DescriptorSetLayoutCache::deinit() {
    for (auto layout : layouts) {
        vkDestroyDescriptorSetLayout(device, layout, nullptr);
    }
}

/*static*/ DescriptorSetBuilder DescriptorSetBuilder::begin(VkDevice device, DescriptorSetLayoutCache& layoutCache, DescriptorSetAllocator& allocator, size_t setCount) {
    DescriptorSetBuilder builder(device, layoutCache, allocator, setCount);

    // TODO: allocate arrays for bindings and writes from an arena via a hint of some sort
    
    return builder;
}

DescriptorSetBuilder& DescriptorSetBuilder::bindDuplicateBuffer(VkDescriptorBufferInfo* info, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding) {
    assert(binding == bindings.size());

    bindings.push_back(descriptorSetLayoutBinding(type, stageFlags, binding));
    for (size_t i = 0; i < setCount; ++i) {
        writes.push_back(writeDescriptorBuffer(type, nullptr, info, binding)); // we will back-fill the dst set on build
    }

    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::bindDuplicateImage(VkDescriptorImageInfo* info, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding) {
    assert(binding == bindings.size());

    bindings.push_back(descriptorSetLayoutBinding(type, stageFlags, binding));
    for (size_t i = 0; i < setCount; ++i) {
        writes.push_back(writeDescriptorImage(type, nullptr, info, binding)); // we will back-fill the dst set on build
    }

    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::bindBuffers(VkDescriptorBufferInfo* info, size_t infoCount, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding) {
    // Otherwise what do we bind for remaining sets?
    assert(infoCount == setCount);
    assert(binding == bindings.size());

    bindings.push_back(descriptorSetLayoutBinding(type, stageFlags, binding));
    for (size_t i = 0; i < infoCount; ++i) {
        writes.push_back(writeDescriptorBuffer(type, nullptr, &info[i], binding)); // we will back-fill the dst set on build
    }

    return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::bindImages(VkDescriptorImageInfo* info, size_t infoCount, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding) {
    // Otherwise what do we bind for remaining sets?
    assert(infoCount == setCount);
    assert(binding == bindings.size());

    bindings.push_back(descriptorSetLayoutBinding(type, stageFlags, binding));
    for (size_t i = 0; i < infoCount; ++i) {
        writes.push_back(writeDescriptorImage(type, nullptr, &info[i], binding)); // we will back-fill the dst set on build
    }

    return *this;
}

VkDescriptorSetLayout DescriptorSetBuilder::build(VkDescriptorSet* descriptorSets, VkDescriptorSetLayoutCreateFlagBits flags) {
    // Check for success
    std::optional<VkDescriptorSetLayout> layout = layoutCache.getLayout(bindings.data(), bindings.size(), flags);
    if (!layout) {
        // TODO: fix
        printf("Failed to fetch a layout\n");
        return layout.value();
    }

    // TODO: error handle
    if (setCount > 0) {
        allocator.alloc(descriptorSets, setCount, layout.value());
    }

    // Backfill with descriptor dstSets as we skipped that in binds
    for (size_t i = 0; i < writes.size(); ++i) {
        writes[i].dstSet = descriptorSets[i % setCount];
    }
    if (writes.size() > 0) {
        vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
    }

    return layout.value();
}
