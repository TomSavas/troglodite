#include <stdio.h>
#include <vector>
#include <math.h>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "VkBootstrap.h"

#include <iostream>
#define VK_CHECK(x)                                                 \
    do                                                              \
{                                                               \
    VkResult err = x;                                           \
    if (err)                                                    \
    {                                                           \
        std::cout <<"Detected Vulkan error: " << err << std::endl; \
        abort();                                                \
    }                                                           \
} while (0)

struct VulkanBackend { 
    vkb::Instance vkbInstance;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkSurfaceKHR surface;

    VkSwapchainKHR swapchain; // from other articles
    // image format expected by the windowing system
    VkFormat swapchainImageFormat;
    //array of images from the swapchain
    std::vector<VkImage> swapchainImages;
    //array of image-views from the swapchain
    std::vector<VkImageView> swapchainImageViews;

    VkSemaphore presentSem;
    VkSemaphore renderSem;
    VkFence renderFence;

    static VulkanBackend init(GLFWwindow* window) {
        VulkanBackend backend;

        backend.initVulkan(window);
        backend.initSwapchain();
        backend.initCommandBuffers();
        backend.initDefaultRenderpass();
        backend.initFramebuffers();
        backend.initSyncStructs();

        return backend;
    }

    void initVulkan(GLFWwindow* window) {
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
    }

    void initSwapchain() {
        vkb::SwapchainBuilder builder { gpu, device, surface };
        vkb::Swapchain vkbSwapchain = builder
            .use_default_format_selection()
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(640, 480)
            .build()
            .value();

        swapchain = vkbSwapchain.swapchain;
        swapchainImages = vkbSwapchain.get_images().value();
        swapchainImageViews = vkbSwapchain.get_image_views().value();
        swapchainImageFormat = vkbSwapchain.image_format;
    }

    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;

    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;

    void initCommandBuffers() {
        //create a command pool for commands submitted to the graphics queue.
        VkCommandPoolCreateInfo commandPoolInfo = {};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.pNext = nullptr;

        //the command pool will be one that can submit graphics commands
        commandPoolInfo.queueFamilyIndex = graphicsQueueFamily;
        //we also want the pool to allow for resetting of individual command buffers
        commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool));


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

    VkRenderPass defaultRenderpass;
    std::vector<VkFramebuffer> framebuffers;
    void initDefaultRenderpass() {
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

        //we are going to create 1 subpass, which is the minimum you can do
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

        //connect the color attachment to the info
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        //connect the subpass to the info
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &defaultRenderpass));
    }

    void initFramebuffers() {
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
            fbInfo.pAttachments = &swapchainImageViews[i];
            VK_CHECK(vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffers[i]));
        }
    }

    void initSyncStructs() {
        //create synchronization structures
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.pNext = nullptr;

        //we want to create the fence with the Create Signaled flag, so we can wait on it before using it on a GPU command (for the first frame)
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &renderFence));

        //for the semaphores we don't need any flags
        VkSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.pNext = nullptr;
        semaphoreCreateInfo.flags = 0;

        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &presentSem));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderSem));
    }

    void draw() {
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
        VkClearValue clearValue;
        float flash = abs(sin(frameNumber / 120.f));
        clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };

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
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clearValue;

        {
            vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
            //finalize the render pass
            vkCmdEndRenderPass(cmd);
            //finalize the command buffer (we can no longer add commands, but it can now be executed)
            VK_CHECK(vkEndCommandBuffer(cmd));
        }

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
    }

    void deinit() {
        vkDestroyFence(device, renderFence, nullptr);
        vkDestroySemaphore(device, presentSem, nullptr);
        vkDestroySemaphore(device, renderSem, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);

        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyRenderPass(device, defaultRenderpass, nullptr);

        //destroy swapchain resources
        for (int i = 0; i < swapchainImageViews.size(); i++) {
            vkDestroyFramebuffer(device, framebuffers[i], nullptr);
            vkDestroyImageView(device, swapchainImageViews[i], nullptr);
        }

        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkb::destroy_debug_utils_messenger(instance, debugMessenger);
        vkDestroyInstance(instance, nullptr);
    }
};

int main(void) {
    if (!glfwInit()) {
        printf("Failed initing GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(640, 480, "Troglodite", NULL, NULL);

    VulkanBackend backend = VulkanBackend::init(window);

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        backend.draw();
    }

    // TODO: move elsewhere. Waits for the renderFence, to make sure fences, semaphores
    // and command pool are no longer in use
    VK_CHECK(vkWaitForFences(backend.device, 1, &backend.renderFence, true, 1000000000));

    backend.deinit();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
