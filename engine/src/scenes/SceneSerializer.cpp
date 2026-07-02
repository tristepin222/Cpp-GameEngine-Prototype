#include "scenes/SceneSerializer.hpp"
#include "scenes/ComponentSerializerRegistry.hpp"
#include "ecs/EntityFactory.hpp"
#include "renderer/VulkanRenderer.hpp"
#include "renderer/ResourceManager.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/Name.hpp"
#include "ecs/components/PrimitiveType.hpp"
#include "ecs/components/Material.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/Grid.hpp"
#include "ecs/components/inputComponent.hpp"
#include "ecs/components/primitives.hpp"
#include "ecs/components/Skeleton.hpp"
#include "ecs/components/Animator.hpp"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>
#include <sstream>

// Static initializer to register core engine components with the registry
/**
 * @brief Static function registering component serializers for built-in camera, grid, and primitive types.
 * @return True when registered.
 */
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
            if (auto* mesh = registry.get<Mesh>(entity)) {
                if (registry.has<Grid>(entity)) {
                    return; // Skip grid mesh details
                }
                if (!mesh->gltfPath.empty()) {
                    out << ",\n" << JSONUtils::indent(indent) << "\"entityType\": " << JSONUtils::quote("Primitive") << ",\n";
                    out << JSONUtils::indent(indent) << "\"gltfPath\": " << JSONUtils::quote(mesh->gltfPath);
                } else if (auto* primitive = registry.get<PrimitiveType>(entity)) {
                    std::string primStr = "Cube";
                    if (primitive->kind == PrimitiveKind::Triangle) primStr = "Triangle";
                    else if (primitive->kind == PrimitiveKind::Quad) primStr = "Quad";
                    
                    out << ",\n" << JSONUtils::indent(indent) << "\"entityType\": " << JSONUtils::quote("Primitive") << ",\n";
                    out << JSONUtils::indent(indent) << "\"primitive\": " << JSONUtils::quote(primStr);
                }
            }
            if (auto* material = registry.get<Material>(entity)) {
                if (registry.has<Grid>(entity)) {
                    return; // Skip grid color details
                }
                out << ",\n" << JSONUtils::indent(indent) << "\"color\": " << JSONUtils::vec4ToJson(material->color);
                if (!material->texturePath.empty()) {
                    out << ",\n" << JSONUtils::indent(indent) << "\"texturePath\": " << JSONUtils::quote(material->texturePath);
                }
            }
        },
        [](Registry& registry, VulkanRenderer& renderer, Entity entity, const std::string& json) {
            std::string type = JSONUtils::extractStringValue(json, "entityType");
            std::string primStr = JSONUtils::extractStringValue(json, "primitive");
            std::string gltfPath = JSONUtils::extractStringValue(json, "gltfPath");
            std::string texturePath = JSONUtils::extractStringValue(json, "texturePath");
            
            if (type == "Camera" || type == "Grid") {
                return; // Managed by separate component deserializers
            }

            // Skip grids using key detection to avoid double parsing
            float dummySpacing = 0.0f;
            if (JSONUtils::extractFloatValue(json, "gridSpacing", dummySpacing)) {
                return;
            }

            bool isPrimitive = (type == "Primitive" || !primStr.empty() || !gltfPath.empty());
            if (!isPrimitive) {
                float colorArray[4]{};
                if (JSONUtils::extractFloatArray(json, "color", colorArray, 4)) {
                    isPrimitive = true;
                    std::string nameVal = JSONUtils::extractStringValue(json, "name");
                    if (nameVal.find("Triangle") != std::string::npos) {
                        primStr = "Triangle";
                    } else if (nameVal.find("Quad") != std::string::npos) {
                        primStr = "Quad";
                    } else {
                        primStr = "Cube";
                    }
                }
            }

            if (isPrimitive) {
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
                
                bool isAssetMesh = !gltfPath.empty();
                Mesh meshData;
                
                if (isAssetMesh) {
                    try {
                        meshData = renderer.resourceManager->loadMesh(gltfPath, renderer);
                        registry.emplace<PrimitiveType>(entity, PrimitiveType{ PrimitiveKind::Cube }); // Default placeholder

                        SkeletonComponent skeleton{};
                        AnimatorComponent animator{};
                        if (renderer.resourceManager->loadSkeletonAndAnimations(gltfPath, skeleton, animator)) {
                            registry.emplace<SkeletonComponent>(entity, std::move(skeleton));
                            registry.emplace<AnimatorComponent>(entity, std::move(animator));
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[SceneSerializer] Error loading mesh " << gltfPath << ": " << e.what() << std::endl;
                        meshData = Primitives::makeCube();
                        registry.emplace<PrimitiveType>(entity, PrimitiveType{ PrimitiveKind::Cube });
                    }
                } else {
                    registry.emplace<PrimitiveType>(entity, PrimitiveType{ kind });
                    switch (kind) {
                    case PrimitiveKind::Triangle: meshData = Primitives::makeTriangle(); break;
                    case PrimitiveKind::Cube: meshData = Primitives::makeCube(); break;
                    case PrimitiveKind::Quad: meshData = Primitives::makeQuad(); break;
                    }
                }
                
                registry.emplace<Mesh>(entity, std::move(meshData));
                registry.emplace<Material>(entity, Material{ color });
                
                if (!texturePath.empty()) {
                    if (auto* material = registry.get<Material>(entity)) {
                        material->texturePath = texturePath;
                        if (auto* tex = renderer.resourceManager->loadTexture(texturePath, renderer)) {
                            material->descriptorSet = tex->descriptorSet;
                        }
                    }
                }
                
                bool hasSkin = registry.has<SkeletonComponent>(entity);
                PipelineHandle pipeline = renderer.createPipelineForShaders(
                    hasSkin ? "build/shaders/skinned.vert.spv" : "build/shaders/unlit.vert.spv",
                    "build/shaders/unlit.frag.spv"
                );

                if (Material* material = registry.get<Material>(entity)) {
                    material->pipeline = pipeline.pipeline;
                    material->pipelineLayout = pipeline.layout;
                }
                
                if (!isAssetMesh) {
                    EntityFactory::uploadMesh(registry, renderer, entity);
                }
            }
        }
    );

    return true;
}

/**
 * @brief Construct a new Scene Serializer:: Scene Serializer object.
 * @param registry Reference to ECS registry.
 * @param renderer Reference to Vulkan renderer.
 */
SceneSerializer::SceneSerializer(Registry& registry, VulkanRenderer& renderer)
    : registry(registry), renderer(renderer) {
    // Force built-in components to be registered exactly once
    static bool init = registerBuiltinComponents();
    (void)init;
}

/**
 * @brief Serializes a list of entities to a JSON file.
 * @param path Target file path.
 * @param entities List of entities to save.
 * @return True if successful, false otherwise.
 */
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

/**
 * @brief Deserializes scene entities from a JSON file.
 * @param path Source file path.
 * @param outEntities Vector to store deserialized entities.
 * @return True if successful, false otherwise.
 */
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
