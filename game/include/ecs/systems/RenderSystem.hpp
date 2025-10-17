#pragma once
#include "../System.hpp"
#include "../Registry.hpp"
#include "../components/Transform.hpp"
#include "../components/Mesh.hpp"
#include "../components/Material.hpp"
#include "../../renderer/VulkanRenderer.hpp"
#include "../components/Renderable.hpp"
#include "glm/gtc/matrix_access.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "../components/Camera.hpp"
#include <iostream>
#include "../components/Grid.hpp"
#include "../components/pushconstants.hpp"

class RenderSystem : public System {
public:
    RenderSystem(Registry& reg, VulkanRenderer& renderer)
        : registry(reg), renderer(renderer) {
        // Auto-track entities with Mesh + Transform + Material
        registry.subscribeToAdded<Mesh>([this](Entity e) { checkAndAdd(e); });
        registry.subscribeToAdded<Transform>([this](Entity e) { checkAndAdd(e); });
        registry.subscribeToAdded<Material>([this](Entity e) { checkAndAdd(e); });
        registry.subscribeToRemoved<Mesh>([this](Entity e) { removeEntity(e); });
        registry.subscribeToRemoved<Transform>([this](Entity e) { removeEntity(e); });
        registry.subscribeToRemoved<Material>([this](Entity e) { removeEntity(e); });

        registry.subscribeToAdded<Grid>([this](Entity e) { checkAndAdd(e); });
        registry.subscribeToRemoved<Grid>([this](Entity e) { removeEntity(e); });
    }

    // Implements the pure virtual function from System
    void update(float /*dt*/) override {
        drawFrame();
    }

    void drawFrame() {
        renderer.beginFrame();
        VkCommandBuffer cmd = renderer.getCurrentCommandBuffer();

        // Get camera entity (assume only one)
        Camera* camera = nullptr;
        Transform* cameraTransform = nullptr;

        for (auto [entity, cam, t] : registry.view<Camera, Transform>()) {
            camera = &cam;
            cameraTransform = &t;
            break; // take the first one
        }

        CameraUBO camUBO{};
        if (camera) {
            camera->aspect = renderer.getSwapchainExtent().width /
                (float)renderer.getSwapchainExtent().height;
            renderer.updateCameraUBO(*camera, camUBO);
        }

        for (Entity e : entities) {
            auto* t = registry.get<Transform>(e);
            auto* mesh = registry.get<Mesh>(e);
            auto* mat = registry.get<Material>(e);
            auto* grid = registry.get<Grid>(e);


            if (!t || !mesh || !mat) continue;
            if (mesh->vertexBuffer == VK_NULL_HANDLE || mesh->indexBuffer == VK_NULL_HANDLE) continue;





            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mat->pipeline);

            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                renderer.getPipelineLayout(),   // same layout used when creating pipeline
                0,                              // firstSet = 0 (matches 'set = 0' in shader)
                1,                              // descriptorSetCount = 1
                &renderer.getCameraDescriptorSet(), // descriptor set handle
                0, nullptr                      // dynamic offsets
            );

            PushConstants push{};

            push.model = t->matrix();
            push.color = mat->color;

            if (grid) {
                // Get camera position
                glm::vec3 camPos(0.0f);
                if (camera) {
                    camPos = cameraTransform->position;
                }

                // Calculate grid translation to follow camera
                glm::vec3 gridPos;
                gridPos.x = std::floor(camPos.x / grid->spacing) * grid->spacing;
                gridPos.y = 0.0f;  // Keep on ground plane
                gridPos.z = std::floor(camPos.z / grid->spacing) * grid->spacing;

                glm::mat4 model = glm::translate(glm::mat4(1.0f), gridPos);

                model = glm::rotate(model, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));

                push.model = model;
                push.color = grid->color;
            }

            vkCmdPushConstants(
                cmd,
                renderer.getPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(PushConstants),
                &push
            );

            VkBuffer vertexBuffers[] = { mesh->vertexBuffer };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(cmd, mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh->indices.size()), 1, 0, 0, 0);
        }

        renderer.endFrame();
    }





private:
    Registry& registry;
    VulkanRenderer& renderer;

    void checkAndAdd(Entity e) {
        // Add if (Mesh + Transform + Material) OR if it has Grid
        if ((registry.get<Mesh>(e) && registry.get<Transform>(e) && registry.get<Material>(e))
            || registry.get<Grid>(e)) {
            if (std::find(entities.begin(), entities.end(), e) == entities.end())
                entities.push_back(e);
        }
    }
};
