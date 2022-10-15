#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "engine.h"
#include "mesh.h"
#include "scene.h"

Material* Scene::createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name) {
    materials[name] = Material { VK_NULL_HANDLE, pipeline, layout };
    return &materials[name];
}

void Scene::initTestScene() {
    RenderObject suzanne;
    //suzanne.mesh = &meshes["suzanne"];
    suzanne.mesh = &meshes["lostEmpire"];
    suzanne.material = &materials["defaultMaterial"];
    suzanne.modelMatrix = glm::translate(glm::vec3(5.f, -10.f, 0.f));

    renderables.push_back(suzanne);

    for (int x = -20; x <= 20; x++) {
        for (int y = -20; y <= 20; y++) {
            RenderObject tri;
            tri.mesh = &meshes["triangle"];
            tri.material = &materials["defaultMaterial"];
            glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, glm::vec3(x, 0, y));
            glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.2, 0.2, 0.2));
            tri.modelMatrix = translation * scale;

            renderables.push_back(tri);
        }
    }
}

void Scene::draw(VulkanBackend& backend, VkCommandBuffer cmd, FrameData& frameData) {
    if (renderables.size() == 0) {
        return;
    }

    glm::mat4 view = glm::translate(glm::mat4(1.f), mainCamera.pos);
    glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.f);
    projection[1][1] *= -1; 

    GPUCameraData cameraData;
    cameraData.view = view;
    cameraData.projection = projection;
    cameraData.viewProjection = projection * view;

    backend.uploadData((void*)&cameraData, sizeof(GPUCameraData), 0, frameData.cameraUBO.allocation);

    backend.sceneParams.ambientColor = glm::vec4(0.f, 0.f, 0.f, 1.f);
    uint32_t sceneParamsUniformOffset = backend.padUniformBufferSize(sizeof(GPUSceneData)) * (backend.frameNumber % VulkanBackend::MAX_FRAMES_IN_FLIGHT);
    backend.uploadData((void*)&backend.sceneParams, sizeof(GPUSceneData), sceneParamsUniformOffset, backend.sceneParamsBuffers.allocation);

    // TODO: redo renderables into a SoA so that we can just upload the matrix array here.
    {
        void* gpuData;
        vmaMapMemory(backend.allocator, frameData.objectDataBuffer.allocation, &gpuData);
        GPUObjectData* gpuObjectData = (GPUObjectData*)gpuData;
        for (size_t i = 0; i < renderables.size(); i++) {
            gpuObjectData[i].modelMatrix = renderables[i].modelMatrix;
        }
        vmaUnmapMemory(backend.allocator, frameData.objectDataBuffer.allocation);
    }

    Mesh* lastMesh = nullptr;
    Material* lastMaterial = nullptr;
    for (size_t i = 0; i < renderables.size(); i++) {
        RenderObject& renderable = renderables[i];
        if (renderable.material == nullptr || renderable.mesh == nullptr) {
            continue;
        }

        if (renderable.material != lastMaterial) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderable.material->pipeline);
            lastMaterial = renderable.material;
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderable.material->pipelineLayout,
                0, 1, &frameData.globalDescriptor, 1, &sceneParamsUniformOffset);

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderable.material->pipelineLayout,
                1, 1, &frameData.objectDescriptor, 0, nullptr);

            if (renderable.material->textureSet != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderable.material->pipelineLayout,
                    2, 1, &renderable.material->textureSet, 0, nullptr);
            }
        }

        MeshPushConstants constants;
        constants.MVP = renderable.modelMatrix;
        vkCmdPushConstants(cmd, renderable.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

        if (renderable.mesh != lastMesh) {
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &renderable.mesh->vertexBuffer.buffer, &offset);
            lastMesh = renderable.mesh;
        }

        vkCmdDraw(cmd, renderable.mesh->vertices.size(), 1, 0, i);
    }
}
