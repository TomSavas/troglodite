#include <chrono>
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
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Troglodite", NULL, NULL);

    VulkanBackend backend = VulkanBackend::init(window);
    backend.registerCallbacks();
    
    backend.scene->backend = &backend;
    backend.scene->initTestScene();
    backend.scene->mainCamera.pos = { 0.f, 6.f, 10.f };

    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    while(!glfwWindowShouldClose(window)) {
        end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        double dt = (double)elapsed.count() / 1000000000.0;
        start = std::chrono::high_resolution_clock::now();

        glfwPollEvents();

        backend.scene->update(dt);
        {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::ShowDemoWindow();
        }

        {
            const float PAD = 10.0f;

            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
            ImVec2 work_size = viewport->WorkSize;
            ImVec2 window_pos;
            window_pos.x = work_pos.x + work_size.x - PAD;
            window_pos.y = work_pos.y + PAD;
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(1.f, 0.f));

            static bool open = false;
            static double frameTimes[256];
            static int frameTimesIdx = 0;
            if (!open) {
                for (int i =0; i < 256; ++i) {
                    frameTimes[i] = dt;
                }
                open = true;
            }

            frameTimes[frameTimesIdx] = dt;
            double avgFrameTime = dt;
            for (int i = (frameTimesIdx + 1) % 256; i != frameTimesIdx; i = (i + 1) % 256) {
                avgFrameTime += frameTimes[i];
            }
            avgFrameTime /= 256.f;
            frameTimesIdx = (frameTimesIdx + 1) % 256;

            ImGui::SetNextWindowBgAlpha(0.75f);
            if (ImGui::Begin("Info", &open, window_flags))
            {
                ImGui::Text("Troglodite");
                ImGui::SameLine();

#ifdef DEBUG
                ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "DEBUG");
#else
                ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "RELEASE");
#endif
                ImGui::Separator();
                ImGui::Text("Frames: %.d", backend.frameNumber);
                ImGui::Separator();
                
                ImGui::Text("CPU: % 11.6f msec", dt * 1000.f);
                ImGui::Text("     % 11.6f Hz", 1.0 / dt);
                ImGui::Text("     Avg");
                ImGui::Text("CPU: % 11.6f msec", avgFrameTime * 1000.f);
                ImGui::Text("     % 11.6f Hz", 1.0 / avgFrameTime);
            }
            ImGui::End();
        }
        ImGui::Render();
        backend.draw();

        end = std::chrono::system_clock::now();
        // TODO: quick hack -- VK_PRESENT_MODE_FIFO_KHR doesn't work on my 6600XT, so
        // hack the FPS limit with VK_PRESENT_MODE_MAILBOX_KHR
        //usleep(16666);
    }
    VK_CHECK(vkDeviceWaitIdle(backend.device));

    backend.deinit();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
