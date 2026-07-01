#pragma once

#include "scenes/Scene.hpp"

/**
 * @class TestScene
 * @brief Concrete implementation of a test level scene.
 */
class TestScene : public Scene {
public:
    /**
     * @brief Construct a new Test Scene object.
     * @param registry Reference to ECS registry.
     * @param renderer Reference to renderer.
     */
    TestScene(Registry& registry, VulkanRenderer& renderer);

    /**
     * @brief Loads initial scene configurations.
     */
    void load() override;
    /**
     * @brief Updates the scene simulation.
     * @param dt Elapsed frame time.
     */
    void update(float dt) override;

private:
    /**
     * @brief Spawns grid elements.
     */
    void createGrid();
    /**
     * @brief Spawns active viewer camera.
     */
    void createCamera();
    /**
     * @brief Spawns a demo triangle entity.
     */
    void createTriangle();
    /**
     * @brief Spawns a demo cube entity.
     */
    void createCube();
};
