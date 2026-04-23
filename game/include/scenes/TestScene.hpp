#pragma once

#include "Scene.hpp"

class TestScene : public Scene {
public:
    TestScene(Registry& registry, VulkanRenderer& renderer);

    void load() override;
    void update(float dt) override;
    bool saveToFile(const std::string& path) override;
    bool loadFromFile(const std::string& path) override;

private:
    void createGrid();
    void createCamera();
    void createTriangle();
    void createCube();
    void uploadMesh(Entity entity);
    Entity findEntityByName(const std::string& name);
};
