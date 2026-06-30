#include "scenes/TestScene.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/components/Material.hpp"
#include <glm/glm.hpp>

TestScene::TestScene(Registry& registry, VulkanRenderer& renderer)
    : Scene(registry, renderer) {
}

void TestScene::load() {
    createGrid();
    createCamera();
    createTriangle();
    createCube();
}

void TestScene::update(float dt) {
    (void)dt;
}

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
    trackEntity(cube);
}
