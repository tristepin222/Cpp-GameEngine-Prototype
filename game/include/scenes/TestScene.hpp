#pragma once

#include <glm/glm.hpp>

#include "Scene.hpp"
#include "../ecs/components/PrimitiveType.hpp"

class TestScene : public Scene {
public:
    TestScene(Registry& registry, VulkanRenderer& renderer);

    void load() override;
    void update(float dt) override;
    bool saveToFile(const std::string& path) override;
    bool loadFromFile(const std::string& path) override;
    Entity createPrimitiveEntity(const std::string& primitiveType) override;
    Entity createEntityOfType(const std::string& entityType) override;
    Entity duplicateEntity(Entity entity) override;
    bool deleteEntity(Entity entity) override;

private:
    void createGrid();
    void createCamera();
    void createTriangle();
    void createCube();
    Entity createPrimitiveEntity(PrimitiveKind kind, const std::string& name, const glm::vec3& position);
    Entity createCameraEntity(const std::string& name, const glm::vec3& position, const glm::vec3& rotation, float fov);
    Entity createGridEntity(const std::string& name, const glm::vec3& position, const glm::vec3& rotation, const glm::vec4& color, float spacing, float size);
    void uploadMesh(Entity entity);
    Entity findEntityByName(const std::string& name) const;
    std::string makeUniqueEntityName(const std::string& baseName) const;
};
