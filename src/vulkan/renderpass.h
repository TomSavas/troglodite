#pragma once 

#include <string>
#include <vector>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "vulkan/texture.h"

enum class RenderAttachmentType {
    COLOR = 0,
    DEPTH_STENCIL,
};

struct RenderAttachmentDesc {
    VkAttachmentDescription description;
    RenderAttachmentType type;

    const bool outputAttachment;

    RenderAttachmentDesc(VkAttachmentDescription desc, RenderAttachmentType type, bool outputAttachment = false) 
        : description(desc), type(type), outputAttachment(outputAttachment) {}
};

struct RenderAttachment {
    RenderAttachmentDesc description;
    // Usually has one, unless a renderpass that uses this requires per-in-flight-frame attachments
    std::vector<Texture*> textures;
};

struct RenderAttachments {
    std::vector<RenderAttachment> attachments;
    
    int64_t outputAttachmentIndex;

    RenderAttachments(VulkanBackend& backend) : outputAttachmentIndex(-1) {}

    uint32_t addTextureBacked(RenderAttachmentDesc&& description, Texture* texture);
    uint32_t addTextureBacked(RenderAttachmentDesc&& description, std::vector<Texture*>& textures);
    uint32_t add(RenderAttachmentDesc&& description, uint32_t textureBackedAttachment);
    uint32_t addOutput(std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews, VkFormat swapchainImageFormat, VkExtent3D outputSize);

    void recreateOutputAttachment(std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews, VkFormat swapchainImageFormat, VkExtent3D outputSize);

    static RenderAttachmentDesc defaultColorAttachmentDescription(bool clearOnLoad = true, bool isOutput = false);
    static RenderAttachmentDesc defaultDepthAttachmentDescription(bool clearOnLoad = true);
};

// TODO: seriously half of this shit is just debug info. Should remove nearly all of it,
struct SubpassAttachment {
    VkAttachmentReference attachmentRef;
    VkSubpassDependency dependency;
    // TODO: remove, not really needed here
    RenderAttachmentType type;
};

struct Subpass {
    std::string name;
    VkPipelineBindPoint bindPoint;
    std::vector<SubpassAttachment> attachments;

    Subpass(std::string name, VkPipelineBindPoint bindPoint, std::vector<SubpassAttachment> attachments = {})
        : name(name), bindPoint(bindPoint), attachments(attachments) {}
};

struct Framebuffer {
    VkExtent2D extent;
    VkFramebuffer framebuffer;
};

struct RenderPass {
    std::string name;
    std::vector<Subpass> subpasses;

    // TODO: move to builder, no need here
    std::vector<uint32_t> attachmentIndices;
    std::vector<std::string> attachmentNames;

    // If these are used depends on the attachment description
    std::vector<VkClearValue> clears;

    VkRenderPass renderPass;

    bool presentationPass;
    // Usually one, unless this renderpass requires per-in-flight-frames framebuffers
    std::vector<Framebuffer> framebuffers;

    RenderPass(std::string name, bool presentationPass) : name(name), presentationPass(presentationPass) {}
    VkRenderPassBeginInfo beginRenderPassInfo(size_t framebufferIndex);
    void rebuildFramebuffers(VkDevice device, RenderAttachments& attachments);
};

struct RenderPassBuilder {
    struct SubpassAttachmentDesc {
        uint32_t attachment;
        VkImageLayout layout;
        VkPipelineStageFlags dstStageMask;
        VkAccessFlags dstAccessMask;
        bool graphicPipelineAttachment;

        bool consumedDuringBuilding;

        SubpassAttachmentDesc(uint32_t attachment, VkImageLayout layout, VkPipelineStageFlags dstStageMask,
            VkAccessFlags dstAccessMask, bool graphicPipelineAttachment = true)
            : attachment(attachment), layout(layout), dstStageMask(dstStageMask), dstAccessMask(dstAccessMask),
            graphicPipelineAttachment(graphicPipelineAttachment), consumedDuringBuilding(false) {}
    };
    struct SubpassDesc {
        std::string name;
        VkPipelineBindPoint bindPoint;
        std::vector<SubpassAttachmentDesc> attachments;
    };

    RenderPass renderPass;
    std::vector<SubpassDesc> subpassDescs;

    static RenderPassBuilder begin(std::string name, bool presentationPass = false);

    RenderPassBuilder& addAttachment(uint32_t attachmentIndex, std::string name, VkClearValue clear = {});
    RenderPassBuilder& addSubpass(VkPipelineBindPoint bindPoint, std::vector<SubpassAttachmentDesc> subpassAttachmentDescs, std::string name = "default subpass");
    RenderPass build(VkDevice device, RenderAttachments& attachments, uint32_t framebufferCount = 1);
};
