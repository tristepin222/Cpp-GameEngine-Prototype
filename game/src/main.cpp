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

    renderer.createInstanceBuffer(10000);
    // Create systems
    auto movementSystem = std::make_shared<MovementSystem>(registry);
    auto renderSystem = std::make_shared<RenderSystem>(registry, renderer);
	auto cameraSystem = std::make_shared<CameraSystem>(registry, renderer);
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

    auto ph = renderer.createPipelineForShaders("build/shaders/grid.vert.spv",
        "build/shaders/grid.frag.spv");

    Material* mat = registry.get<Material>(grid);
    mat->pipeline = ph.pipeline;
    mat->pipelineLayout = ph.layout;



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

    renderer.transformSoA.positions.push_back(camEntity.getComponent<Transform>().position);
    renderer.transformSoA.rotations.push_back(camEntity.getComponent<Transform>().rotation);
    renderer.transformSoA.scales.push_back(camEntity.getComponent<Transform>().scale);
    size_t index = renderer.cameraSoA.pushCamera(cam->fov, cam->aspect, cam->nearPlane, cam->farPlane);
    camEntity.getComponent<Transform>().soaIndex = index;
    // Triangle entity
    Entity tri = registry.create();
    registry.emplace<Transform>(tri, glm::vec3(-1.f, 0.f, 0.f));
    registry.emplace<Mesh>(tri, Primitives::makeTriangle()); // centered at origin
    registry.emplace<Material>(tri, Material{ {1.f, 0.f, 0.f, 1.f} });

    // Use helper to get both pipeline and layout
    PipelineHandle triPipeline = renderer.createPipelineForShaders(
        "build/shaders/unlit.vert.spv",
        "build/shaders/unlit.frag.spv"
    );

    Material* triMat = registry.get<Material>(tri);
    triMat->pipeline = triPipeline.pipeline;
    triMat->pipelineLayout = triPipeline.layout;

    // Cube entity
    Entity cube = registry.create();
    registry.emplace<Transform>(cube, glm::vec3(1.f, 0.f, 0.f));
    registry.emplace<Mesh>(cube, Primitives::makeCube()); // centered at origin
    registry.emplace<Material>(cube, Material{ {0.f, 1.f, 0.f, 1.f} });

    PipelineHandle cubePipeline = renderer.createPipelineForShaders(
        "build/shaders/unlit.vert.spv",
        "build/shaders/unlit.frag.spv"
    );

    Material* cubeMat = registry.get<Material>(cube);
    cubeMat->pipeline = cubePipeline.pipeline;
    cubeMat->pipelineLayout = cubePipeline.layout;

    // Upload both meshes
    for (auto e : { grid, tri, cube }) {
        Mesh* mesh = registry.get<Mesh>(e);
        if (mesh) {
            size_t meshID = renderer.meshSoA.push(mesh->vertices, mesh->indices);
            renderer.uploadMesh(meshID); // now it matches your new function


            mesh->vertexBuffer = renderer.meshSoA.vertexBuffers[meshID].get(); // get VkBuffer
            mesh->indexBuffer = renderer.meshSoA.indexBuffers[meshID].get();  // get VkBuffer
        }
    }



    while (!renderer.shouldClose()) {
        glfwPollEvents();
        float dt = renderer.getDeltaTime();

		sysManager.updateAll(dt);
        renderSystem->drawFrame();
    }

    renderer.cleanup();
    glfwTerminate();


    return EXIT_SUCCESS;
}
