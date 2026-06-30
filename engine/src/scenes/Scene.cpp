#include "scenes/Scene.hpp"
#include "scenes/SceneSerializer.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/EntityCloner.hpp"
#include "ecs/components/Name.hpp"
#include "renderer/VulkanRenderer.hpp"
#include <algorithm>

Scene::Scene(Registry& registry, VulkanRenderer& renderer)
    : registry(registry), renderer(renderer) {
}

Scene::~Scene() {
}

void Scene::unload() {
    for (Entity entity : ownedEntities) {
        registry.destroy(entity);
    }
    ownedEntities.clear();
}

bool Scene::saveToFile(const std::string& path) {
    SceneSerializer serializer(registry, renderer);
    return serializer.serialize(path, ownedEntities);
}

bool Scene::loadFromFile(const std::string& path) {
    SceneSerializer serializer(registry, renderer);
    unload();
    
    std::vector<Entity> newEntities;
    if (serializer.deserialize(path, newEntities)) {
        for (Entity entity : newEntities) {
            trackEntity(entity);
        }
        return true;
    }
    return false;
}

Entity Scene::createPrimitiveEntity(const std::string& primitiveType) {
    PrimitiveKind kind = PrimitiveKind::Cube;
    std::string baseName = "Cube";

    if (primitiveType == "Triangle") {
        kind = PrimitiveKind::Triangle;
        baseName = "Triangle";
    } else if (primitiveType == "Cube") {
        kind = PrimitiveKind::Cube;
        baseName = "Cube";
    } else if (primitiveType == "Quad") {
        kind = PrimitiveKind::Quad;
        baseName = "Quad";
    } else {
        return Entity();
    }

    Entity entity = EntityFactory::spawnPrimitive(
        registry,
        renderer,
        kind,
        makeUniqueEntityName(baseName),
        glm::vec3(0.0f)
    );
    return trackEntity(entity);
}

Entity Scene::createEntityOfType(const std::string& entityType) {
    Entity entity;
    if (entityType == "Camera") {
        entity = EntityFactory::spawnCamera(
            registry,
            renderer,
            makeUniqueEntityName("Camera"),
            glm::vec3(0.0f, 5.0f, 5.0f),
            glm::vec3(-90.0f, 0.0f, 0.0f),
            45.0f
        );
    } else if (entityType == "Grid") {
        entity = EntityFactory::spawnGrid(
            registry,
            renderer,
            makeUniqueEntityName("Grid"),
            glm::vec3(0.0f),
            glm::vec3(-90.0f, 0.0f, 0.0f),
            glm::vec4(0.3f, 0.3f, 0.3f, 1.0f),
            1.0f,
            100.0f
        );
    } else {
        return Entity();
    }

    return trackEntity(entity);
}

Entity Scene::duplicateEntity(Entity entity) {
    Entity duplicated = EntityCloner::clone(registry, renderer, entity);
    if (duplicated.getId() != Entity::INVALID_ENTITY) {
        trackEntity(duplicated);
        
        // Ensure name is unique
        if (Name* name = registry.get<Name>(duplicated)) {
            name->value = makeUniqueEntityName(name->value);
        }
    }
    return duplicated;
}

bool Scene::deleteEntity(Entity entity) {
    if (entity.getId() == Entity::INVALID_ENTITY) {
        return false;
    }
    untrackEntity(entity);
    registry.destroy(entity);
    return true;
}

Entity Scene::trackEntity(Entity entity) {
    ownedEntities.push_back(entity);
    return entity;
}

void Scene::untrackEntity(Entity entity) {
    ownedEntities.erase(
        std::remove(ownedEntities.begin(), ownedEntities.end(), entity),
        ownedEntities.end()
    );
}

Entity Scene::findEntityByName(const std::string& name) const {
    for (auto [entity, entityName] : registry.view<Name>()) {
        if (entityName.value == name) {
            return entity;
        }
    }
    return Entity();
}

std::string Scene::makeUniqueEntityName(const std::string& baseName) const {
    if (findEntityByName(baseName).getId() == Entity::INVALID_ENTITY) {
        return baseName;
    }

    int suffix = 1;
    while (true) {
        std::string candidate = baseName + " " + std::to_string(suffix);
        if (findEntityByName(candidate).getId() == Entity::INVALID_ENTITY) {
            return candidate;
        }
        ++suffix;
    }
}
