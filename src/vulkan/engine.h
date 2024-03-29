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
#include "vulkan/renderpass.h"
#include "vulkan/samplers.h"

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

struct UploadContext {
    VkFence uploadFence;
    VkCommandPool cmdPool;
    VkCommandBuffer cmdBuffer;
};

struct FunctionQueue {
    std::deque<std::function<void()>> functions;

    void enqueue(std::function<void()>&& func);
    void execute();
};

struct GPUCameraData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 viewProjection;
};

struct GPUSceneData {
    glm::vec4 fogColor; // w exponent
    glm::vec4 fogDistances; // x for min, y for max, zw unused
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

struct GPUObjectData {
    glm::mat4 modelMatrix;
};

struct FrameData {
    AllocatedBuffer cameraUBO;
    VkDescriptorSet globalDescriptor;

    AllocatedBuffer objectDataBuffer;
    VkDescriptorSet objectDescriptor;

    VkSemaphore presentSem;
    VkSemaphore renderSem;
    VkFence renderFence;

    VkCommandPool cmdPool;
    VkCommandBuffer cmdBuffer;
};

struct RenderAttachments;
struct DescriptorSetLayoutCache;
struct DescriptorSetAllocator;
struct ShaderModuleCache;
struct ShaderPassCache;
struct Materials;
struct RenderPass;
struct VulkanBackend { 
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    FrameData inFlightFrames[MAX_FRAMES_IN_FLIGHT];

    VkViewport viewport;
    VkRect2D scissor;

    VkExtent3D viewportSize;

    GPUSceneData sceneParams;
    AllocatedBuffer sceneParamsBuffers;

    FunctionQueue deinitQueue;
    FunctionQueue swapchainDeinitQueue;

    bool swapchainRegenRequested = false;

    Scene* scene;

    UploadContext uploadCtx;

    vkb::Instance vkbInstance;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkSurfaceKHR surface;
    VkPhysicalDeviceProperties gpuProperties;

    VkSwapchainKHR swapchain; // from other articles
    // image format expected by the windowing system
    VkFormat swapchainImageFormat;
    //array of images from the swapchain
    std::vector<VkImage> swapchainImages;
    //array of image-views from the swapchain
    std::vector<VkImageView> swapchainImageViews;

    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;

    VmaAllocator allocator;

    int frameNumber;

    GLFWwindow* window;

    RenderAttachments* attachments;

    DescriptorSetLayoutCache* descriptorSetLayoutCache;
    DescriptorSetAllocator* descriptorSetAllocator;

    ShaderModuleCache* shaderModuleCache;
    ShaderPassCache* shaderPassCache;
    SamplerCache* samplerCache;

    TextureCache* textureCache;
    Materials* materials;

    RenderPass* outputRenderPass;
    std::vector<RenderPass> renderPasses;

    VkDescriptorSet blitTextureDescriptorSet;

    // TODO: should be stored along with the descriptor set
    VkDescriptorSetLayout globalDescriptorSetLayout;
    VkDescriptorSetLayout objectDescriptorSetLayout;
    VkDescriptorPool descriptorPool;

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    void registerCallbacks();

    static VulkanBackend init(GLFWwindow* window);
    void deinit();

    void initVulkan();
    void initSwapchain();
    void initCommandBuffers();
    void initDefaultRenderpass();
    void initSyncStructs();
    void initDescriptors();
    void initPipelines();
    void initImgui();

    void uploadMesh(Mesh& mesh);
    void uploadData(const void* data, size_t size, size_t offset, VmaAllocation allocation);

    void draw();

    void immediateBlockingSubmit(std::function<void(VkCommandBuffer)>&& func);

    FrameData& currentFrame();

    AllocatedBuffer createBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    size_t padUniformBufferSize(size_t requestedSize);
};
