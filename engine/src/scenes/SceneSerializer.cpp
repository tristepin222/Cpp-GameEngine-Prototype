#include "scenes/SceneSerializer.hpp"
#include "scenes/ComponentSerializerRegistry.hpp"
#include "ecs/EntityFactory.hpp"
#include "renderer/VulkanRenderer.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/Name.hpp"
#include "ecs/components/PrimitiveType.hpp"
#include "ecs/components/Material.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/Grid.hpp"
#include "ecs/components/inputComponent.hpp"
#include "ecs/components/primitives.hpp"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>
#include <sstream>

// Static initializer to register core engine components with the registry
static bool registerBuiltinComponents() {
    auto& reg = ComponentSerializerRegistry::getInstance();
    
    // 1. Camera Component
    reg.registerComponent(
        "Camera",
        [](Registry& registry, Entity entity, std::ostream& out, int indent) {
            if (auto* camera = registry.get<Camera>(entity)) {
                out << ",\n" << JSONUtils::indent(indent) << "\"entityType\": " << JSONUtils::quote("Camera") << ",\n";
                out << JSONUtils::indent(indent) << "\"fov\": " << camera->fov;
            }
        },
        [](Registry& registry, VulkanRenderer& renderer, Entity entity, const std::string& json) {
            float fov = 45.0f;
            bool hasFov = JSONUtils::extractFloatValue(json, "fov", fov);
            std::string type = JSONUtils::extractStringValue(json, "entityType");
            
            if (type == "Camera" || hasFov) {
                registry.emplace<Camera>(entity, Camera{});
                registry.emplace<InputComponent>(entity, InputComponent{});
                
                if (Camera* camera = registry.get<Camera>(entity)) {
                    camera->fov = fov;
                    int width = 0;
                    int height = 0;
                    glfwGetWindowSize(renderer.getWindow(), &width, &height);
                    camera->aspect = static_cast<float>(width) / static_cast<float>(height);
                    
                    if (Transform* transform = registry.get<Transform>(entity)) {
                        renderer.setActiveCamera(camera->projection(), transform->position, camera->view(*transform));
                    }
                }
            }
        }
    );

    // 2. Grid Component
    reg.registerComponent(
        "Grid",
        [](Registry& registry, Entity entity, std::ostream& out, int indent) {
            if (auto* grid = registry.get<Grid>(entity)) {
                out << ",\n" << JSONUtils::indent(indent) << "\"entityType\": " << JSONUtils::quote("Grid") << ",\n";
                out << JSONUtils::indent(indent) << "\"gridSpacing\": " << grid->spacing << ",\n";
                out << JSONUtils::indent(indent) << "\"gridSize\": " << grid->size;
            }
        },
        [](Registry& registry, VulkanRenderer& renderer, Entity entity, const std::string& json) {
            std::string type = JSONUtils::extractStringValue(json, "entityType");
            float spacing = 1.0f;
            float size = 100.0f;
            bool hasSpacing = JSONUtils::extractFloatValue(json, "gridSpacing", spacing);
            bool hasSize = JSONUtils::extractFloatValue(json, "gridSize", size);
            
            if (type == "Grid" || hasSpacing || hasSize) {
                glm::vec4 color(0.3f, 0.3f, 0.3f, 1.0f);
                float colorArray[4]{};
                if (JSONUtils::extractFloatArray(json, "color", colorArray, 4)) {
                    color = glm::vec4(colorArray[0], colorArray[1], colorArray[2], colorArray[3]);
                }
                
                registry.emplace<PrimitiveType>(entity, PrimitiveType{ PrimitiveKind::Quad });
                registry.emplace<Material>(entity, Material{ color });
                registry.emplace<Mesh>(entity, Primitives::makeQuad());
                registry.emplace<Grid>(entity, Grid{ spacing, size, color });
                
                PipelineHandle gridPipeline = renderer.createPipelineForShaders(
                    "build/shaders/grid.vert.spv",
                    "build/shaders/grid.frag.spv"
                );
                
                if (Material* material = registry.get<Material>(entity)) {
                    material->pipeline = gridPipeline.pipeline;
                    material->pipelineLayout = gridPipeline.layout;
                }
                
                EntityFactory::uploadMesh(registry, renderer, entity);
            }
        }
    );

    // 3. Primitive Component (Mesh & Material)
    reg.registerComponent(
        "Primitive",
        [](Registry& registry, Entity entity, std::ostream& out, int indent) {
            if (auto* primitive = registry.get<PrimitiveType>(entity)) {
                if (registry.has<Grid>(entity)) {
                    return; // Skip grid mesh details
                }
                
                std::string primStr = "Cube";
                if (primitive->kind == PrimitiveKind::Triangle) primStr = "Triangle";
                else if (primitive->kind == PrimitiveKind::Quad) primStr = "Quad";
                
                out << ",\n" << JSONUtils::indent(indent) << "\"entityType\": " << JSONUtils::quote("Primitive") << ",\n";
                out << JSONUtils::indent(indent) << "\"primitive\": " << JSONUtils::quote(primStr);
            }
            if (auto* material = registry.get<Material>(entity)) {
                if (registry.has<Grid>(entity)) {
                    return; // Skip grid color details
                }
                out << ",\n" << JSONUtils::indent(indent) << "\"color\": " << JSONUtils::vec4ToJson(material->color);
            }
        },
        [](Registry& registry, VulkanRenderer& renderer, Entity entity, const std::string& json) {
            std::string type = JSONUtils::extractStringValue(json, "entityType");
            std::string primStr = JSONUtils::extractStringValue(json, "primitive");
            
            if (type == "Camera" || type == "Grid") {
                return; // Managed by separate component deserializers
            }
            
            if (type == "Primitive" || !primStr.empty()) {
                PrimitiveKind kind = PrimitiveKind::Cube;
                if (primStr == "Triangle") kind = PrimitiveKind::Triangle;
                else if (primStr == "Quad") kind = PrimitiveKind::Quad;
                
                glm::vec4 color(1.0f);
                float colorArray[4]{};
                bool hasColor = JSONUtils::extractFloatArray(json, "color", colorArray, 4);
                if (hasColor) {
                    color = glm::vec4(colorArray[0], colorArray[1], colorArray[2], colorArray[3]);
                } else {
                    if (kind == PrimitiveKind::Triangle) color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                    else if (kind == PrimitiveKind::Cube) color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
                }
                
                registry.emplace<PrimitiveType>(entity, PrimitiveType{ kind });
                
                Mesh meshData;
                switch (kind) {
                case PrimitiveKind::Triangle: meshData = Primitives::makeTriangle(); break;
                case PrimitiveKind::Cube: meshData = Primitives::makeCube(); break;
                case PrimitiveKind::Quad: meshData = Primitives::makeQuad(); break;
                }
                
                registry.emplace<Mesh>(entity, std::move(meshData));
                registry.emplace<Material>(entity, Material{ color });
                
                PipelineHandle pipeline = renderer.createPipelineForShaders(
                    "build/shaders/unlit.vert.spv",
                    "build/shaders/unlit.frag.spv"
                );
                
                if (Material* material = registry.get<Material>(entity)) {
                    material->pipeline = pipeline.pipeline;
                    material->pipelineLayout = pipeline.layout;
                }
                
                EntityFactory::uploadMesh(registry, renderer, entity);
            }
        }
    );

    return true;
}

SceneSerializer::SceneSerializer(Registry& registry, VulkanRenderer& renderer)
    : registry(registry), renderer(renderer) {
    // Force built-in components to be registered exactly once
    static bool init = registerBuiltinComponents();
    (void)init;
}

bool SceneSerializer::serialize(const std::string& path, const std::vector<Entity>& entities) {
    std::filesystem::path outputPath(path);
    if (outputPath.has_parent_path()) {
        std::filesystem::create_directories(outputPath.parent_path());
    }

    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "{\n";
    out << JSONUtils::indent(1) << "\"scene\": " << JSONUtils::quote("TestScene") << ",\n";
    out << JSONUtils::indent(1) << "\"entities\": [\n";

    bool first = true;
    for (Entity entity : entities) {
        Name* name = registry.get<Name>(entity);
        Transform* transform = registry.get<Transform>(entity);
        if (!name || !transform) {
            continue;
        }

        if (!first) {
            out << ",\n";
        }
        first = false;

        out << JSONUtils::indent(2) << "{\n";
        out << JSONUtils::indent(3) << "\"name\": " << JSONUtils::quote(name->value) << ",\n";
        out << JSONUtils::indent(3) << "\"position\": " << JSONUtils::vec3ToJson(transform->position) << ",\n";
        out << JSONUtils::indent(3) << "\"rotation\": " << JSONUtils::vec3ToJson(transform->rotation) << ",\n";
        out << JSONUtils::indent(3) << "\"scale\": " << JSONUtils::vec3ToJson(transform->scale);

        // Dynamically invoke all registered component serializers to append custom properties
        for (const auto& reg : ComponentSerializerRegistry::getInstance().getRegistrations()) {
            reg.serialize(registry, entity, out, 3);
        }

        out << "\n" << JSONUtils::indent(2) << "}";
    }

    out << "\n" << JSONUtils::indent(1) << "]\n";
    out << "}\n";
    return true;
}

bool SceneSerializer::deserialize(const std::string& path, std::vector<Entity>& outEntities) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string source = buffer.str();

    std::vector<std::string> entityObjects = JSONUtils::extractEntityObjects(source);
    if (entityObjects.empty()) {
        return false;
    }

    for (const std::string& entityJson : entityObjects) {
        std::string name = JSONUtils::extractStringValue(entityJson, "name");
        if (name.empty()) {
            continue;
        }

        Entity entity = registry.create();
        if (entity.getId() == Entity::INVALID_ENTITY) {
            continue;
        }

        // Initialize core components
        registry.emplace<Name>(entity, Name{ name });
        registry.emplace<Transform>(entity, Transform{});
        
        Transform* transform = registry.get<Transform>(entity);
        if (transform) {
            float position[3]{};
            float rotation[3]{};
            float scale[3]{1.0f, 1.0f, 1.0f};

            if (JSONUtils::extractFloatArray(entityJson, "position", position, 3)) {
                transform->position = glm::vec3(position[0], position[1], position[2]);
            }
            if (JSONUtils::extractFloatArray(entityJson, "rotation", rotation, 3)) {
                transform->rotation = glm::vec3(rotation[0], rotation[1], rotation[2]);
            }
            if (JSONUtils::extractFloatArray(entityJson, "scale", scale, 3)) {
                transform->scale = glm::vec3(scale[0], scale[1], scale[2]);
            }
        }

        // Invoke all registered component deserializers
        for (const auto& reg : ComponentSerializerRegistry::getInstance().getRegistrations()) {
            reg.deserialize(registry, renderer, entity, entityJson);
        }

        outEntities.push_back(entity);
    }

    return !outEntities.empty();
}
