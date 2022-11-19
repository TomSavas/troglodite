#include <math.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "vulkan/descriptors.h"
#include "vulkan/engine.h"
#include "vulkan/mesh.h"
#include "vulkan/vk_shader.h"
#include "vulkan/vk_init_helpers.h"
#include "vulkan/pipeline_builder.h"
#include "vulkan/material.h"

#define LOG_CALL(code) do {                                      \
        std::cout << "Calling: " #code << std::endl; \
        code;                                                    \
    }                                                            \
    while(0)

void FunctionQueue::enqueue(std::function<void()>&& func) {
    functions.push_back(func);
}

void FunctionQueue::execute() {
    for (auto& func : functions) {
        func();
    }
    functions.clear();
}

/*static*/ void VulkanBackend::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    VulkanBackend* backend = reinterpret_cast<VulkanBackend*>(glfwGetWindowUserPointer(window));
    backend->swapchainRegenRequested = true;
}

void VulkanBackend::registerCallbacks() {
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, VulkanBackend::framebufferResizeCallback);
}

VkExtent3D glfwFramebufferSize(GLFWwindow* window) {
    int width;
    int height;  
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    return VkExtent3D {(uint32_t) width, (uint32_t) height, 1};
}

/*static*/ VulkanBackend VulkanBackend::init(GLFWwindow* window) {
    VulkanBackend backend;
    backend.window = window;
    backend.scene = new Scene();

    backend.viewportSize = glfwFramebufferSize(window);

    // TODO: bad place for this -- move
    backend.viewport.x = 0.f;
    backend.viewport.y = 0.f;
    backend.viewport.width = 1920.f;
    backend.viewport.height = 1080.f;
    backend.viewport.minDepth = 0.f;
    backend.viewport.maxDepth = 1.f;

    backend.scissor.offset = { 0, 0 };
    backend.scissor.extent = { 1920, 1080 };

    backend.initVulkan();
    backend.initSwapchain();
    backend.initCommandBuffers();
    backend.initDefaultRenderpass();
    backend.initFramebuffers();
    backend.initSyncStructs();
    backend.initDescriptors();
    backend.initPipelines();
    backend.initImgui();

    backend.frameNumber = 0;

    return backend;
}

void VulkanBackend::uploadMesh(Mesh& mesh) {
    const size_t bufferSize = mesh.vertices.size() * sizeof(Vertex);

    AllocatedBuffer cpuBuffer = createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    uploadData(mesh.vertices.data(), bufferSize, 0, cpuBuffer.allocation);

    mesh.vertexBuffer = createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    immediateBlockingSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy copy = {};       
        copy.srcOffset = 0;
        copy.dstOffset = 0;
        copy.size = bufferSize;
        vkCmdCopyBuffer(cmd, cpuBuffer.buffer, mesh.vertexBuffer.buffer, 1, &copy);
    });

    deinitQueue.enqueue([=]() {
        LOG_CALL(vmaDestroyBuffer(allocator, mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation));
    });
    vmaDestroyBuffer(allocator, cpuBuffer.buffer, cpuBuffer.allocation);
}

void VulkanBackend::uploadData(const void* data, size_t size, size_t offset, VmaAllocation allocation) {
    char* dataOnGPU;
    vmaMapMemory(allocator, allocation, (void**) &dataOnGPU);

    dataOnGPU += offset;

    memcpy(dataOnGPU, data, size);
    vmaUnmapMemory(allocator, allocation);
}

void VulkanBackend::deinit() {
    swapchainDeinitQueue.execute();
    deinitQueue.execute();

    vmaDestroyAllocator(allocator);

    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkb::destroy_debug_utils_messenger(instance, debugMessenger);
    vkDestroyInstance(instance, nullptr);
}

void VulkanBackend::initVulkan() {
    vkb::InstanceBuilder builder;
    vkbInstance = builder.set_app_name("Troglodite")
#ifdef DEBUG
        .request_validation_layers(true)
#else //DEBUG
        .request_validation_layers(false)
#endif //DEBUG
        .require_api_version(1, 1, 0)
        .use_default_debug_messenger()
        .build()
        .value();
    instance = vkbInstance.instance;
    debugMessenger = vkbInstance.debug_messenger;

    VkResult err = glfwCreateWindowSurface(instance, window, NULL, &surface);
    if (err != VK_SUCCESS) {
        printf("Failed creating surface\n");
    }

    vkb::PhysicalDeviceSelector selector { vkbInstance };
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 1)
        .set_surface(surface)
        .select()
        .value();
    vkb::DeviceBuilder deviceBuilder { physicalDevice };
    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParametersFeatures = {};
    shaderDrawParametersFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
    shaderDrawParametersFeatures.pNext = nullptr;
    shaderDrawParametersFeatures.shaderDrawParameters = VK_TRUE;
    deviceBuilder.add_pNext(&shaderDrawParametersFeatures);
    vkb::Device vkbDevice = deviceBuilder.build().value();
    gpu = physicalDevice.physical_device;
    device = vkbDevice.device;

    gpuProperties = vkbDevice.physical_device.properties;

    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = gpu;   
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    vmaCreateAllocator(&allocatorInfo, &allocator);
}

void VulkanBackend::initSwapchain() {
    viewportSize = glfwFramebufferSize(window);
    vkDeviceWaitIdle(device);

    vkb::SwapchainBuilder builder { gpu, device, surface };
    vkb::Swapchain vkbSwapchain = builder
        .use_default_format_selection()
        //.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
        //.set_desired_present_mode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
        //.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
        .set_desired_extent(viewportSize.width, viewportSize.height)
        .build()
        .value();

    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
    swapchainImageFormat = vkbSwapchain.image_format;

    swapchainDeinitQueue.enqueue([=]() {
        LOG_CALL(vkDestroySwapchainKHR(device, swapchain, nullptr));
    });

    VkExtent3D depthImageExtent = viewportSize;
    depthFormat = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo depthImageInfo = imageCreateInfo(depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);

    VmaAllocationCreateInfo depthImageAlloc = {};
    depthImageAlloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    depthImageAlloc.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vmaCreateImage(allocator, &depthImageInfo, &depthImageAlloc, &depthImage.image, &depthImage.allocation, nullptr);

    swapchainDeinitQueue.enqueue([=]() {
        LOG_CALL(vmaDestroyImage(allocator, depthImage.image, depthImage.allocation));
    });

    VkImageViewCreateInfo depthViewInfo = imageViewCreateInfo(depthFormat, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(device, &depthViewInfo, nullptr, &depthImageView));

    swapchainDeinitQueue.enqueue([=]() {
        LOG_CALL(vkDestroyImageView(device, depthImageView, nullptr));
    });
}

void VulkanBackend::initCommandBuffers() {
    VkCommandPoolCreateInfo commandPoolInfo = commandPoolCreateInfo(graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VkCommandBufferAllocateInfo cmdAllocInfo = commandBufferAllocateInfo(1, VK_COMMAND_BUFFER_LEVEL_PRIMARY, VK_NULL_HANDLE);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &inFlightFrames[i].cmdPool));
        cmdAllocInfo.commandPool = inFlightFrames[i].cmdPool;
        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &inFlightFrames[i].cmdBuffer));
    }

    deinitQueue.enqueue([=]() {
        LOG_CALL(
            for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroyCommandPool(device, inFlightFrames[i].cmdPool, nullptr);
            }
        );
    });

    VkCommandPoolCreateInfo uploadCmdPoolInfo = commandPoolCreateInfo(graphicsQueueFamily, 0);
    VK_CHECK(vkCreateCommandPool(device, &uploadCmdPoolInfo, nullptr, &uploadCtx.cmdPool));

    VkCommandBufferAllocateInfo uploadCmdAllocInfo = commandBufferAllocateInfo(1, VK_COMMAND_BUFFER_LEVEL_PRIMARY, uploadCtx.cmdPool);
    VK_CHECK(vkAllocateCommandBuffers(device, &uploadCmdAllocInfo, &uploadCtx.cmdBuffer));

    deinitQueue.enqueue([=]() {
        LOG_CALL(vkDestroyCommandPool(device, uploadCtx.cmdPool, nullptr));
    });
}

void VulkanBackend::initDefaultRenderpass() {
    // the renderpass will use this color attachment.
    VkAttachmentDescription colorAttachment = {};
    //the attachment will have the format needed by the swapchain
    colorAttachment.format = swapchainImageFormat;
    //1 sample, we won't be doing MSAA
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    // we Clear when this attachment is loaded
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // we keep the attachment stored when the renderpass ends
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    //we don't care about stencil
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    //we don't know or care about the starting layout of the attachment
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    //after the renderpass ends, the image has to be on a layout ready for display
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    //attachment number will index into the pAttachments array in the parent renderpass itself
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.flags = 0;
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = {};
    //attachment number will index into the pAttachments array in the parent renderpass itself
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    //we are going to create 1 subpass, which is the minimum you can do
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency colorDependency = {};
    colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    colorDependency.dstSubpass = 0;
    colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.srcAccessMask = 0;
    colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency depthDependency = {};
    depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    depthDependency.dstSubpass = 0;
    depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.srcAccessMask = 0;
    depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency subpassDependencies[] = { colorDependency, depthDependency };

    VkAttachmentDescription attachments[] = { colorAttachment, depthAttachment };

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

    //connect the color attachment to the info
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments = &attachments[0];
    //connect the subpass to the info
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = &subpassDependencies[0];

    VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &defaultRenderpass));
    deinitQueue.enqueue([=]() {
        LOG_CALL(vkDestroyRenderPass(device, defaultRenderpass, nullptr));
    });
}

void VulkanBackend::initFramebuffers() {
    //create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
    VkFramebufferCreateInfo fbInfo = {};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.pNext = nullptr;

    fbInfo.renderPass = defaultRenderpass;
    fbInfo.attachmentCount = 1;
    fbInfo.width = viewportSize.width;
    fbInfo.height = viewportSize.height;
    fbInfo.layers = 1;

    //grab how many images we have in the swapchain
    const uint32_t swapchainImageCount = swapchainImages.size();
    framebuffers = std::vector<VkFramebuffer>(swapchainImageCount);

    //create framebuffers for each of the swapchain image views
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        VkImageView attachments[] = { swapchainImageViews[i], depthImageView };

        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments = attachments;

        VK_CHECK(vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffers[i]));
    }

    swapchainDeinitQueue.enqueue([=]() {
        LOG_CALL(
            for (size_t i = 0; i < swapchainImageViews.size(); i++) {
                vkDestroyFramebuffer(device, framebuffers[i], nullptr);
                // TODO: maybe where it's created?
                vkDestroyImageView(device, swapchainImageViews[i], nullptr);
            }
        );
    });
}

void VulkanBackend::initSyncStructs() {
    // We want to create the fence with the Create Signaled flag, so we can wait on it before using it on a GPU command (for the first frame)
    VkFenceCreateInfo frameRenderFenceCreateInfo = fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    // For the semaphores we don't need any flags
    VkSemaphoreCreateInfo frameSemCreateInfo = semaphoreCreateInfo(0);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateFence(device, &frameRenderFenceCreateInfo, nullptr, &inFlightFrames[i].renderFence));
        VK_CHECK(vkCreateSemaphore(device, &frameSemCreateInfo, nullptr, &inFlightFrames[i].presentSem));
        VK_CHECK(vkCreateSemaphore(device, &frameSemCreateInfo, nullptr, &inFlightFrames[i].renderSem));
    }
    deinitQueue.enqueue([=](){
        LOG_CALL(
            for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroyFence(device, inFlightFrames[i].renderFence, nullptr);
                vkDestroySemaphore(device, inFlightFrames[i].presentSem, nullptr);
                vkDestroySemaphore(device, inFlightFrames[i].renderSem, nullptr);
            }
        );
    });

    VkFenceCreateInfo uploadFenceCreateInfo = fenceCreateInfo(0);
    VK_CHECK(vkCreateFence(device, &uploadFenceCreateInfo, nullptr, &uploadCtx.uploadFence));
    deinitQueue.enqueue([=](){
        LOG_CALL(vkDestroyFence(device, uploadCtx.uploadFence, nullptr));
    });
}

void VulkanBackend::draw() {
    VK_CHECK(vkWaitForFences(device, 1, &currentFrame().renderFence, true, 1000000000));

    // Request image from the swapchain, one second timeout
    uint32_t swapchainImageIndex;
    VkResult nextImageResult = vkAcquireNextImageKHR(device, swapchain, 1000000000, currentFrame().presentSem, nullptr, &swapchainImageIndex);

    bool swapchainRegenNeeded = nextImageResult == VK_ERROR_OUT_OF_DATE_KHR 
        || nextImageResult == VK_SUBOPTIMAL_KHR
        || swapchainRegenRequested;
    if (swapchainRegenNeeded) {
        swapchainDeinitQueue.execute();
        initSwapchain();
        initFramebuffers();

        swapchainRegenRequested = false;

        // Re-request the image from recreated swapchain and continue as usual
        VK_CHECK(vkWaitForFences(device, 1, &currentFrame().renderFence, true, 1000000000));
        nextImageResult = vkAcquireNextImageKHR(device, swapchain, 1000000000, currentFrame().presentSem, nullptr, &swapchainImageIndex);
    }
    VK_CHECK(nextImageResult);

    VK_CHECK(vkResetFences(device, 1, &currentFrame().renderFence));

    //now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(currentFrame().cmdBuffer, 0));

    //naming it cmd for shorter writing
    VkCommandBuffer cmd = currentFrame().cmdBuffer;

    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    //make a clear-color from frame number. This will flash with a 120*pi frame period.
    VkClearValue colorClear;
    float flash = abs(sin(float(frameNumber) / 120.f));
    colorClear.color = { { 0.0f, 0.0f, flash, 1.0f } };

    VkClearValue depthClear;
    depthClear.depthStencil.depth = 1.f;

    //start the main renderpass.
    //We will use the clear color from above, and the framebuffer of the index the swapchain gave us
    VkRenderPassBeginInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.pNext = nullptr;

    rpInfo.renderPass = defaultRenderpass;
    rpInfo.renderArea.offset.x = 0;
    rpInfo.renderArea.offset.y = 0;
    rpInfo.renderArea.extent = { viewportSize.width, viewportSize.height };
    rpInfo.framebuffer = framebuffers[swapchainImageIndex];

    viewport.width = viewportSize.width;
    viewport.height = viewportSize.height;
    scissor.extent = { viewportSize.width, viewportSize.height };

    //connect clear values
    VkClearValue clearValues[] = { colorClear, depthClear };
    rpInfo.clearValueCount = 2;
    rpInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    scene->draw(cmd, currentFrame());
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd); // TODO: move out to UI pass, or something above that
    vkCmdEndRenderPass(cmd);
    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    //prepare the submission to the queue.
    //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    //we will signal the _renderSemaphore, to signal that rendering has finished

    VkSubmitInfo submit = submitInfo(&cmd);
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit.pWaitDstStageMask = &waitStage;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &currentFrame().presentSem;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &currentFrame().renderSem;

    // submit command buffer to the queue and execute it.
    // renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submit, currentFrame().renderFence));

    // this will put the image we just rendered into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as it's necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;

    presentInfo.pSwapchains = &swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &currentFrame().renderSem;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(graphicsQueue, &presentInfo));

    frameNumber++;
    //printf("frame: %d\n", frameNumber);
}

void VulkanBackend::immediateBlockingSubmit(std::function<void(VkCommandBuffer)>&& func) {
    VkCommandBufferBeginInfo beginInfo = commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(uploadCtx.cmdBuffer, &beginInfo));
    func(uploadCtx.cmdBuffer);
    VK_CHECK(vkEndCommandBuffer(uploadCtx.cmdBuffer));

    VkSubmitInfo submit = submitInfo(&uploadCtx.cmdBuffer);
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submit, uploadCtx.uploadFence));

    vkWaitForFences(device, 1, &uploadCtx.uploadFence, true, 1000000000);
    vkResetFences(device, 1, &uploadCtx.uploadFence);

    vkResetCommandPool(device, uploadCtx.cmdPool, 0);
}

void VulkanBackend::initDescriptors() {
    VkDescriptorPoolSize descriptorPoolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    };

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.pNext = nullptr;
    poolCreateInfo.flags = 0;
    poolCreateInfo.maxSets = 1000;
    poolCreateInfo.poolSizeCount = sizeof(descriptorPoolSizes) / sizeof(VkDescriptorPoolSize);
    poolCreateInfo.pPoolSizes = descriptorPoolSizes;

    vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool);
    deinitQueue.enqueue([=]() {
        LOG_CALL(vkDestroyDescriptorPool(device, descriptorPool, nullptr));
    });

    descriptorSetLayoutCache = new DescriptorSetLayoutCache(device);
    descriptorSetAllocator = new DescriptorSetAllocator(device, descriptorPool);

    const int MAX_OBJECTS = 10000;
    // Create buffers
    const size_t sceneParamsBuffersSize = MAX_FRAMES_IN_FLIGHT * padUniformBufferSize(sizeof(GPUSceneData));
    sceneParamsBuffers = createBuffer(sceneParamsBuffersSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    VkDescriptorBufferInfo sceneParamsDescriptorInfo = descriptorBufferInfo(sceneParamsBuffers.buffer, 0, sizeof(GPUSceneData));
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        inFlightFrames[i].cameraUBO = createBuffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        inFlightFrames[i].objectDataBuffer = createBuffer(sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    // Generate infos
    VkDescriptorBufferInfo cameraDescriptorInfos[MAX_FRAMES_IN_FLIGHT];
    VkDescriptorBufferInfo objectDescriptorInfos[MAX_FRAMES_IN_FLIGHT];
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        cameraDescriptorInfos[i] = descriptorBufferInfo(inFlightFrames[i].cameraUBO.buffer, 0, sizeof(GPUCameraData));
        objectDescriptorInfos[i] = descriptorBufferInfo(inFlightFrames[i].objectDataBuffer.buffer, 0, sizeof(GPUObjectData) * MAX_OBJECTS);
    }

    // Build descriptor sets
    // TODO: congregate into a single build
    VkDescriptorSet cameraDescriptorSets[MAX_FRAMES_IN_FLIGHT];
    globalDescriptorSetLayout = DescriptorSetBuilder::begin(device, *descriptorSetLayoutCache, *descriptorSetAllocator, MAX_FRAMES_IN_FLIGHT)
        .bindBuffers(cameraDescriptorInfos, MAX_FRAMES_IN_FLIGHT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
        .bindDuplicateBuffer(&sceneParamsDescriptorInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
        .build(cameraDescriptorSets);

    VkDescriptorSet objectDescriptorSets[MAX_FRAMES_IN_FLIGHT];
    objectDescriptorSetLayout = DescriptorSetBuilder::begin(device, *descriptorSetLayoutCache, *descriptorSetAllocator, MAX_FRAMES_IN_FLIGHT)
        .bindBuffers(objectDescriptorInfos, MAX_FRAMES_IN_FLIGHT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
        .build(objectDescriptorSets);

    // TODO: temporarily write back descriptor sets until frame data is redone to SOA
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        inFlightFrames[i].globalDescriptor = cameraDescriptorSets[i];
        inFlightFrames[i].objectDescriptor = objectDescriptorSets[i]; 
    }

    deinitQueue.enqueue([=]() {
        LOG_CALL(
            for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vmaDestroyBuffer(allocator, inFlightFrames[i].objectDataBuffer.buffer, inFlightFrames[i].objectDataBuffer.allocation); 
                vmaDestroyBuffer(allocator, inFlightFrames[i].cameraUBO.buffer, inFlightFrames[i].cameraUBO.allocation); 
            }
            vmaDestroyBuffer(allocator, sceneParamsBuffers.buffer, sceneParamsBuffers.allocation); 
        );
        LOG_CALL(descriptorSetLayoutCache->deinit());
    });
}

void VulkanBackend::initPipelines() {
    textureCache = new TextureCache(*this);
    shaderModuleCache = new ShaderModuleCache(device);
    shaderPassCache = new ShaderPassCache(device, *shaderModuleCache, *descriptorSetLayoutCache);
    materials = new Materials(*shaderPassCache);

    PipelineBuilder forwardPipelineBuilder;
    VertexInputDescription vertexDescription = Vertex::getVertexDescription();
    forwardPipelineBuilder.vertexInputInfo = vertexInputStateCreateInfo(vertexDescription);
    forwardPipelineBuilder.inputAssembly = inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    forwardPipelineBuilder.rasterizer = rasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
    forwardPipelineBuilder.multisampling = multisampleStateCreateInfo();
    forwardPipelineBuilder.colorBlendAttachment = colorBlendAttachmentState();
    forwardPipelineBuilder.depthStencil = depthStencilCreateInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

    // TODO: add error material

    // TODO: pass these into shaders as specialization constants
    const uint8_t SCENE_DESCRIPTOR_SET_INDEX = 0;
    const uint8_t OBJECT_DATA_DESCRIPTOR_SET_INDEX = 1;

    ShaderPassCache::ShaderStageCreateInfos::DescriptorTypeOverride sceneParamsDescriptorOverride {
        "sceneParams", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC 
    };

    //CacheLoadResult<ShaderPassInfo> csmPassInfoResult = shaderPassCache->loadInfo(ShaderPassCache::ShaderStageCreateInfos(
    //    {
    //        ShaderPassCache::ShaderStageCreateInfo(SHADER_PATH("model_transform.vert.glsl"), VK_SHADER_STAGE_VERTEX_BIT),
    //        ShaderPassCache::ShaderStageCreateInfo(SHADER_PATH("csm_gen_lightspace_transforms.geom.glsl"), VK_SHADER_STAGE_GEOMETRY_BIT),
    //        ShaderPassCache::ShaderStageCreateInfo(SHADER_PATH("empty.frag.glsl"), VK_SHADER_STAGE_FRAGMENT_BIT),
    //    },
    //    {
    //        sceneParamsDescriptorOverride,
    //    }));
    CacheLoadResult<ShaderPassInfo> forwardLitPassInfoResult = shaderPassCache->loadInfo(ShaderPassCache::ShaderStageCreateInfos(
        {
            ShaderPassCache::ShaderStageCreateInfo(SHADER_PATH("mvp_transform.vert.glsl"), VK_SHADER_STAGE_VERTEX_BIT),
            ShaderPassCache::ShaderStageCreateInfo(SHADER_PATH("forward_unlit.frag.glsl"), VK_SHADER_STAGE_FRAGMENT_BIT),
            //ShaderPassCache::ShaderStageCreateInfo(SHADER_PATH("forward_lit.frag.glsl"), VK_SHADER_STAGE_FRAGMENT_BIT),
        },
        {
            sceneParamsDescriptorOverride,
        }));


    if (/*csmPassInfoResult.success &&*/ forwardLitPassInfoResult.success) {
        std::vector<uint8_t> globalPerInFlightFramesDescriptorSets { SCENE_DESCRIPTOR_SET_INDEX, OBJECT_DATA_DESCRIPTOR_SET_INDEX };

        materials->enqueue(std::move(MaterialBuilder::begin(Materials::DEFAULT_LIT)
            //.beginPass(PassType::DIRECTIONAL_SHADOW, csmPassInfoResult.pass, shadowPipelineBuilder, shadowRenderpass)
            //    .perInFlightFramesDescriptors(MAX_FRAMES_IN_FLIGHT, globalPerInFlightFramesDescriptorSets)
            //.endPass()
            .beginPass(PassType::FORWARD_OPAQUE, forwardLitPassInfoResult.data, forwardPipelineBuilder, /*forwardRenderpass*/defaultRenderpass, viewport, scissor)
                //.perInFlightFramesDescriptorSets(MAX_FRAMES_IN_FLIGHT, globalPerInFlightFramesDescriptorSets)
            .endPass()
        ));
    } else {
        printf("Failed creating material \"%s\" - failed retrieving shaders\n", Materials::DEFAULT_LIT);
    }

    // TODO: redo materials internals to support SoA for usage with vkCreateGraphicsPipelines
    materials->buildQueued();

    VkSamplerCreateInfo samplerInfo = samplerCreateInfo(VK_FILTER_NEAREST);
    CacheLoadResult<SampledTexture> defaultAlbedo = textureCache->load("/home/savas/Projects/ignoramus_renderer/assets/textures/default.jpeg", samplerInfo);
    if (defaultAlbedo.success) {
        // TODO: mem leak
        materials->materials[Materials::DEFAULT_LIT].defaultTextures["albedo"] = new SampledTexture();
        *materials->materials[Materials::DEFAULT_LIT].defaultTextures["albedo"] = *defaultAlbedo.data;
    } else {
        printf("failed creating default texture\n");
        assert(false);
    }
}

void VulkanBackend::initImgui() {
    // Create descriptor pool for ImGui
    // the size of the pool is very oversize, but it's copied from imgui demo itself.
    VkDescriptorPoolSize poolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.pNext = nullptr;
    poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolCreateInfo.maxSets = 1000;
    poolCreateInfo.poolSizeCount = std::size(poolSizes);
    poolCreateInfo.pPoolSizes = poolSizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &imguiPool));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = gpu;
    initInfo.Device = device;
    initInfo.Queue = graphicsQueue;
    initInfo.DescriptorPool = imguiPool;
    initInfo.MinImageCount = 3;
    initInfo.ImageCount = 3;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo, defaultRenderpass);

    immediateBlockingSubmit([&](VkCommandBuffer cmd) {
        ImGui_ImplVulkan_CreateFontsTexture(cmd);
    });
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    deinitQueue.enqueue([=]() {
        vkDestroyDescriptorPool(device, imguiPool, nullptr);

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    });
}

FrameData& VulkanBackend::currentFrame() {
    return inFlightFrames[frameNumber % MAX_FRAMES_IN_FLIGHT];
}

AllocatedBuffer VulkanBackend::createBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
    VkBufferCreateInfo bufferInfo = bufferCreateInfo(size, usage);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = memoryUsage;

    AllocatedBuffer buffer;
    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer.buffer, &buffer.allocation, nullptr));

    return buffer;
}

size_t VulkanBackend::padUniformBufferSize(size_t requestedSize) {
    size_t minUboAlignment = gpuProperties.limits.minUniformBufferOffsetAlignment;
    size_t size = requestedSize;
    if (minUboAlignment > 0) {
        size = (size + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }

    return size;
}
