#include <glfw3.h>
#include <iostream>
#include "renderer/VulkanRenderer.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/systems/RenderSystem.hpp"
#include "ecs/SystemManager.hpp"
#include "ecs/components/primitives.hpp"
#include "ecs/Components/Camera.hpp"
#include "ecs/systems/CameraSystem.hpp"
#include "ecs/components/Grid.hpp"
#include "ecs/systems/InputSystem.hpp"

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "ECS Vulkan Engine", nullptr, nullptr);

    VulkanRenderer renderer(window);
	Registry registry;


    // Create systems
    auto movementSystem = std::make_shared<MovementSystem>(registry);
    auto renderSystem = std::make_shared<RenderSystem>(registry, renderer);
	auto cameraSystem = std::make_shared<CameraSystem>(registry);
	auto inputSystem = std::make_shared<InputSystem>(registry, renderer);

    SystemManager sysManager;
    sysManager.addSystem(inputSystem);
    sysManager.addSystem(cameraSystem);
    sysManager.addSystem(movementSystem);
    sysManager.addSystem(renderSystem);

    Entity grid = registry.create();
    registry.emplace<Transform>(grid,glm::vec3(0.f)); // identity

    Transform gridTransform{};

    gridTransform.rotation = glm::vec3(-90.f, 0.f, 0.f); // rotation in degrees (pitch, yaw, roll)
	registry.get<Transform>(grid)->rotation = gridTransform.rotation;

    registry.emplace<Material>(grid, Material{ glm::vec4(0.3f,0.3f,0.3f,1.f) });
    registry.emplace<Mesh>(grid, Primitives::makeQuad()); // a simple quad, or just 6 vertices
    registry.emplace<Grid>(grid, Grid{ 1.f, 100.f, glm::vec4(0.3f,0.3f,0.3f,1.f) });
    registry.get<Material>(grid)->pipeline =
        renderer.createPipelineForShaders("build/shaders/grid.vert.spv", "build/shaders/grid.frag.spv");



    Entity camEntity = registry.create();
    if (camEntity.getId() == Entity::INVALID_ENTITY) throw std::runtime_error("Ran out of entity IDs");
    registry.emplace<Camera>(camEntity, Camera{});
	registry.emplace<Transform>(camEntity, Transform{});
	registry.emplace<InputComponent>(camEntity, InputComponent{});
    Camera* cam = registry.get<Camera>(camEntity);
    if (!cam) throw std::runtime_error("Camera component missing!");
    if (!camEntity.hasComponent<Transform>()) throw std::runtime_error("Camera entity missing Transform!");
	camEntity.getComponent<Transform>().position = glm::vec3(0.f, 0.f, 5.f);
    cam->fov = 45;
    // Triangle entity
    Entity tri = registry.create();
    registry.emplace<Transform>(tri, glm::vec3(-1.f, 0.f, 0.f));
    registry.emplace<Mesh>(tri, Primitives::makeTriangle()); // centered at origin
    registry.emplace<Material>(tri, Material{ {1.f,0.f,0.f,1.f} });
    registry.get<Material>(tri)->pipeline =
        renderer.createPipelineForShaders("build/shaders/unlit.vert.spv", "build/shaders/unlit.frag.spv");

    // Cube entity
    Entity cube = registry.create();
    registry.emplace<Transform>(cube, glm::vec3(1.f, 0.f, 0.f));
    registry.emplace<Mesh>(cube, Primitives::makeCube()); // centered at origin
    registry.emplace<Material>(cube, Material{ {0.f,1.f,0.f,1.f} });
    registry.get<Material>(cube)->pipeline =
        renderer.createPipelineForShaders("build/shaders/unlit.vert.spv", "build/shaders/unlit.frag.spv");

    // Upload both meshes
    for (auto e : {grid, tri, cube }) {
        Mesh* mesh = registry.get<Mesh>(e);
        if (mesh) renderer.uploadMesh(*mesh);
    }



    while (!renderer.shouldClose()) {
        glfwPollEvents();
        float dt = renderer.getDeltaTime();

		sysManager.updateAll(dt);
    }

    renderer.cleanup();
    glfwTerminate();


    return EXIT_SUCCESS;
}