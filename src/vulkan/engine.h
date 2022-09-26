#pragma once

#include <deque>
#include <functional>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "VkBootstrap.h"

#include "vulkan/mesh.h"
#include "vulkan/types.h"
#include "vulkan/vk_init_helpers.h"

#define VK_CHECK(x)                                                \
    do                                                             \
{                                                                  \
    VkResult err = x;                                              \
    if (err)                                                       \
    {                                                              \
        std::cout << "Detected Vulkan error: " << err << std::endl;\
        raise(SIGTERM);                                            \
    }                                                              \
} while (0)

struct FunctionQueue {
    std::deque<std::function<void()>> functions;

    void enqueue(std::function<void()>&& func);
    void execute();
};

struct VulkanPipelineBuilder {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo multisampling;
    VkPipelineLayout pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo depthStencil;

    VkPipeline build(VkDevice device, VkRenderPass pass);
};

struct VulkanBackend { 
    FunctionQueue deinitQueue;

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

    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;

    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;

    VkRenderPass defaultRenderpass;
    std::vector<VkFramebuffer> framebuffers;

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    VmaAllocator allocator;

    Mesh triangleMesh;

    VkImageView depthImageView;
    AllocatedImage depthImage;

    VkFormat depthFormat;


    static VulkanBackend init(GLFWwindow* window);
    void deinit();

    void initVulkan(GLFWwindow* window);
    void initSwapchain();
    void initCommandBuffers();
    void initDefaultRenderpass();
    void initFramebuffers();
    void initSyncStructs();
    void initPipelines();

    void loadMeshes();
    void uploadMesh(Mesh& mesh);

    void draw();
};
