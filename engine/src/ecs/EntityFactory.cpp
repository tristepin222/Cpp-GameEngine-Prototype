#include "ecs/EntityFactory.hpp"
#include "renderer/VulkanRenderer.hpp"
#include "renderer/ResourceManager.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/Name.hpp"
#include "ecs/components/PrimitiveType.hpp"
#include "ecs/components/Mesh.hpp"
#include "ecs/components/Material.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/inputComponent.hpp"
#include "ecs/components/Grid.hpp"
#include "ecs/components/primitives.hpp"
#include "ecs/components/Skeleton.hpp"
#include "ecs/components/Animator.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>

/**
 * @namespace EntityFactory
 * @brief Factory methods for creating common game entities (cameras, grids, shapes) and uploading their mesh data.
 */
namespace EntityFactory {

    /**
     * @brief Uploads mesh component data to Vulkan GPU buffers.
     * @param registry ECS registry containing target entity.
     * @param renderer Vulkan renderer instance.
     * @param entity Target entity.
     */
    void uploadMesh(Registry& registry, VulkanRenderer& renderer, Entity entity) {
        Mesh* mesh = registry.get<Mesh>(entity);
        if (!mesh) {
            return;
        }

        const size_t meshID = renderer.meshSoA.push(mesh->vertices, mesh->indices);
        renderer.uploadMesh(meshID);

        mesh->vertexBuffer = renderer.meshSoA.vertexBuffers[meshID].get();
        mesh->indexBuffer = renderer.meshSoA.indexBuffers[meshID].get();
    }

    /**
     * @brief Spawns a primitive shape entity (e.g. Triangle, Cube, Quad).
     * @param registry Target ECS Registry.
     * @param renderer Vulkan renderer context.
     * @param kind Primitive kind (Triangle, Cube, Quad).
     * @param name Entity name.
     * @param position Translation vector.
     * @return Spawned entity handle.
     */
    Entity spawnPrimitive(Registry& registry, VulkanRenderer& renderer, PrimitiveKind kind, const std::string& name, const glm::vec3& position) {
        Entity entity = registry.create();
        if (entity.getId() == Entity::INVALID_ENTITY) {
            return Entity();
        }

        registry.emplace<Transform>(entity, Transform{ position });
        registry.emplace<Name>(entity, Name{ name });
        registry.emplace<PrimitiveType>(entity, PrimitiveType{ kind });

        Mesh meshData;
        glm::vec4 defaultColor(1.0f);
        switch (kind) {
        case PrimitiveKind::Triangle:
            meshData = Primitives::makeTriangle();
            defaultColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
            break;
        case PrimitiveKind::Cube:
            meshData = Primitives::makeCube();
            defaultColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
            break;
        case PrimitiveKind::Quad:
            meshData = Primitives::makeQuad();
            break;
        }

        registry.emplace<Mesh>(entity, std::move(meshData));
        registry.emplace<Material>(entity, Material{ defaultColor });

        PipelineHandle pipeline = renderer.createPipelineForShaders(
            "build/shaders/unlit.vert.spv",
            "build/shaders/unlit.frag.spv"
        );

        if (Material* material = registry.get<Material>(entity)) {
            material->pipeline = pipeline.pipeline;
            material->pipelineLayout = pipeline.layout;
        }

        uploadMesh(registry, renderer, entity);
        return entity;
    }

    /**
     * @brief Spawns a camera entity with projection setup.
     * @param registry Target ECS Registry.
     * @param renderer Vulkan renderer context.
     * @param name Camera name.
     * @param position Location vector.
     * @param rotation Yaw, pitch, roll orientation.
     * @param fov Field of view.
     * @return Spawned camera entity handle.
     */
    Entity spawnCamera(Registry& registry, VulkanRenderer& renderer, const std::string& name, const glm::vec3& position, const glm::vec3& rotation, float fov) {
        Entity cameraEntity = registry.create();
        if (cameraEntity.getId() == Entity::INVALID_ENTITY) {
            throw std::runtime_error("Ran out of entity IDs");
        }

        registry.emplace<Camera>(cameraEntity, Camera{});
        registry.emplace<Transform>(cameraEntity, Transform{});
        registry.emplace<InputComponent>(cameraEntity, InputComponent{});
        registry.emplace<Name>(cameraEntity, Name{ name });

        Camera* camera = registry.get<Camera>(cameraEntity);
        Transform* transform = registry.get<Transform>(cameraEntity);
        if (!camera || !transform) {
            throw std::runtime_error("Camera setup failed");
        }

        transform->position = position;
        transform->rotation = rotation;
        camera->fov = fov;

        int width = 0;
        int height = 0;
        glfwGetWindowSize(renderer.getWindow(), &width, &height);
        camera->aspect = static_cast<float>(width) / static_cast<float>(height);
        renderer.setActiveCamera(camera->projection(), transform->position, camera->view(*transform));
        return cameraEntity;
    }

    /**
     * @brief Spawns a grid overlay drawing reference.
     * @param registry Target ECS Registry.
     * @param renderer Vulkan renderer context.
     * @param name Grid name.
     * @param position Location vector.
     * @param rotation Orientation rotation.
     * @param color Grid line color.
     * @param spacing Line separation distance.
     * @param size Overall grid area size.
     * @return Spawned grid entity handle.
     */
    Entity spawnGrid(Registry& registry, VulkanRenderer& renderer, const std::string& name, const glm::vec3& position, const glm::vec3& rotation, const glm::vec4& color, float spacing, float size) {
        Entity grid = registry.create();
        if (grid.getId() == Entity::INVALID_ENTITY) {
            return Entity();
        }

        registry.emplace<Transform>(grid, Transform{ position });
        if (Transform* gridTransform = registry.get<Transform>(grid)) {
            gridTransform->rotation = rotation;
        }

        registry.emplace<Name>(grid, Name{ name });
        registry.emplace<PrimitiveType>(grid, PrimitiveType{ PrimitiveKind::Quad });
        registry.emplace<Material>(grid, Material{ color });
        registry.emplace<Mesh>(grid, Primitives::makeQuad());
        registry.emplace<Grid>(grid, Grid{ spacing, size, color });

        PipelineHandle gridPipeline = renderer.createPipelineForShaders(
            "build/shaders/grid.vert.spv",
            "build/shaders/grid.frag.spv"
        );

        if (Material* material = registry.get<Material>(grid)) {
            material->pipeline = gridPipeline.pipeline;
            material->pipelineLayout = gridPipeline.layout;
        }

        uploadMesh(registry, renderer, grid);
        return grid;
    }

    Entity spawnAnimatedModel(Registry& registry, VulkanRenderer& renderer, const std::string& name, const std::string& gltfPath, const glm::vec3& position) {
        Entity entity = registry.create();
        if (entity.getId() == Entity::INVALID_ENTITY) {
            return Entity();
        }

        registry.emplace<Transform>(entity, Transform{ position });
        registry.emplace<Name>(entity, Name{ name });

        // Load Mesh using resource manager
        Mesh mesh = renderer.resourceManager->loadMesh(gltfPath, renderer);
        registry.emplace<Mesh>(entity, std::move(mesh));

        // Check if there is a skeleton and animations
        SkeletonComponent skeleton{};
        AnimatorComponent animator{};
        bool hasSkin = renderer.resourceManager->loadSkeletonAndAnimations(gltfPath, skeleton, animator);

        Material material{ glm::vec4(1.0f) };
        PipelineHandle pipeline;
        if (hasSkin) {
            pipeline = renderer.createPipelineForShaders(
                "build/shaders/skinned.vert.spv",
                "build/shaders/unlit.frag.spv"
            );
            registry.emplace<SkeletonComponent>(entity, std::move(skeleton));
            registry.emplace<AnimatorComponent>(entity, std::move(animator));
        } else {
            pipeline = renderer.createPipelineForShaders(
                "build/shaders/unlit.vert.spv",
                "build/shaders/unlit.frag.spv"
            );
        }
        material.pipeline = pipeline.pipeline;
        material.pipelineLayout = pipeline.layout;
        registry.emplace<Material>(entity, std::move(material));

        return entity;
    }
}
