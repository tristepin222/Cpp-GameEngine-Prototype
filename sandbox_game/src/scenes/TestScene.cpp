#include "scenes/TestScene.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/components/Material.hpp"
#include "GameMetadataComponent.hpp"
#include <glm/glm.hpp>

/**
 * @brief Construct a new Test Scene:: Test Scene object.
 * @param registry Reference to ECS registry.
 * @param renderer Reference to renderer.
 */
TestScene::TestScene(Registry& registry, VulkanRenderer& renderer)
    : Scene(registry, renderer) {
}

/**
 * @brief Spawns default entities like grid, camera, triangle, and cube.
 */
void TestScene::load() {
    createGrid();
    createCamera();
    createTriangle();
    createCube();
}

/**
 * @brief Updates scene state.
 * @param dt Elapsed frame time.
 */
void TestScene::update(float dt) {
    (void)dt;
}

/**
 * @brief Spawns grid entity.
 */
void TestScene::createGrid() {
    trackEntity(EntityFactory::spawnGrid(
        registry,
        renderer,
        "Grid",
        glm::vec3(0.0f),
        glm::vec3(-90.0f, 0.0f, 0.0f),
        glm::vec4(0.3f, 0.3f, 0.3f, 1.0f),
        1.0f,
        100.0f
    ));
}

/**
 * @brief Spawns camera entity.
 */
void TestScene::createCamera() {
    trackEntity(EntityFactory::spawnCamera(
        registry,
        renderer,
        "Camera",
        glm::vec3(0.0f, 5.0f, 5.0f),
        glm::vec3(-90.0f, 0.0f, 0.0f),
        45.0f
    ));
}

/**
 * @brief Spawns triangle primitive entity.
 */
void TestScene::createTriangle() {
    Entity triangle = EntityFactory::spawnPrimitive(
        registry,
        renderer,
        PrimitiveKind::Triangle,
        "Triangle",
        glm::vec3(-1.0f, 0.0f, 0.0f)
    );
    if (Material* material = registry.get<Material>(triangle)) {
        material->color = { 1.0f, 0.0f, 0.0f, 1.0f };
    }
    trackEntity(triangle);
}

/**
 * @brief Spawns cube primitive entity.
 */
void TestScene::createCube() {
    Entity cube = EntityFactory::spawnPrimitive(
        registry,
        renderer,
        PrimitiveKind::Cube,
        "Cube",
        glm::vec3(1.0f, 0.0f, 0.0f)
    );
    if (Material* material = registry.get<Material>(cube)) {
        material->color = { 0.0f, 1.0f, 0.0f, 1.0f };
    }
    
    // Attach custom GameMetadataComponent!
    registry.emplace<GameMetadataComponent>(cube, GameMetadataComponent{ 99.0f, "Boss" });
    
    trackEntity(cube);
}
