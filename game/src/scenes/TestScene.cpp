#include "scenes/TestScene.hpp"

#include <stdexcept>

#include "ecs/Registry.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/Grid.hpp"
#include "ecs/components/Material.hpp"
#include "ecs/components/Mesh.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/inputComponent.hpp"
#include "ecs/components/primitives.hpp"
#include "renderer/VulkanRenderer.hpp"

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
    Entity grid = trackEntity(registry.create());
    registry.emplace<Transform>(grid, Transform{ glm::vec3(0.0f) });

    Transform* gridTransform = registry.get<Transform>(grid);
    gridTransform->rotation = glm::vec3(-90.0f, 0.0f, 0.0f);

    registry.emplace<Material>(grid, Material{ glm::vec4(0.3f, 0.3f, 0.3f, 1.0f) });
    registry.emplace<Mesh>(grid, Primitives::makeQuad());
    registry.emplace<Grid>(grid, Grid{ 1.0f, 100.0f, glm::vec4(0.3f, 0.3f, 0.3f, 1.0f) });

    PipelineHandle gridPipeline = renderer.createPipelineForShaders(
        "build/shaders/grid.vert.spv",
        "build/shaders/grid.frag.spv"
    );

    Material* material = registry.get<Material>(grid);
    material->pipeline = gridPipeline.pipeline;
    material->pipelineLayout = gridPipeline.layout;

    uploadMesh(grid);
}

void TestScene::createCamera() {
    Entity cameraEntity = trackEntity(registry.create());
    if (cameraEntity.getId() == Entity::INVALID_ENTITY) {
        throw std::runtime_error("Ran out of entity IDs");
    }

    registry.emplace<Camera>(cameraEntity, Camera{});
    registry.emplace<Transform>(cameraEntity, Transform{});
    registry.emplace<InputComponent>(cameraEntity, InputComponent{});

    Camera* camera = registry.get<Camera>(cameraEntity);
    Transform* transform = registry.get<Transform>(cameraEntity);

    if (!camera || !transform) {
        throw std::runtime_error("Camera setup failed");
    }

    transform->position = glm::vec3(0.0f, 5.0f, 5.0f);
    transform->rotation = glm::vec3(-90.0f, 0.0f, 0.0f);
    camera->fov = 45.0f;

    int width = 0;
    int height = 0;
    glfwGetWindowSize(renderer.getWindow(), &width, &height);
    camera->aspect = static_cast<float>(width) / static_cast<float>(height);
    renderer.setActiveCamera(camera->viewProjection(*transform), transform->position);
}

void TestScene::createTriangle() {
    Entity triangle = trackEntity(registry.create());
    registry.emplace<Transform>(triangle, Transform{ glm::vec3(-1.0f, 0.0f, 0.0f) });
    registry.emplace<Mesh>(triangle, Primitives::makeTriangle());
    registry.emplace<Material>(triangle, Material{ {1.0f, 0.0f, 0.0f, 1.0f} });

    PipelineHandle trianglePipeline = renderer.createPipelineForShaders(
        "build/shaders/unlit.vert.spv",
        "build/shaders/unlit.frag.spv"
    );

    Material* material = registry.get<Material>(triangle);
    material->pipeline = trianglePipeline.pipeline;
    material->pipelineLayout = trianglePipeline.layout;

    uploadMesh(triangle);
}

void TestScene::createCube() {
    Entity cube = trackEntity(registry.create());
    registry.emplace<Transform>(cube, Transform{ glm::vec3(1.0f, 0.0f, 0.0f) });
    registry.emplace<Mesh>(cube, Primitives::makeCube());
    registry.emplace<Material>(cube, Material{ {0.0f, 1.0f, 0.0f, 1.0f} });

    PipelineHandle cubePipeline = renderer.createPipelineForShaders(
        "build/shaders/unlit.vert.spv",
        "build/shaders/unlit.frag.spv"
    );

    Material* material = registry.get<Material>(cube);
    material->pipeline = cubePipeline.pipeline;
    material->pipelineLayout = cubePipeline.layout;

    uploadMesh(cube);
}

void TestScene::uploadMesh(Entity entity) {
    Mesh* mesh = registry.get<Mesh>(entity);
    if (!mesh) {
        return;
    }

    const size_t meshID = renderer.meshSoA.push(mesh->vertices, mesh->indices);
    renderer.uploadMesh(meshID);

    mesh->vertexBuffer = renderer.meshSoA.vertexBuffers[meshID].get();
    mesh->indexBuffer = renderer.meshSoA.indexBuffers[meshID].get();
}
