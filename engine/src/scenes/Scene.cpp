#include "scenes/Scene.hpp"
#include "scenes/SceneSerializer.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/EntityCloner.hpp"
#include "ecs/components/Name.hpp"
#include "ecs/components/Hierarchy.hpp"
#include "renderer/VulkanRenderer.hpp"
#include <algorithm>
#include <filesystem>

/**
 * @brief Construct a new Scene:: Scene object.
 * @param registry Reference to ECS registry.
 * @param renderer Reference to renderer.
 */
Scene::Scene(Registry& registry, VulkanRenderer& renderer)
    : registry(registry), renderer(renderer) {
}

/**
 * @brief Destroy the Scene:: Scene object.
 */
Scene::~Scene() {
}

/**
 * @brief Destroys all entities tracked by this scene.
 */
void Scene::unload() {
    for (Entity entity : ownedEntities) {
        registry.destroy(entity);
    }
    ownedEntities.clear();
}

/**
 * @brief Serializes scene entities to file.
 * @param path Output path.
 * @return True if successful, false otherwise.
 */
bool Scene::saveToFile(const std::string& path) {
    SceneSerializer serializer(registry, renderer);
    bool ok = serializer.serialize(path, ownedEntities);
    if (ok) {
        std::filesystem::path p(path);
        std::string pathStr = p.generic_string();
        
        if (pathStr.rfind("./", 0) == 0) {
            pathStr = pathStr.substr(2);
        }
        
        if (pathStr.rfind("assets/", 0) == 0) {
            std::string sourcePath = "../../../sandbox_game/" + pathStr;
            std::filesystem::path sourceDirPath = std::filesystem::path(sourcePath).parent_path();
            if (std::filesystem::exists("../../../sandbox_game/assets")) {
                std::filesystem::create_directories(sourceDirPath);
                serializer.serialize(sourcePath, ownedEntities);
            }
        }
    }
    return ok;
}

/**
 * @brief Deserializes and loads scene entities from file.
 * @param path Input path.
 * @return True if successful, false otherwise.
 */
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

/**
 * @brief Spawns a primitive shape entity.
 * @param primitiveType Shape type string ("Triangle", "Cube", "Quad").
 * @return Spawned entity handle.
 */
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

/**
 * @brief Spawns a component helper entity ("Camera", "Grid").
 * @param entityType Type string.
 * @return Spawned entity handle.
 */
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

/**
 * @brief Clones the target entity.
 * @param entity Entity to clone.
 * @return Duplicated entity handle.
 */
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

/**
 * @brief Deletes and destroys the target entity.
 * @param entity Entity to delete.
 * @return True if successful, false otherwise.
 */
bool Scene::deleteEntity(Entity entity) {
    if (entity.getId() == Entity::INVALID_ENTITY) {
        return false;
    }
    
    // Find all children recursively
    std::vector<Entity> toDelete;
    toDelete.push_back(entity);
    
    size_t cursor = 0;
    while (cursor < toDelete.size()) {
        Entity current = toDelete[cursor];
        for (auto [childEntity, hierarchy] : registry.view<HierarchyComponent>()) {
            if (hierarchy.parent == current) {
                if (std::find(toDelete.begin(), toDelete.end(), childEntity) == toDelete.end()) {
                    toDelete.push_back(childEntity);
                }
            }
        }
        cursor++;
    }
    
    // Destroy all of them
    for (Entity e : toDelete) {
        untrackEntity(e);
        registry.destroy(e);
    }
    
    return true;
}

/**
 * @brief Registers entity under scene tracking list.
 * @param entity Target entity.
 * @return Tracked entity.
 */
Entity Scene::trackEntity(Entity entity) {
    ownedEntities.push_back(entity);
    return entity;
}

/**
 * @brief Removes entity from scene tracking list.
 * @param entity Target entity.
 */
void Scene::untrackEntity(Entity entity) {
    ownedEntities.erase(
        std::remove(ownedEntities.begin(), ownedEntities.end(), entity),
        ownedEntities.end()
    );
}

/**
 * @brief Resolves active entity matching name property.
 * @param name Search name query.
 * @return Entity handle.
 */
Entity Scene::findEntityByName(const std::string& name) const {
    for (auto [entity, entityName] : registry.view<Name>()) {
        if (entityName.value == name) {
            return entity;
        }
    }
    return Entity();
}

/**
 * @brief Creates unique suffix indices if name conflicts.
 * @param baseName Desired name.
 * @return Unique name.
 */
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
