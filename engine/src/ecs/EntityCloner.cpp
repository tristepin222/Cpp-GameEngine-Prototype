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

namespace EntityCloner {

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
            }
        }

        return duplicated;
    }
}
