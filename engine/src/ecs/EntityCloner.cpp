#include "ecs/EntityCloner.hpp"
#include "ecs/EntityFactory.hpp"
#include "renderer/VulkanRenderer.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/Name.hpp"
#include "ecs/components/PrimitiveType.hpp"
#include "ecs/components/Mesh.hpp"
#include "ecs/components/Material.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/Grid.hpp"
#include "ecs/components/Skeleton.hpp"
#include "renderer/ResourceManager.hpp"

/**
 * @namespace EntityCloner
 * @brief Utility functions for duplicating entities and their components.
 */
namespace EntityCloner {

    /**
     * @brief Clones a source entity and offsets its position slightly.
     * @param registry ECS registry reference.
     * @param renderer Vulkan renderer.
     * @param source The target entity to clone.
     * @return Offset clone entity.
     */
    Entity clone(Registry& registry, VulkanRenderer& renderer, Entity source) {
        if (source.getId() == Entity::INVALID_ENTITY) {
            return Entity();
        }

        Name* name = registry.get<Name>(source);
        Transform* transform = registry.get<Transform>(source);
        if (!name || !transform) {
            return Entity();
        }

        const std::string cloneName = name->value + " Copy";
        const glm::vec3 offsetPosition = transform->position + glm::vec3(1.0f, 0.0f, 1.0f);

        if (Camera* camera = registry.get<Camera>(source)) {
            Entity duplicated = EntityFactory::spawnCamera(registry, renderer, cloneName, offsetPosition, transform->rotation, camera->fov);
            if (Camera* duplicatedCamera = registry.get<Camera>(duplicated)) {
                duplicatedCamera->nearPlane = camera->nearPlane;
                duplicatedCamera->farPlane = camera->farPlane;
                duplicatedCamera->moveSpeed = camera->moveSpeed;
                duplicatedCamera->mouseSensitivity = camera->mouseSensitivity;
            }
            return duplicated;
        }

        if (Grid* grid = registry.get<Grid>(source)) {
            return EntityFactory::spawnGrid(
                registry,
                renderer,
                cloneName,
                offsetPosition,
                transform->rotation,
                grid->color,
                grid->spacing,
                grid->size
            );
        }

        PrimitiveKind kind = PrimitiveKind::Cube;
        if (PrimitiveType* primitive = registry.get<PrimitiveType>(source)) {
            kind = primitive->kind;
        }

        Entity duplicated = EntityFactory::spawnPrimitive(registry, renderer, kind, cloneName, offsetPosition);
        if (Transform* duplicatedTransform = registry.get<Transform>(duplicated)) {
            duplicatedTransform->rotation = transform->rotation;
            duplicatedTransform->scale = transform->scale;
        }
        if (Material* sourceMaterial = registry.get<Material>(source)) {
            if (Material* duplicatedMaterial = registry.get<Material>(duplicated)) {
                duplicatedMaterial->color = sourceMaterial->color;
                duplicatedMaterial->texturePath = sourceMaterial->texturePath;
                duplicatedMaterial->normalMapPath = sourceMaterial->normalMapPath;
                duplicatedMaterial->metallicMapPath = sourceMaterial->metallicMapPath;
                duplicatedMaterial->shaderName = sourceMaterial->shaderName;
                duplicatedMaterial->roughness = sourceMaterial->roughness;
                duplicatedMaterial->metallic = sourceMaterial->metallic;
                duplicatedMaterial->filterMode = sourceMaterial->filterMode;

                // Recreate descriptor set for duplicate
                renderer.resourceManager->updateMaterialDescriptorSet(*duplicatedMaterial, renderer);

                // Recreate pipeline for duplicate based on shaderName and skin (no skin for primitives cloned here)
                bool hasSkin = registry.has<SkeletonComponent>(duplicated);
                std::string vertShader = "unlit.vert.spv";
                std::string fragShader = "unlit.frag.spv";
                if (duplicatedMaterial->shaderName == "Lit") {
                    vertShader = hasSkin ? "skinned_lit.vert.spv" : "lit.vert.spv";
                    fragShader = "lit.frag.spv";
                } else {
                    vertShader = hasSkin ? "skinned.vert.spv" : "unlit.vert.spv";
                    fragShader = "unlit.frag.spv";
                }

                PipelineHandle pipeline = renderer.createPipelineForShaders(
                    renderer.resolveShaderPath("build/shaders/" + vertShader),
                    renderer.resolveShaderPath("build/shaders/" + fragShader)
                );
                duplicatedMaterial->pipeline = pipeline.pipeline;
                duplicatedMaterial->pipelineLayout = pipeline.layout;
            }
        }

        return duplicated;
    }
}
