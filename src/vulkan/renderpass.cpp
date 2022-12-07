#include "renderpass.h"

#include "engine.h"

uint32_t RenderAttachments::addTextureBacked(RenderAttachmentDesc&& description, Texture* texture) {
    attachments.push_back(RenderAttachment{ description, { texture } });
    return attachments.size() - 1;
}

uint32_t RenderAttachments::addTextureBacked(RenderAttachmentDesc&& description, std::vector<Texture*>& textures) {
    attachments.push_back(RenderAttachment{ description, textures });
    return attachments.size() - 1;
}

uint32_t RenderAttachments::add(RenderAttachmentDesc&& description, uint32_t textureBackedAttachment) {
    attachments.emplace_back(RenderAttachment{ description, attachments[textureBackedAttachment].textures });
    return attachments.size() - 1;
}

uint32_t RenderAttachments::addOutput(std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews, VkFormat swapchainImageFormat, VkExtent3D outputSize) {
    assert(outputAttachmentIndex == -1);
    RenderAttachmentDesc attachmentDesc(defaultColorAttachmentDescription().description, RenderAttachmentType::COLOR, true);

    RenderAttachment& outputAttachment = attachments.emplace_back(RenderAttachment{ attachmentDesc });
    outputAttachmentIndex = attachments.size() - 1;

    outputAttachment.description.description.format = swapchainImageFormat;

    // To be filled in by recreateOutputAttachment
    for (size_t i = 0; i < swapchainImages.size(); i++) {
        outputAttachment.textures.push_back(new Texture());
    }
    recreateOutputAttachment(swapchainImages, swapchainImageViews, swapchainImageFormat, outputSize);

    return outputAttachmentIndex;
}

void RenderAttachments::recreateOutputAttachment(std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews, VkFormat swapchainImageFormat, VkExtent3D outputSize) {
    assert(outputAttachmentIndex != -1);

    RenderAttachment& outputAttachment = attachments[outputAttachmentIndex];
    outputAttachment.description.description.format = swapchainImageFormat;

    for (size_t i = 0; i < swapchainImages.size(); i++) {
        // TODO: should use the texture cache
        outputAttachment.textures[i]->image = AllocatedImage { swapchainImages[i], VK_NULL_HANDLE, outputSize };
        outputAttachment.textures[i]->view = swapchainImageViews[i];
        outputAttachment.textures[i]->mipCount = 1;
    }
}

/*static*/ RenderAttachmentDesc RenderAttachments::defaultColorAttachmentDescription(bool clearOnLoad) {
    VkAttachmentDescription attachment = {};
    attachment.flags = 0;
    attachment.format = VK_FORMAT_B8G8R8A8_SRGB;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = clearOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    return RenderAttachmentDesc(attachment, RenderAttachmentType::COLOR);
}

/*static*/ RenderAttachmentDesc RenderAttachments::defaultDepthAttachmentDescription(bool clearOnLoad) {
    VkAttachmentDescription attachment = {};
    attachment.flags = 0;
    attachment.format = VK_FORMAT_D32_SFLOAT;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = clearOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    return RenderAttachmentDesc(attachment, RenderAttachmentType::DEPTH_STENCIL);
}

VkRenderPassBeginInfo RenderPass::beginRenderPassInfo(size_t framebufferIndex) {
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.pNext = nullptr;

    info.renderPass = renderPass;
    info.renderArea.offset.x = 0;
    info.renderArea.offset.y = 0;
    info.renderArea.extent = framebuffers[framebufferIndex].extent;
    info.framebuffer = framebuffers[framebufferIndex].framebuffer;

    info.clearValueCount = clears.size();
    info.pClearValues = clears.data();

    return info;
}

/*static*/ RenderPassBuilder RenderPassBuilder::begin(std::string name, bool presentationPass) {
    return RenderPassBuilder{ RenderPass(name, presentationPass) };
}

RenderPassBuilder& RenderPassBuilder::addAttachment(uint32_t attachmentIndex, std::string name, VkClearValue clear) {
    renderPass.attachmentIndices.push_back(attachmentIndex);
    renderPass.attachmentNames.push_back(name);
    renderPass.clears.push_back(clear);

    return *this;
}

RenderPassBuilder& RenderPassBuilder::addSubpass(VkPipelineBindPoint bindPoint, std::vector<SubpassAttachmentDesc> subpassAttachmentDescs, std::string name) {
    subpassDescs.push_back(RenderPassBuilder::SubpassDesc{ name, bindPoint, subpassAttachmentDescs });

    return *this;
}

RenderPass RenderPassBuilder::build(VkDevice device, RenderAttachments& attachments, uint32_t framebufferCount) {
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

    for (auto& subpassDesc : subpassDescs) {
        renderPass.subpasses.emplace_back(subpassDesc.name, subpassDesc.bindPoint);
    }

    // TODO: Should do some validation here to make sure that subpasses don't have repeating attachments, etc.

    std::vector<VkAttachmentDescription> renderPassAttachments;
    for (uint32_t attachmentIndex : renderPass.attachmentIndices) {
        RenderAttachmentDesc& attachmentDesc = attachments.attachments[attachmentIndex].description;
        renderPassAttachments.push_back(attachmentDesc.description);

        bool firstDependency = true;
        VkSubpassDependency lastSubpassDependency;

        // Usually not many subpasses and not many subpass attachments.
        // Could do better though
        for (size_t i = 0; i < subpassDescs.size(); i++) {
            Subpass& subpass = renderPass.subpasses[i];
            SubpassDesc& subpassDesc = subpassDescs[i];

            // Subpass shouldn't have more than one attachment referring to the same renderpass attachment
            SubpassAttachmentDesc* matchingDesc = nullptr;
            for (auto& subpassAttachmentDesc : subpassDesc.attachments) {
                if (subpassAttachmentDesc.attachment == attachmentIndex) {
                    matchingDesc = &subpassAttachmentDesc;
                    break;
                }
            }
            if (matchingDesc == nullptr) {
                continue;
            }
            matchingDesc->consumedDuringBuilding = true;
            
            SubpassAttachment attachment;
            attachment.attachmentRef = {};
            attachment.attachmentRef.attachment = renderPassAttachments.size() - 1;
            attachment.attachmentRef.layout = matchingDesc->layout;

            attachment.dependency = {};
            attachment.dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            attachment.dependency.dstSubpass = i;
            // TODO: might cause a stall -- wait until the pipeline hits this stage again
            // will do for the moment
            attachment.dependency.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            attachment.dependency.dstStageMask = matchingDesc->dstStageMask;
            attachment.dependency.srcAccessMask = VK_ACCESS_NONE;
            attachment.dependency.dstAccessMask = matchingDesc->dstAccessMask;

            if (!firstDependency) {
                attachment.dependency.srcSubpass = lastSubpassDependency.dstSubpass;
                attachment.dependency.srcStageMask = lastSubpassDependency.dstStageMask;
                attachment.dependency.srcAccessMask = lastSubpassDependency.dstAccessMask;
            }
    
            attachment.type = attachmentDesc.type;

            subpass.attachments.push_back(attachment);

            firstDependency = false;
            lastSubpassDependency = attachment.dependency;
        }
    }
    assert(renderPass.attachmentIndices.size() == renderPassAttachments.size());
    renderPassInfo.attachmentCount = renderPass.attachmentIndices.size();
    renderPassInfo.pAttachments = renderPassAttachments.data();

    // At this point all the subpasses have their attachmentRefs and dependencies filled in, unless
    // they reference some attachments not belonging to this renderpass.. FUCK TODO
    // Eeh do it later, can't be bothered now. Left "consumedDuringBuilding" for that.

    std::vector<VkSubpassDescription> subpasses;
    std::vector<VkSubpassDependency> subpassDependencies;

    std::vector<std::vector<VkAttachmentReference>> perSubpassColorAttachments(subpassDescs.size());
    std::vector<std::vector<VkAttachmentReference>> perSubpassDepthStencilAttachments(subpassDescs.size());
    for (size_t i = 0; i < subpassDescs.size(); i++) {
        Subpass& subpass = renderPass.subpasses[i];
        SubpassDesc& subpassDesc = subpassDescs[i];

        std::vector<VkAttachmentReference>& colorAttachments = perSubpassColorAttachments[i];
        std::vector<VkAttachmentReference>& depthStencilAttachments = perSubpassDepthStencilAttachments[i];
        for (auto& attachment : subpass.attachments) {
            subpassDependencies.push_back(attachment.dependency);

            // TODO: OKAY this is completely fucked. I'm missing input attachments... 
            switch (attachment.type) {
                case RenderAttachmentType::COLOR:
                    colorAttachments.push_back(attachment.attachmentRef);
                    break;
                case RenderAttachmentType::DEPTH_STENCIL:
                    depthStencilAttachments.push_back(attachment.attachmentRef);
                    break;
                default:
                    assert(false);
                    break;
            }
        }

        assert(colorAttachments.size() == depthStencilAttachments.size());

        VkSubpassDescription vkSubpassDesc = {};
        vkSubpassDesc.pipelineBindPoint = subpass.bindPoint;
        vkSubpassDesc.colorAttachmentCount = colorAttachments.size();
        vkSubpassDesc.pColorAttachments = colorAttachments.data();
        vkSubpassDesc.pDepthStencilAttachment = depthStencilAttachments.data();
        // TODO: ignores resolve and preserve attachments

        subpasses.push_back(vkSubpassDesc);
    }
    assert(subpassDescs.size() == subpasses.size());
    renderPassInfo.subpassCount = subpassDescs.size();
    renderPassInfo.pSubpasses = subpasses.data();
    renderPassInfo.dependencyCount = subpassDependencies.size();
    renderPassInfo.pDependencies = subpassDependencies.data();

    // Build renderpass
    VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass.renderPass));

    // Build framebuffer
    renderPass.framebuffers.reserve(framebufferCount);
    renderPass.rebuildFramebuffers(device, attachments);

    return renderPass;
}

void RenderPass::rebuildFramebuffers(VkDevice device, RenderAttachments& attachments) {
    // Depends on capacity and size. The capacity has to be set externally by the builder 
    // and controls how many framebuffers we have. Size specifies how many of those framebuffers
    // are alive and valid
    if (framebuffers.size() > 0) {
        for (auto& framebuffer : framebuffers) {
            vkDestroyFramebuffer(device, framebuffer.framebuffer, nullptr);
        }
        framebuffers.clear();
    }

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.pNext = nullptr;

    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = attachmentIndices.size();

    std::vector<VkImageView> framebufferAttachments(attachmentIndices.size());
    for (uint32_t i = 0; i < framebuffers.capacity(); i++) {
        Framebuffer& framebuffer = framebuffers.emplace_back();

        framebufferAttachments.clear();
        for (uint32_t attachmentIndex : attachmentIndices) {
            RenderAttachment& renderAttachment = attachments.attachments[attachmentIndex];
            uint32_t textureIndex = i % renderAttachment.textures.size();
            assert(renderAttachment.textures[textureIndex] != nullptr);

            framebufferAttachments.push_back(renderAttachment.textures[textureIndex]->view);

            if (renderAttachment.description.type == RenderAttachmentType::COLOR) {
                // For the time being we expect all of the color attachments to be of
                // the same size, so this should be safe
                framebufferInfo.width = renderAttachment.textures[textureIndex]->image.extent.width;
                framebufferInfo.height = renderAttachment.textures[textureIndex]->image.extent.height;
                framebufferInfo.layers = 1;

                framebuffer.extent.width = renderAttachment.textures[textureIndex]->image.extent.width;
                framebuffer.extent.height = renderAttachment.textures[textureIndex]->image.extent.height;
            }
        }
        framebufferInfo.pAttachments = framebufferAttachments.data();

        VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer.framebuffer));
    }
}
