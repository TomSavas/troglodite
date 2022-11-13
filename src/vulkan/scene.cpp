#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "engine.h"
#include "mesh.h"
#include "scene.h"
#include "vk_init_helpers.h"
#include "descriptors.h"

TEMPMaterial* Scene::createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name) {
    materials[name] = TEMPMaterial { VK_NULL_HANDLE, pipeline, layout };
    return &materials[name];
}

Scene::Object Scene::addObject(std::string meshName, std::string materialDir, bool separateMaterialInstances) {
    // TODO: we need to cache meshes
    Object object;
    object.objectDataIndex = objectData.size();

    objectData.emplace_back();

    static Model model; // TEMP: testing 
    model.loadFromObj(meshName.c_str(), materialDir.c_str());
    for (auto& mesh : model.meshes) {
        //printf("uploading mesh %s\n", mesh.name.c_str());
        backend->uploadMesh(mesh);

        uint32_t meshInstanceIndex = meshInstances.size();
        uint32_t materialInstanceIndex = backend->materials->materials[Materials::DEFAULT_LIT].instances.size();

        // TODO: separatematerialInstances
        MaterialInstance& materialInstance = backend->materials->materials[Materials::DEFAULT_LIT].instances.emplace_back();
        materialInstance.meshInstanceIndices.push_back(meshInstanceIndex);

        // TODO: some creator for materialInstance
        for (auto defaultTexture : backend->materials->materials[Materials::DEFAULT_LIT].defaultTextures) {
            materialInstance.textures[defaultTexture.first] = defaultTexture.second;
        }

        // Override the default textures
        // TEMP
        VkSamplerCreateInfo samplerInfo = samplerCreateInfo(VK_FILTER_NEAREST);

        // TODO: load textures
        CacheLoadResult<SampledTexture> albedo = backend->textureCache->load(materialDir + "/../" + mesh.loaderMaterial.diffuse_texname.c_str(), samplerInfo);
        if (albedo.success) {
            materialInstance.textures["albedo"] = albedo.data;
        }
        CacheLoadResult<SampledTexture> normal = backend->textureCache->load(materialDir + "/../" + mesh.loaderMaterial.bump_texname.c_str(), samplerInfo);
        if (normal.success) {
            materialInstance.textures["normal"] = normal.data;
        }

        // TEMP
        VkDescriptorImageInfo albedoDescriptorInfo = {};
        albedoDescriptorInfo.sampler = materialInstance.textures["albedo"]->sampler;
        albedoDescriptorInfo.imageView = materialInstance.textures["albedo"]->texture->view;
        albedoDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        //VkDescriptorImageInfo normalDescriptorInfo = {};
        //normalDescriptorInfo.sampler = normal.data->sampler;
        //normalDescriptorInfo.imageView = normal.data->texture->view;
        //normalDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        DescriptorSetBuilder::begin(backend->device, *backend->descriptorSetLayoutCache, *backend->descriptorSetAllocator)
            .bindImages(&albedoDescriptorInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0)
            //.bindImages(&normalDescriptorInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
            .build(&materialInstance.textureDescriptorSet);

        // TODO: stupid -- for testing only. Once we cache meshes we can simply add to objectIndices 
        meshInstances.push_back(MeshInstance{ mesh, std::vector<uint32_t>{ object.objectDataIndex } });

        object.meshInstanceIndices.push_back(meshInstanceIndex);
        object.materialInstanceIndices.push_back(materialInstanceIndex);
    }

    return object;
}

void Scene::initTestScene() {
    //Object sponza = loadMesh("/home/savas/Projects/ignoramus_renderer/assets/sponza/sponza.obj", "/home/savas/Projects/ignoramus_renderer/assets/sponza");
    Object object = addObject("/home/savas/Projects/ignoramus_renderer/assets/sponza/sponza.obj", "/home/savas/Projects/ignoramus_renderer/assets/sponza");

    RenderObject suzanne;
    suzanne.mesh = &meshes["suzanne"];
    //suzanne.mesh = &meshes["lostEmpire"];
    suzanne.material = &materials["defaultMaterial"];
    suzanne.modelMatrix = glm::translate(glm::vec3(5.f, -10.f, 0.f));

    //renderables.push_back(suzanne);

    for (int i = 0; i < object.meshInstanceIndices.size(); ++i) {
        renderables.push_back(RenderObject{
            &meshInstances[object.meshInstanceIndices[i]].mesh,
            &materials["defaultMaterial"],
            glm::translate(glm::vec3(5.f, -10.f, 0.f)),

            &backend->materials->materials[Materials::DEFAULT_LIT]
                .instances[object.materialInstanceIndices[i]]
        });
    }

    //for (int x = -20; x <= 20; x++) {
    //    for (int y = -20; y <= 20; y++) {
    //        RenderObject tri;
    //        tri.mesh = &meshes["triangle"];
    //        tri.material = &materials["defaultMaterial"];
    //        glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, glm::vec3(x, 0, y));
    //        glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.2, 0.2, 0.2));
    //        tri.modelMatrix = translation * scale;

    //        renderables.push_back(tri);
    //    }
    //}
}

glm::vec3 right(glm::mat4 mat) {
    return mat * glm::vec4(1.f, 0.f, 0.f, 0.f);
}

glm::vec3 up(glm::mat4 mat) {
    return mat * glm::vec4(0.f, 1.f, 0.f, 0.f);
}

glm::vec3 forward(glm::mat4 mat) {
    return mat * glm::vec4(0.f, 0.f, -1.f, 0.f);
}

void Scene::update(float dt) {
    static glm::dvec2 lastMousePos = glm::vec2(-1.f -1.f);
    if (glfwGetMouseButton(backend->window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        static double radToVertical = .0;
        static double radToHorizon = .0;

        if (lastMousePos.x == -1.f) {
            glfwGetCursorPos(backend->window, &lastMousePos.x, &lastMousePos.y);
        }

        glm::dvec2 mousePos;
        glfwGetCursorPos(backend->window, &mousePos.x, &mousePos.y);
        glm::dvec2 mousePosDif = mousePos - lastMousePos;
        lastMousePos = mousePos;

        radToVertical += mousePosDif.x * mainCamera.horizontalRotationSpeed / mainCamera.aspectRatio;
        while(radToVertical > glm::pi<float>() * 2) {
            radToVertical -= glm::pi<float>() * 2;
        }
        while(radToVertical < -glm::pi<float>() * 2) {
            radToVertical += glm::pi<float>() * 2;
        }

        radToHorizon -= mousePosDif.y * mainCamera.horizontalRotationSpeed;
        radToHorizon = std::min(std::max(radToHorizon, -glm::pi<double>() / 2 + 0.01), glm::pi<double>() / 2 - 0.01);

        mainCamera.rotation = glm::eulerAngleYX(-radToVertical, radToHorizon);
    }
    else
    {
        glfwGetCursorPos(backend->window, &lastMousePos.x, &lastMousePos.y);
    }

    glm::vec3 dir(0.f, 0.f, 0.f);
    if (glfwGetKey(backend->window, GLFW_KEY_W) == GLFW_PRESS) {
        dir += forward(mainCamera.rotation);
    }
    if (glfwGetKey(backend->window, GLFW_KEY_S) == GLFW_PRESS) {
        dir -= forward(mainCamera.rotation);
    }

    if (glfwGetKey(backend->window, GLFW_KEY_D) == GLFW_PRESS) {
        dir += right(mainCamera.rotation);
    }
    if (glfwGetKey(backend->window, GLFW_KEY_A) == GLFW_PRESS) {
        dir -= right(mainCamera.rotation);
    }

    if (glfwGetKey(backend->window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        dir += up(mainCamera.rotation);
    }
    if (glfwGetKey(backend->window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        dir -= up(mainCamera.rotation);
    }

    mainCamera.pos += dir * mainCamera.moveSpeed * dt;
}

void Scene::draw(VkCommandBuffer cmd, FrameData& frameData) {
    if (renderables.size() == 0) {
        return;
    }

    glm::mat4 view = glm::lookAt(mainCamera.pos, mainCamera.pos + forward(mainCamera.rotation), up(mainCamera.rotation));
    glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 20000.f);
    projection[1][1] *= -1; 

    GPUCameraData cameraData;
    cameraData.view = view;
    cameraData.projection = projection;
    cameraData.viewProjection = projection * view;

    backend->uploadData((void*)&cameraData, sizeof(GPUCameraData), 0, frameData.cameraUBO.allocation);

    backend->sceneParams.ambientColor = glm::vec4(0.f, 0.f, 0.f, 1.f);
    uint32_t sceneParamsUniformOffset = backend->padUniformBufferSize(sizeof(GPUSceneData)) * (backend->frameNumber % VulkanBackend::MAX_FRAMES_IN_FLIGHT);
    backend->uploadData((void*)&backend->sceneParams, sizeof(GPUSceneData), sceneParamsUniformOffset, backend->sceneParamsBuffers.allocation);

    // TODO: redo renderables into a SoA so that we can just upload the matrix array here.
    {
        void* gpuData;
        vmaMapMemory(backend->allocator, frameData.objectDataBuffer.allocation, &gpuData);
        GPUObjectData* gpuObjectData = (GPUObjectData*)gpuData;
        for (size_t i = 0; i < renderables.size(); i++) {
            gpuObjectData[i].modelMatrix = renderables[i].modelMatrix;
        }
        vmaUnmapMemory(backend->allocator, frameData.objectDataBuffer.allocation);
    }

    //for (uint8_t passIndex = 0; passIndex < (uint8_t)PassType::PASS_COUNT; ++passIndex) {
    //    PassType pass = static_cast<PassType>(passIndex);

    //    //for (Material* material : passMaterials[passIndex]) {
    //    for (auto mat : backend->materials->materials) {
    //        Material& material = mat.second;

    //        ShaderPass* shaderPass = material.perPassShaders[passIndex];
    //        if (shaderPass == nullptr) {
    //            continue;
    //        }

    //        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shaderPass->pipeline);
    //        //material.bindDescriptorSets(pass);

    //        // TODO: push all metrial instances and then draw with a single draw call
    //        // TODO: cache the resulting draw calls and only invalidate the cache once objects
    //        // get added / removed... Weeeeeeell might get a little bit more complicated when doing culling
    //        for (auto& materialInstance : material.instances) {
    //            //materialInstance.bindBuffers();
    //            //materialInstance.bindImages();

    //            // TODO: merge into a single draw call -- simple just write objectIds into a buffer
    //            MeshInstance* lastMeshInstance = nullptr;
    //            for (uint32_t meshInstanceIndex : materialInstance.meshInstanceIndices) {
    //                MeshInstance& meshInstance = meshInstances[meshInstanceIndex];
    //                if (&meshInstance != lastMeshInstance) {
    //                    VkDeviceSize offset = 0;
    //                    vkCmdBindVertexBuffers(cmd, 0, 1, &meshInstance.mesh.vertexBuffer.buffer, &offset);
    //                    lastMeshInstance = &meshInstance;
    //                }

    //                for (uint32_t objectIndex : meshInstance.objectDataIndices) {
    //                    // TODO: bind object data?
    //                    vkCmdDraw(cmd, meshInstance.mesh.vertices.size(), 1, 0, objectIndex);
    //                }
    //            }
    //        }
    //    }
    //}

    Mesh* lastMesh = nullptr;
    TEMPMaterial* lastMaterial = nullptr;
    for (size_t i = 0; i < renderables.size(); i++) {
        RenderObject& renderable = renderables[i];
        if (renderable.material == nullptr || renderable.mesh == nullptr) {
            continue;
        }

        if (renderable.material != lastMaterial) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderable.material->pipeline);
            lastMaterial = renderable.material;
            //renderable.materialInstance->baseMaterial.bindDescriptorSets();

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderable.material->pipelineLayout,
                0, 1, &frameData.globalDescriptor, 1, &sceneParamsUniformOffset);

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderable.material->pipelineLayout,
                1, 1, &frameData.objectDescriptor, 0, nullptr);

            //if (renderable.material->textureSet != VK_NULL_HANDLE) {
            //    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderable.material->pipelineLayout,
            //        2, 1, &renderable.material->textureSet, 0, nullptr);
            //}
        }
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderable.material->pipelineLayout,
            2, 1, &renderable.tempInstance->textureDescriptorSet, 0, nullptr);

        //renderable.materialInstance.bindBuffers();
        //renderable.materialInstance.bindImages();

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
