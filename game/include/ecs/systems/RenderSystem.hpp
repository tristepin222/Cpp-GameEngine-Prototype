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
#include "../components/Grid.hpp"
#include "../components/pushconstants.hpp"
#include <functional>

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

    void update(float dt) override {
        buildInstanceData();
        renderer.updateInstanceBuffer();  // bulk upload to GPU
    }

    void drawFrame(const std::function<void(VkCommandBuffer)>& overlayPass = {}) {
        renderer.beginFrame();

        VkCommandBuffer cmd = renderer.getCurrentCommandBuffer();

        // --- Update CameraSoA using TransformSoA ---
        renderer.updateCameraUBO();


        std::vector<InstanceDataGPU> gpuData(renderer.instanceDataCPU.size());
        for (size_t i = 0; i < renderer.instanceDataCPU.size(); i++) {
            gpuData[i].model = renderer.instanceDataCPU.models[i];
            gpuData[i].color = renderer.instanceDataCPU.colors[i];
        }
        renderer.instanceBuffer.uploadData(gpuData.data(), gpuData.size() * sizeof(InstanceDataGPU));


        // --- Draw grids ---
        drawGrids();

        // --- Draw instances ---
        drawBatches();

        if (overlayPass) {
            overlayPass(cmd);
        }

        renderer.endFrame();
    }

private:
    Registry& registry;
    VulkanRenderer& renderer;
    std::vector<Entity> entities;
    std::unordered_map<Entity, size_t> entityToInstanceIndex;

    void checkAndAdd(Entity e) {
        if ((registry.get<Mesh>(e) && registry.get<Transform>(e) && registry.get<Material>(e))
            || registry.get<Grid>(e)) {
            if (std::find(entities.begin(), entities.end(), e) == entities.end())
                entities.push_back(e);
        }
    }

    void removeEntity(Entity e) {
        entities.erase(std::remove(entities.begin(), entities.end(), e), entities.end());
    }

    void buildInstanceData() {
        renderer.instanceDataCPU.clear();
        entityToInstanceIndex.clear();

        // --- Mesh + Material instances (excluding grids) ---
        for (auto [entity, mesh, transform, material] :
            registry.view<Mesh, Transform, Material>()) {

            // Skip entities that have Grid component (they're drawn separately)
            if (registry.get<Grid>(entity)) continue;

            InstanceData inst{};
            inst.model = transform.matrix();
            inst.materialID = material.id;
            inst.meshID = mesh.id;
            inst.color = material.color;

            size_t idx = renderer.instanceDataCPU.push(inst);
            entityToInstanceIndex[entity] = idx;
        }
    }

    glm::vec3 getCameraPosition() {
        if (renderer.hasActiveCamera()) {
            return renderer.getActiveCameraPosition();
        }
        return glm::vec3(0.0f);
    }

    void drawBatches() {
        VkCommandBuffer cmd = renderer.getCurrentCommandBuffer();
        VkDescriptorSet cameraSet = renderer.getCameraDescriptorSet();

        // --- Group instances by Mesh + Material using the *existing* instance indices
        std::unordered_map<std::pair<Mesh*, Material*>, std::vector<size_t>, pair_hash> batches;

        for (Entity e : entities) {
            auto* mesh = registry.get<Mesh>(e);
            auto* mat = registry.get<Material>(e);
            auto* transform = registry.get<Transform>(e);
            if (!mesh || !mat || !transform) continue;

            auto it = entityToInstanceIndex.find(e);
            if (it == entityToInstanceIndex.end()) continue;
            size_t idx = it->second;
            batches[{mesh, mat}].push_back(idx);
        }

        for (auto& [key, batch] : batches) {
            Mesh* mesh = key.first;
            Material* mat = key.second;

            if (!mesh || !mat || batch.empty()) continue;

            // --- Bind pipeline
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mat->pipeline);

            // --- Bind descriptor sets (camera)
            vkCmdBindDescriptorSets(cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                mat->pipelineLayout,
                0,
                1,
                &cameraSet,
                0,
                nullptr);

            // --- Bind vertex buffers
            VkBuffer vertexBuffers[] = { mesh->vertexBuffer };
            VkDeviceSize vertexOffsets[] = { 0 };
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, vertexOffsets);

            // If you still have an instance buffer bound elsewhere, it's harmless,
            // but we don't rely on it here because shader uses push.model.
            // vkCmdBindVertexBuffers(cmd, 1, 1, &renderer.instanceBuffer.get(), offsets);

            // --- Bind index buffer
            vkCmdBindIndexBuffer(cmd, mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            // Draw each instance in this batch individually, pushing its model/color
            for (size_t instanceIdx : batch) {
                // read instance data from renderer.instanceDataCPU
                const InstanceDataGPU& inst = renderer.instanceDataCPU.get(instanceIdx);

                PushConstants pc{};
                pc.model = inst.model;
                pc.color = inst.color;
                // If your PushConstants struct on CPU side has more fields, initialize them here.

                vkCmdPushConstants(cmd,
                    mat->pipelineLayout,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0,
                    sizeof(PushConstants),
                    &pc);

                // Draw one instance
                vkCmdDrawIndexed(cmd,
                    static_cast<uint32_t>(mesh->indices.size()),
                    1, // instance count = 1 because we handle instances manually
                    0,  // firstIndex
                    0,  // vertexOffset
                    0); // firstInstance
            }
        }
    }

    void drawGrids() {
        VkCommandBuffer cmd = renderer.getCurrentCommandBuffer();
        VkDescriptorSet cameraSet = renderer.getCameraDescriptorSet();

        for (Entity e : entities) {
            auto* grid = registry.get<Grid>(e);
            auto* mat = registry.get<Material>(e);
            if (!grid || !mat) continue;

            // Bind grid pipeline
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mat->pipeline);

            // Bind descriptor sets (camera)
            vkCmdBindDescriptorSets(cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                mat->pipelineLayout,
                0,
                1,
                &cameraSet,
                0,
                nullptr);

            // Get camera position for grid positioning
            glm::vec3 camPos = getCameraPosition();

            // Push constants for grid shader
            struct GridPushConstants {
                glm::mat4 viewProj;
                glm::vec4 color;
                glm::vec3 camPos;
                float scale;
                float fade;
            } pc;

            // Use the camera's view-projection matrix
            if (renderer.hasActiveCamera()) {
                pc.viewProj = renderer.getActiveCameraViewProj();
            } else {
                pc.viewProj = glm::mat4(1.0f);
            }

            pc.color = grid->color;
            pc.camPos = camPos;
            pc.scale = grid->spacing;
            pc.fade = grid->size;

            vkCmdPushConstants(cmd,
                mat->pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(GridPushConstants),
                &pc);

            // Draw 6 vertices so the shader can emit a full-screen quad as two triangles.
            vkCmdDraw(cmd, 6, 1, 0, 0);
        }
    }

    struct pair_hash {
        std::size_t operator()(const std::pair<Mesh*, Material*>& p) const {
            return std::hash<Mesh*>()(p.first) ^ (std::hash<Material*>()(p.second) << 1);
        }
    };
};
