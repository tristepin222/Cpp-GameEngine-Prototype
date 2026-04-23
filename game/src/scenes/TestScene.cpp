#include "scenes/TestScene.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include "ecs/Registry.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/Grid.hpp"
#include "ecs/components/Material.hpp"
#include "ecs/components/Mesh.hpp"
#include "ecs/components/Name.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/inputComponent.hpp"
#include "ecs/components/primitives.hpp"
#include "renderer/VulkanRenderer.hpp"

namespace {
    std::string indent(int level) {
        return std::string(static_cast<size_t>(level) * 2, ' ');
    }

    std::string quote(const std::string& value) {
        return "\"" + value + "\"";
    }

    std::string vec3ToJson(const glm::vec3& v) {
        std::ostringstream out;
        out << "[" << v.x << ", " << v.y << ", " << v.z << "]";
        return out.str();
    }

    std::string vec4ToJson(const glm::vec4& v) {
        std::ostringstream out;
        out << "[" << v.r << ", " << v.g << ", " << v.b << ", " << v.a << "]";
        return out.str();
    }

    std::string extractStringValue(const std::string& source, const std::string& key) {
        const std::string token = "\"" + key + "\"";
        size_t keyPos = source.find(token);
        if (keyPos == std::string::npos) {
            return {};
        }

        size_t colonPos = source.find(':', keyPos);
        size_t firstQuote = source.find('"', colonPos + 1);
        size_t secondQuote = source.find('"', firstQuote + 1);
        if (colonPos == std::string::npos || firstQuote == std::string::npos || secondQuote == std::string::npos) {
            return {};
        }

        return source.substr(firstQuote + 1, secondQuote - firstQuote - 1);
    }

    bool extractFloatArray(const std::string& source, const std::string& key, float* values, size_t count) {
        const std::string token = "\"" + key + "\"";
        size_t keyPos = source.find(token);
        if (keyPos == std::string::npos) {
            return false;
        }

        size_t open = source.find('[', keyPos);
        size_t close = source.find(']', open);
        if (open == std::string::npos || close == std::string::npos) {
            return false;
        }

        std::string payload = source.substr(open + 1, close - open - 1);
        for (char& c : payload) {
            if (c == ',') {
                c = ' ';
            }
        }

        std::istringstream stream(payload);
        for (size_t i = 0; i < count; ++i) {
            if (!(stream >> values[i])) {
                return false;
            }
        }

        return true;
    }

    bool extractFloatValue(const std::string& source, const std::string& key, float& value) {
        const std::string token = "\"" + key + "\"";
        size_t keyPos = source.find(token);
        if (keyPos == std::string::npos) {
            return false;
        }

        size_t colonPos = source.find(':', keyPos);
        if (colonPos == std::string::npos) {
            return false;
        }

        std::string payload = source.substr(colonPos + 1);
        std::istringstream stream(payload);
        return static_cast<bool>(stream >> value);
    }

    std::vector<std::string> extractEntityObjects(const std::string& source) {
        std::vector<std::string> objects;
        size_t entitiesPos = source.find("\"entities\"");
        if (entitiesPos == std::string::npos) {
            return objects;
        }

        size_t arrayOpen = source.find('[', entitiesPos);
        if (arrayOpen == std::string::npos) {
            return objects;
        }

        int arrayDepth = 0;
        size_t arrayClose = std::string::npos;
        for (size_t i = arrayOpen; i < source.size(); ++i) {
            if (source[i] == '[') {
                ++arrayDepth;
            } else if (source[i] == ']') {
                --arrayDepth;
                if (arrayDepth == 0) {
                    arrayClose = i;
                    break;
                }
            }
        }

        if (arrayClose == std::string::npos) {
            return objects;
        }

        size_t pos = arrayOpen + 1;
        while (pos < arrayClose) {
            size_t objectStart = source.find('{', pos);
            if (objectStart == std::string::npos || objectStart > arrayClose) {
                break;
            }

            int depth = 0;
            size_t objectEnd = objectStart;
            for (; objectEnd < source.size(); ++objectEnd) {
                if (source[objectEnd] == '{') {
                    ++depth;
                } else if (source[objectEnd] == '}') {
                    --depth;
                    if (depth == 0) {
                        break;
                    }
                }
            }

            if (depth == 0 && objectEnd < source.size()) {
                objects.push_back(source.substr(objectStart, objectEnd - objectStart + 1));
            }

            pos = objectEnd + 1;
        }

        return objects;
    }
}

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

bool TestScene::saveToFile(const std::string& path) {
    std::filesystem::path outputPath(path);
    if (outputPath.has_parent_path()) {
        std::filesystem::create_directories(outputPath.parent_path());
    }

    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "{\n";
    out << indent(1) << "\"scene\": " << quote("TestScene") << ",\n";
    out << indent(1) << "\"entities\": [\n";

    bool first = true;
    for (auto [entity, name, transform] : registry.view<Name, Transform>()) {
        if (!first) {
            out << ",\n";
        }
        first = false;

        out << indent(2) << "{\n";
        out << indent(3) << "\"name\": " << quote(name.value) << ",\n";
        out << indent(3) << "\"position\": " << vec3ToJson(transform.position) << ",\n";
        out << indent(3) << "\"rotation\": " << vec3ToJson(transform.rotation) << ",\n";
        out << indent(3) << "\"scale\": " << vec3ToJson(transform.scale);

        if (auto* material = registry.get<Material>(entity)) {
            out << ",\n" << indent(3) << "\"color\": " << vec4ToJson(material->color);
        }

        if (auto* grid = registry.get<Grid>(entity)) {
            out << ",\n" << indent(3) << "\"gridSpacing\": " << grid->spacing;
            out << ",\n" << indent(3) << "\"gridSize\": " << grid->size;
        }

        if (auto* camera = registry.get<Camera>(entity)) {
            out << ",\n" << indent(3) << "\"fov\": " << camera->fov;
        }

        out << "\n" << indent(2) << "}";
    }

    out << "\n" << indent(1) << "]\n";
    out << "}\n";
    return true;
}

bool TestScene::loadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string source = buffer.str();

    bool appliedAny = false;
    for (const std::string& entityJson : extractEntityObjects(source)) {
        const std::string name = extractStringValue(entityJson, "name");
        if (name.empty()) {
            continue;
        }

        Entity entity = findEntityByName(name);
        if (entity.getId() == Entity::INVALID_ENTITY) {
            continue;
        }

        if (Transform* transform = registry.get<Transform>(entity)) {
            float position[3]{};
            float rotation[3]{};
            float scale[3]{};

            if (extractFloatArray(entityJson, "position", position, 3)) {
                transform->position = glm::vec3(position[0], position[1], position[2]);
                appliedAny = true;
            }
            if (extractFloatArray(entityJson, "rotation", rotation, 3)) {
                transform->rotation = glm::vec3(rotation[0], rotation[1], rotation[2]);
                appliedAny = true;
            }
            if (extractFloatArray(entityJson, "scale", scale, 3)) {
                transform->scale = glm::vec3(scale[0], scale[1], scale[2]);
                appliedAny = true;
            }
        }

        if (Material* material = registry.get<Material>(entity)) {
            float color[4]{};
            if (extractFloatArray(entityJson, "color", color, 4)) {
                material->color = glm::vec4(color[0], color[1], color[2], color[3]);
                appliedAny = true;
            }
        }

        if (Grid* grid = registry.get<Grid>(entity)) {
            float spacing = 0.0f;
            float size = 0.0f;
            if (extractFloatValue(entityJson, "gridSpacing", spacing)) {
                grid->spacing = spacing;
                appliedAny = true;
            }
            if (extractFloatValue(entityJson, "gridSize", size)) {
                grid->size = size;
                appliedAny = true;
            }
        }

        if (Camera* camera = registry.get<Camera>(entity)) {
            float fov = 0.0f;
            if (extractFloatValue(entityJson, "fov", fov)) {
                camera->fov = fov;
                appliedAny = true;
            }

            if (Transform* transform = registry.get<Transform>(entity)) {
                renderer.setActiveCamera(camera->viewProjection(*transform), transform->position);
            }
        }
    }

    return appliedAny;
}

void TestScene::createGrid() {
    Entity grid = trackEntity(registry.create());
    registry.emplace<Transform>(grid, Transform{ glm::vec3(0.0f) });

    Transform* gridTransform = registry.get<Transform>(grid);
    gridTransform->rotation = glm::vec3(-90.0f, 0.0f, 0.0f);

    registry.emplace<Material>(grid, Material{ glm::vec4(0.3f, 0.3f, 0.3f, 1.0f) });
    registry.emplace<Mesh>(grid, Primitives::makeQuad());
    registry.emplace<Grid>(grid, Grid{ 1.0f, 100.0f, glm::vec4(0.3f, 0.3f, 0.3f, 1.0f) });
    registry.emplace<Name>(grid, Name{ "Grid" });

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
    registry.emplace<Name>(cameraEntity, Name{ "Camera" });

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
    registry.emplace<Name>(triangle, Name{ "Triangle" });

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
    registry.emplace<Name>(cube, Name{ "Cube" });

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

Entity TestScene::findEntityByName(const std::string& name) {
    for (auto [entity, entityName] : registry.view<Name>()) {
        if (entityName.value == name) {
            return entity;
        }
    }

    return Entity();
}
