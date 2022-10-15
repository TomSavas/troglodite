#include <stdio.h>
#include <vector>
#include <math.h>

#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "vulkan/engine.h"

#define TINYOBJLOADER_IMPLEMENTATION
// Optional. define TINYOBJLOADER_USE_MAPBOX_EARCUT gives robust trinagulation. Requires C++11
#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include "tiny_obj_loader.h"

int main(void) {
    if (!glfwInit()) {
        printf("Failed initing GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(640, 480, "Troglodite", NULL, NULL);

    VulkanBackend backend = VulkanBackend::init(window);
    
    backend.scene.initTestScene();
    backend.scene.mainCamera.pos = { 0.f, -6.f, -10.f };

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::ShowDemoWindow();
        }

        ImGui::Render();
        backend.draw();

        // TODO: quick hack -- VK_PRESENT_MODE_FIFO_KHR doesn't work on my 6600XT, so
        // hack the FPS limit with VK_PRESENT_MODE_MAILBOX_KHR
        usleep(16666);
    }
    VK_CHECK(vkDeviceWaitIdle(backend.device));

    backend.deinit();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
