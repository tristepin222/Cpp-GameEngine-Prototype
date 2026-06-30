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
#include "ecs/components/PrimitiveType.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/inputComponent.hpp"
#include "ecs/components/primitives.hpp"
#include "renderer/VulkanRenderer.hpp"

namespace {
    std::string primitiveKindToString(PrimitiveKind kind) {
        switch (kind) {
        case PrimitiveKind::Triangle: return "Triangle";
        case PrimitiveKind::Cube: return "Cube";
        case PrimitiveKind::Quad: return "Quad";
        default: return "Triangle";
        }
    }

    bool tryParsePrimitiveKind(const std::string& value, PrimitiveKind& kind) {
        if (value == "Triangle") {
            kind = PrimitiveKind::Triangle;
            return true;
        }
        if (value == "Cube") {
            kind = PrimitiveKind::Cube;
            return true;
        }
        if (value == "Quad") {
            kind = PrimitiveKind::Quad;
            return true;
        }
        return false;
    }

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

        if (auto* primitive = registry.get<PrimitiveType>(entity)) {
            out << ",\n" << indent(3) << "\"entityType\": " << quote("Primitive");
            out << ",\n" << indent(3) << "\"primitive\": " << quote(primitiveKindToString(primitive->kind));
        }

        if (auto* material = registry.get<Material>(entity)) {
            out << ",\n" << indent(3) << "\"color\": " << vec4ToJson(material->color);
        }

        if (auto* grid = registry.get<Grid>(entity)) {
            out << ",\n" << indent(3) << "\"entityType\": " << quote("Grid");
            out << ",\n" << indent(3) << "\"gridSpacing\": " << grid->spacing;
            out << ",\n" << indent(3) << "\"gridSize\": " << grid->size;
        }

        if (auto* camera = registry.get<Camera>(entity)) {
            out << ",\n" << indent(3) << "\"entityType\": " << quote("Camera");
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

    struct SerializedEntity {
        std::string entityType;
        std::string primitive;
        std::string name;
        glm::vec3 position{ 0.0f };
        glm::vec3 rotation{ 0.0f };
        glm::vec3 scale{ 1.0f };
        glm::vec4 color{ 1.0f };
        float gridSpacing = 1.0f;
        float gridSize = 100.0f;
        float fov = 45.0f;
        bool hasColor = false;
        bool hasGrid = false;
        bool hasFov = false;
    };

    std::vector<SerializedEntity> entitiesToCreate;
    for (const std::string& entityJson : extractEntityObjects(source)) {
        SerializedEntity data;
        data.name = extractStringValue(entityJson, "name");
        data.entityType = extractStringValue(entityJson, "entityType");
        data.primitive = extractStringValue(entityJson, "primitive");

        float position[3]{};
        float rotation[3]{};
        float scale[3]{};
        float color[4]{};

        if (extractFloatArray(entityJson, "position", position, 3)) {
            data.position = glm::vec3(position[0], position[1], position[2]);
        }
        if (extractFloatArray(entityJson, "rotation", rotation, 3)) {
            data.rotation = glm::vec3(rotation[0], rotation[1], rotation[2]);
        }
        if (extractFloatArray(entityJson, "scale", scale, 3)) {
            data.scale = glm::vec3(scale[0], scale[1], scale[2]);
        }
        if (extractFloatArray(entityJson, "color", color, 4)) {
            data.color = glm::vec4(color[0], color[1], color[2], color[3]);
            data.hasColor = true;
        }
        if (extractFloatValue(entityJson, "gridSpacing", data.gridSpacing)) {
            data.hasGrid = true;
        }
        if (extractFloatValue(entityJson, "gridSize", data.gridSize)) {
            data.hasGrid = true;
        }
        if (extractFloatValue(entityJson, "fov", data.fov)) {
            data.hasFov = true;
        }

        if (!data.name.empty()) {
            entitiesToCreate.push_back(data);
        }
    }

    if (entitiesToCreate.empty()) {
        return false;
    }

    unload();

    bool createdAny = false;
    for (const SerializedEntity& data : entitiesToCreate) {
        Entity entity;

        if (data.entityType == "Camera" || data.hasFov) {
            entity = createCameraEntity(data.name, data.position, data.rotation, data.fov);
        } else if (data.entityType == "Grid" || data.hasGrid) {
            entity = createGridEntity(
                data.name,
                data.position,
                data.rotation,
                data.hasColor ? data.color : glm::vec4(0.3f, 0.3f, 0.3f, 1.0f),
                data.gridSpacing,
                data.gridSize
            );
        } else {
            PrimitiveKind kind = PrimitiveKind::Cube;
            if (!data.primitive.empty()) {
                tryParsePrimitiveKind(data.primitive, kind);
            } else if (data.name == "Triangle") {
                kind = PrimitiveKind::Triangle;
            } else if (data.name == "Grid") {
                kind = PrimitiveKind::Quad;
            }

            entity = createPrimitiveEntity(kind, data.name, data.position);
            if (Transform* transform = registry.get<Transform>(entity)) {
                transform->rotation = data.rotation;
                transform->scale = data.scale;
            }
            if (Material* material = registry.get<Material>(entity); material && data.hasColor) {
                material->color = data.color;
            }
        }

        if (entity.getId() != Entity::INVALID_ENTITY) {
            createdAny = true;
        }
    }

    return createdAny;
}

void TestScene::createGrid() {
    createGridEntity(
        "Grid",
        glm::vec3(0.0f),
        glm::vec3(-90.0f, 0.0f, 0.0f),
        glm::vec4(0.3f, 0.3f, 0.3f, 1.0f),
        1.0f,
        100.0f
    );
}

void TestScene::createCamera() {
    createCameraEntity(
        "Camera",
        glm::vec3(0.0f, 5.0f, 5.0f),
        glm::vec3(-90.0f, 0.0f, 0.0f),
        45.0f
    );
}

void TestScene::createTriangle() {
    Entity triangle = createPrimitiveEntity(PrimitiveKind::Triangle, "Triangle", glm::vec3(-1.0f, 0.0f, 0.0f));
    if (Material* material = registry.get<Material>(triangle)) {
        material->color = { 1.0f, 0.0f, 0.0f, 1.0f };
    }
}

void TestScene::createCube() {
    Entity cube = createPrimitiveEntity(PrimitiveKind::Cube, "Cube", glm::vec3(1.0f, 0.0f, 0.0f));
    if (Material* material = registry.get<Material>(cube)) {
        material->color = { 0.0f, 1.0f, 0.0f, 1.0f };
    }
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

Entity TestScene::findEntityByName(const std::string& name) const {
    for (auto [entity, entityName] : registry.view<Name>()) {
        if (entityName.value == name) {
            return entity;
        }
    }

    return Entity();
}

Entity TestScene::createPrimitiveEntity(const std::string& primitiveType) {
    PrimitiveKind kind = PrimitiveKind::Cube;
    std::string baseName = "Cube";

    if (primitiveType == "Triangle") {
        kind = PrimitiveKind::Triangle;
        baseName = "Triangle";
    } else if (primitiveType == "Cube") {
        kind = PrimitiveKind::Cube;
        baseName = "Cube";
    } else {
        return Entity();
    }

    return createPrimitiveEntity(kind, makeUniqueEntityName(baseName), glm::vec3(0.0f));
}

Entity TestScene::createEntityOfType(const std::string& entityType) {
    if (entityType == "Camera") {
        return createCameraEntity(
            makeUniqueEntityName("Camera"),
            glm::vec3(0.0f, 5.0f, 5.0f),
            glm::vec3(-90.0f, 0.0f, 0.0f),
            45.0f
        );
    }

    if (entityType == "Grid") {
        return createGridEntity(
            makeUniqueEntityName("Grid"),
            glm::vec3(0.0f),
            glm::vec3(-90.0f, 0.0f, 0.0f),
            glm::vec4(0.3f, 0.3f, 0.3f, 1.0f),
            1.0f,
            100.0f
        );
    }

    return Entity();
}

Entity TestScene::duplicateEntity(Entity entity) {
    if (entity.getId() == Entity::INVALID_ENTITY) {
        return Entity();
    }

    Name* name = registry.get<Name>(entity);
    Transform* transform = registry.get<Transform>(entity);
    if (!name || !transform) {
        return Entity();
    }

    const std::string duplicateName = makeUniqueEntityName(name->value);
    const glm::vec3 offsetPosition = transform->position + glm::vec3(1.0f, 0.0f, 1.0f);

    if (Camera* camera = registry.get<Camera>(entity)) {
        Entity duplicated = createCameraEntity(duplicateName, offsetPosition, transform->rotation, camera->fov);
        if (Camera* duplicatedCamera = registry.get<Camera>(duplicated)) {
            duplicatedCamera->nearPlane = camera->nearPlane;
            duplicatedCamera->farPlane = camera->farPlane;
            duplicatedCamera->moveSpeed = camera->moveSpeed;
            duplicatedCamera->mouseSensitivity = camera->mouseSensitivity;
        }
        return duplicated;
    }

    if (Grid* grid = registry.get<Grid>(entity)) {
        return createGridEntity(
            duplicateName,
            offsetPosition,
            transform->rotation,
            grid->color,
            grid->spacing,
            grid->size
        );
    }

    PrimitiveKind kind = PrimitiveKind::Cube;
    if (PrimitiveType* primitive = registry.get<PrimitiveType>(entity)) {
        kind = primitive->kind;
    }

    Entity duplicated = createPrimitiveEntity(kind, duplicateName, offsetPosition);
    if (Transform* duplicatedTransform = registry.get<Transform>(duplicated)) {
        duplicatedTransform->rotation = transform->rotation;
        duplicatedTransform->scale = transform->scale;
    }
    if (Material* sourceMaterial = registry.get<Material>(entity)) {
        if (Material* duplicatedMaterial = registry.get<Material>(duplicated)) {
            duplicatedMaterial->color = sourceMaterial->color;
        }
    }

    return duplicated;
}

bool TestScene::deleteEntity(Entity entity) {
    if (entity.getId() == Entity::INVALID_ENTITY) {
        return false;
    }

    untrackEntity(entity);
    registry.destroy(entity);
    return true;
}

Entity TestScene::createPrimitiveEntity(PrimitiveKind kind, const std::string& name, const glm::vec3& position) {
    Entity entity = trackEntity(registry.create());
    if (entity.getId() == Entity::INVALID_ENTITY) {
        return Entity();
    }

    registry.emplace<Transform>(entity, Transform{ position });
    registry.emplace<Name>(entity, Name{ name });
    registry.emplace<PrimitiveType>(entity, PrimitiveType{ kind });

    Mesh meshData;
    glm::vec4 defaultColor(1.0f);
    switch (kind) {
    case PrimitiveKind::Triangle:
        meshData = Primitives::makeTriangle();
        defaultColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        break;
    case PrimitiveKind::Cube:
        meshData = Primitives::makeCube();
        defaultColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
        break;
    case PrimitiveKind::Quad:
        meshData = Primitives::makeQuad();
        break;
    }

    registry.emplace<Mesh>(entity, std::move(meshData));
    registry.emplace<Material>(entity, Material{ defaultColor });

    PipelineHandle pipeline = renderer.createPipelineForShaders(
        "build/shaders/unlit.vert.spv",
        "build/shaders/unlit.frag.spv"
    );

    if (Material* material = registry.get<Material>(entity)) {
        material->pipeline = pipeline.pipeline;
        material->pipelineLayout = pipeline.layout;
    }

    uploadMesh(entity);
    return entity;
}

Entity TestScene::createCameraEntity(const std::string& name, const glm::vec3& position, const glm::vec3& rotation, float fov) {
    Entity cameraEntity = trackEntity(registry.create());
    if (cameraEntity.getId() == Entity::INVALID_ENTITY) {
        throw std::runtime_error("Ran out of entity IDs");
    }

    registry.emplace<Camera>(cameraEntity, Camera{});
    registry.emplace<Transform>(cameraEntity, Transform{});
    registry.emplace<InputComponent>(cameraEntity, InputComponent{});
    registry.emplace<Name>(cameraEntity, Name{ name });

    Camera* camera = registry.get<Camera>(cameraEntity);
    Transform* transform = registry.get<Transform>(cameraEntity);
    if (!camera || !transform) {
        throw std::runtime_error("Camera setup failed");
    }

    transform->position = position;
    transform->rotation = rotation;
    camera->fov = fov;

    int width = 0;
    int height = 0;
    glfwGetWindowSize(renderer.getWindow(), &width, &height);
    camera->aspect = static_cast<float>(width) / static_cast<float>(height);
    renderer.setActiveCamera(camera->projection(), transform->position, camera->view(*transform));
    return cameraEntity;
}

Entity TestScene::createGridEntity(const std::string& name, const glm::vec3& position, const glm::vec3& rotation, const glm::vec4& color, float spacing, float size) {
    Entity grid = trackEntity(registry.create());
    if (grid.getId() == Entity::INVALID_ENTITY) {
        return Entity();
    }

    registry.emplace<Transform>(grid, Transform{ position });
    if (Transform* gridTransform = registry.get<Transform>(grid)) {
        gridTransform->rotation = rotation;
    }

    registry.emplace<Name>(grid, Name{ name });
    registry.emplace<PrimitiveType>(grid, PrimitiveType{ PrimitiveKind::Quad });
    registry.emplace<Material>(grid, Material{ color });
    registry.emplace<Mesh>(grid, Primitives::makeQuad());
    registry.emplace<Grid>(grid, Grid{ spacing, size, color });

    PipelineHandle gridPipeline = renderer.createPipelineForShaders(
        "build/shaders/grid.vert.spv",
        "build/shaders/grid.frag.spv"
    );

    if (Material* material = registry.get<Material>(grid)) {
        material->pipeline = gridPipeline.pipeline;
        material->pipelineLayout = gridPipeline.layout;
    }

    uploadMesh(grid);
    return grid;
}

std::string TestScene::makeUniqueEntityName(const std::string& baseName) const {
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
