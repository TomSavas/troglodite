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
#include "vulkan/scene.h"
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

struct GPUCameraData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 viewProjection;
};

struct FrameData {
    AllocatedBuffer cameraUBO;
    VkDescriptorSet globalDescriptor;

    VkSemaphore presentSem;
    VkSemaphore renderSem;
    VkFence renderFence;

    VkCommandPool cmdPool;
    VkCommandBuffer cmdBuffer;
};

struct VulkanBackend { 
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    FrameData inFlightFrames[MAX_FRAMES_IN_FLIGHT];

    FunctionQueue deinitQueue;

    Scene scene;

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

    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;

    VkRenderPass defaultRenderpass;
    std::vector<VkFramebuffer> framebuffers;

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    VmaAllocator allocator;

    Mesh triangleMesh;

    VkImageView depthImageView;
    AllocatedImage depthImage;

    VkFormat depthFormat;

    int frameNumber;

    // TODO: should be stored along with the descriptor set
    VkDescriptorSetLayout globalDescriptorSetLayout;
    VkDescriptorPool descriptorPool;

    static VulkanBackend init(GLFWwindow* window);
    void deinit();

    void initVulkan(GLFWwindow* window);
    void initSwapchain();
    void initCommandBuffers();
    void initDefaultRenderpass();
    void initFramebuffers();
    void initSyncStructs();
    void initDescriptors();
    void initPipelines();

    void loadMeshes();
    void uploadMesh(Mesh& mesh);
    void uploadData(const void* data, size_t size, VmaAllocation allocation);

    AllocatedBuffer createBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

    void draw();

    FrameData& currentFrame();
};
