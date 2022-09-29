#include <math.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "vulkan/engine.h"
#include "vulkan/mesh.h"
#include "vulkan/vk_shader.h"

#define LOG_CALL(code) do {                                      \
        std::cout << "Calling deinitQueue: " #code << std::endl; \
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
}

VkPipeline VulkanPipelineBuilder::build(VkDevice device, VkRenderPass pass) {
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;

    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;

    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;

    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending; // TODO change
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = pass;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline pipeline = {};
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        printf("Failed creating graphics pipeline\n");
        return VK_NULL_HANDLE;
    }

    return pipeline;
}

/*static*/ VulkanBackend VulkanBackend::init(GLFWwindow* window) {
    VulkanBackend backend;
    
    backend.initVulkan(window);
    backend.initSwapchain();
    backend.initCommandBuffers();
    backend.initDefaultRenderpass();
    backend.initFramebuffers();
    backend.initSyncStructs();
    backend.initPipelines();

    backend.loadMeshes();

    return backend;
}

void VulkanBackend::loadMeshes() {
    // TODO: get rid of copying
    Mesh suzanneMesh;
    suzanneMesh.loadFromObj("/home/savas/Projects/ignoramus_renderer/assets/suzanne.obj");
    scene.meshes["suzanne"] = suzanneMesh;
    uploadMesh(scene.meshes["suzanne"]);

    Mesh triangleMesh;
    triangleMesh.vertices.resize(3);
    triangleMesh.vertices[0].position = { 1.f, 1.f, 0.0f };
    triangleMesh.vertices[1].position = {-1.f, 1.f, 0.0f };
    triangleMesh.vertices[2].position = { 0.f,-1.f, 0.0f };
    triangleMesh.vertices[0].color = { 0.f, 1.f, 0.0f }; //pure green
    triangleMesh.vertices[1].color = { 0.f, 1.f, 0.0f }; //pure green
    triangleMesh.vertices[2].color = { 0.f, 1.f, 0.0f }; //pure green
    scene.meshes["triangle"] = triangleMesh;
    uploadMesh(scene.meshes["triangle"]);
}

void VulkanBackend::uploadMesh(Mesh& mesh) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = mesh.vertices.size() * sizeof(Vertex);
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                &mesh.vertexBuffer.buffer, &mesh.vertexBuffer.allocation,
                nullptr));
    
    // TEMP: assets should have a better way of de-initing
    deinitQueue.enqueue([=]() {
        LOG_CALL(vmaDestroyBuffer(allocator, mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation));
    });
    
    void* vertexData;
    vmaMapMemory(allocator, mesh.vertexBuffer.allocation, &vertexData);
    memcpy(vertexData, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
    vmaUnmapMemory(allocator, mesh.vertexBuffer.allocation);
}

void VulkanBackend::deinit() {
    deinitQueue.execute();

    vmaDestroyAllocator(allocator);

    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkb::destroy_debug_utils_messenger(instance, debugMessenger);
    vkDestroyInstance(instance, nullptr);
}

void VulkanBackend::initVulkan(GLFWwindow* window) {
    vkb::InstanceBuilder builder;
    vkbInstance = builder.set_app_name("Troglodite")
        .request_validation_layers(true)
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
    vkb::Device vkbDevice = deviceBuilder.build().value();
    gpu = physicalDevice.physical_device;
    device = vkbDevice.device;

    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = gpu;   
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    vmaCreateAllocator(&allocatorInfo, &allocator);
}

void VulkanBackend::initSwapchain() {
    vkb::SwapchainBuilder builder { gpu, device, surface };
    vkb::Swapchain vkbSwapchain = builder
        .use_default_format_selection()
        //.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
        //.set_desired_present_mode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
        //.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
        .set_desired_extent(640, 480)
        .build()
        .value();

    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
    swapchainImageFormat = vkbSwapchain.image_format;

    deinitQueue.enqueue([=]() {
        LOG_CALL(vkDestroySwapchainKHR(device, swapchain, nullptr));
    });

    VkExtent3D depthImageExtent = { 640, 480, 1 };
    depthFormat = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo depthImageInfo = imageCreateInfo(depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);

    VmaAllocationCreateInfo depthImageAlloc = {};
    depthImageAlloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    depthImageAlloc.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vmaCreateImage(allocator, &depthImageInfo, &depthImageAlloc, &depthImage.image, &depthImage.allocation, nullptr);

    deinitQueue.enqueue([=]() {
        LOG_CALL(vmaDestroyImage(allocator, depthImage.image, depthImage.allocation));
    });

    VkImageViewCreateInfo depthViewInfo = imageViewCreateInfo(depthFormat, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(device, &depthViewInfo, nullptr, &depthImageView));

    deinitQueue.enqueue([=]() {
        LOG_CALL(vkDestroyImageView(device, depthImageView, nullptr));
    });

    //TODO: delete depth stuff
}

void VulkanBackend::initCommandBuffers() {
    //create a command pool for commands submitted to the graphics queue.
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.pNext = nullptr;

    //the command pool will be one that can submit graphics commands
    commandPoolInfo.queueFamilyIndex = graphicsQueueFamily;
    //we also want the pool to allow for resetting of individual command buffers
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool));
    deinitQueue.enqueue([=]() {
        LOG_CALL(vkDestroyCommandPool(device, commandPool, nullptr));
    });

    //allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo = {};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.pNext = nullptr;

    //commands will be made from our _commandPool
    cmdAllocInfo.commandPool = commandPool;
    //we will allocate 1 command buffer
    cmdAllocInfo.commandBufferCount = 1;
    // command level is Primary
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &mainCommandBuffer));
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
    fbInfo.width = 640;
    fbInfo.height = 480;
    fbInfo.layers = 1;

    //grab how many images we have in the swapchain
    const uint32_t swapchainImageCount = swapchainImages.size();
    framebuffers = std::vector<VkFramebuffer>(swapchainImageCount);

    //create framebuffers for each of the swapchain image views
    for (int i = 0; i < swapchainImageCount; i++) {
        VkImageView attachments[] = { swapchainImageViews[i], depthImageView };

        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments = attachments;

        VK_CHECK(vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffers[i]));
    }

    deinitQueue.enqueue([=]() {
        LOG_CALL(
            for (int i = 0; i < swapchainImageViews.size(); i++) {
                vkDestroyFramebuffer(device, framebuffers[i], nullptr);
                // TODO: maybe where it's created?
                vkDestroyImageView(device, swapchainImageViews[i], nullptr);
            }
        );
    });
}

void VulkanBackend::initSyncStructs() {
    //create synchronization structures
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;

    //we want to create the fence with the Create Signaled flag, so we can wait on it before using it on a GPU command (for the first frame)
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &renderFence));
    deinitQueue.enqueue([=](){
        LOG_CALL(vkDestroyFence(device, renderFence, nullptr));
    });

    //for the semaphores we don't need any flags
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0;

    VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &presentSem));
    VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderSem));
    deinitQueue.enqueue([=](){
        LOG_CALL(
            vkDestroySemaphore(device, presentSem, nullptr);
            vkDestroySemaphore(device, renderSem, nullptr);
        );
    });
}

void VulkanBackend::draw() {
    VK_CHECK(vkWaitForFences(device, 1, &renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(device, 1, &renderFence));

    //request image from the swapchain, one second timeout
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(device, swapchain, 1000000000, presentSem, nullptr, &swapchainImageIndex));

    //now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(mainCommandBuffer, 0));

    //naming it cmd for shorter writing
    VkCommandBuffer cmd = mainCommandBuffer;

    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.pNext = nullptr;

    cmdBeginInfo.pInheritanceInfo = nullptr;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    //make a clear-color from frame number. This will flash with a 120*pi frame period.
    static float frameNumber = 0.0;
    VkClearValue colorClear;
    float flash = abs(sin(frameNumber / 120.f));
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
    rpInfo.renderArea.extent = { 640, 480 };
    rpInfo.framebuffer = framebuffers[swapchainImageIndex];

    //connect clear values
    VkClearValue clearValues[] = { colorClear, depthClear };
    rpInfo.clearValueCount = 2;
    rpInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    scene.draw(cmd);
    vkCmdEndRenderPass(cmd);
    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    //prepare the submission to the queue.
    //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    //we will signal the _renderSemaphore, to signal that rendering has finished

    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = nullptr;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit.pWaitDstStageMask = &waitStage;

    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &presentSem;

    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderSem;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submit, renderFence));

    // this will put the image we just rendered into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as it's necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;

    presentInfo.pSwapchains = &swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &renderSem;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(graphicsQueue, &presentInfo));

    //increase the number of frames drawn
    frameNumber++;
    printf("frame: %f\n", frameNumber);
}

void VulkanBackend::initPipelines() {
    VkPipelineLayoutCreateInfo layoutInfo = layoutCreateInfo();
    
    VkPushConstantRange pushConstant;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(MeshPushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    layoutInfo.pPushConstantRanges = &pushConstant;
    layoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));
    deinitQueue.enqueue([=]() {
        LOG_CALL(vkDestroyPipelineLayout(device, pipelineLayout, nullptr));
    });

    VkShaderModule triangleVert;
    //if (!loadShaderModule("./shaders/triangle.vert.glsl.spv", device, &triangleVert)) {
    if (!loadShaderModule("./shaders/tri.vert.glsl.spv", device, &triangleVert)) {
        printf("Failed building triangle vert shader.\n");
    } else {
        printf("Built triangle vert shader.\n");
    }

    VkShaderModule triangleFrag;
    if (!loadShaderModule("./shaders/triangle.frag.glsl.spv", device, &triangleFrag)) {
        printf("Failed building triangle frag shader.\n");
    } else {
        printf("Built triangle frag shader.\n");
    }

    VulkanPipelineBuilder builder;

    builder.shaderStages.push_back(shaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, triangleVert));
    builder.shaderStages.push_back(shaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFrag));

    VertexInputDescription vertexInputDescription = Vertex::getVertexDescription();

    builder.vertexInputInfo = vertexInputStateCreateInfo();
    builder.vertexInputInfo.pVertexBindingDescriptions = vertexInputDescription.bindings.data();
    builder.vertexInputInfo.vertexBindingDescriptionCount = vertexInputDescription.bindings.size();
    builder.vertexInputInfo.pVertexAttributeDescriptions = vertexInputDescription.attributes.data();
    builder.vertexInputInfo.vertexAttributeDescriptionCount = vertexInputDescription.attributes.size();

    builder.inputAssembly = inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    builder.viewport.x = 0.f;
    builder.viewport.y = 0.f;
    builder.viewport.width = 640.f;
    builder.viewport.height = 480.f;
    builder.viewport.minDepth = 0.f;
    builder.viewport.maxDepth = 1.f;

    builder.scissor.offset = { 0, 0 };
    builder.scissor.extent = { 640, 480 };

    builder.rasterizer = rasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
    builder.multisampling = multisampleStateCreateInfo();
    builder.colorBlendAttachment = colorBlendAttachmentState();
    builder.depthStencil = depthStencilCreateInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
    builder.pipelineLayout = pipelineLayout;

    pipeline = builder.build(device, defaultRenderpass);
    scene.createMaterial(pipeline, pipelineLayout, "defaultMaterial");

    deinitQueue.enqueue([=]() {
        LOG_CALL(vkDestroyPipeline(device, pipeline, nullptr));
    });

    // Compiled into the pipeline, no need to have them hanging around any more
    vkDestroyShaderModule(device, triangleVert, nullptr);
    vkDestroyShaderModule(device, triangleFrag, nullptr);

    //vkDestroyPipeline(device, pipeline, nullptr);
    //vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
}
