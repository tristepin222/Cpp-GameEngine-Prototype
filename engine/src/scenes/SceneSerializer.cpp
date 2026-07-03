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
#include "ecs/components/Hierarchy.hpp"
#include "ecs/components/AnimationController.hpp"
#include "ecs/components/IKSolver.hpp"

struct ParentNameComponent {
    std::string name;
};
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
                    if (mesh->primitiveIndex != -1) {
                        out << ",\n" << JSONUtils::indent(indent) << "\"primitiveIndex\": " << mesh->primitiveIndex;
                    }
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

                float primIndexFloat = -1.0f;
                int primitiveIndex = -1;
                if (JSONUtils::extractFloatValue(json, "primitiveIndex", primIndexFloat)) {
                    primitiveIndex = static_cast<int>(primIndexFloat);
                }
                
                bool isAssetMesh = !gltfPath.empty();
                Mesh meshData;
                
                if (isAssetMesh) {
                    try {
                        meshData = renderer.resourceManager->loadMesh(gltfPath, renderer, primitiveIndex);
                        meshData.primitiveIndex = primitiveIndex;
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

    // 4. Hierarchy Component (Parent-Child relationship)
    reg.registerComponent(
        "Hierarchy",
        [](Registry& registry, Entity entity, std::ostream& out, int indent) {
            if (auto* hierarchy = registry.get<HierarchyComponent>(entity)) {
                if (hierarchy->parent.getId() != Entity::INVALID_ENTITY && registry.isValid(hierarchy->parent)) {
                    if (auto* parentName = registry.get<Name>(hierarchy->parent)) {
                        out << ",\n" << JSONUtils::indent(indent) << "\"entityType\": " << JSONUtils::quote("Hierarchy") << ",\n";
                        out << JSONUtils::indent(indent) << "\"parentName\": " << JSONUtils::quote(parentName->value);
                    }
                }
            }
        },
        [](Registry& registry, VulkanRenderer& renderer, Entity entity, const std::string& json) {
            std::string parentNameStr = JSONUtils::extractStringValue(json, "parentName");
            if (!parentNameStr.empty()) {
                registry.emplace<ParentNameComponent>(entity, ParentNameComponent{ parentNameStr });
            }
        }
    );

    // 5. Animation Controller Component
    reg.registerComponent(
        "AnimationController",
        [](Registry& registry, Entity entity, std::ostream& out, int indent) {
            if (auto* controller = registry.get<AnimationControllerComponent>(entity)) {
                out << ",\n" << JSONUtils::indent(indent) << "\"entityType\": " << JSONUtils::quote("AnimationController");
                out << ",\n" << JSONUtils::indent(indent) << "\"currentState\": " << JSONUtils::quote(controller->currentState);
                
                bool hasLocomotion = (controller->states.size() == 2 && controller->transitions.size() == 2);
                out << ",\n" << JSONUtils::indent(indent) << "\"hasLocomotionSetup\": " << (hasLocomotion ? "true" : "false");
                
                out << ",\n" << JSONUtils::indent(indent) << "\"parameters\": {";
                bool first = true;
                for (const auto& [name, val] : controller->parameters) {
                    if (!first) out << ", ";
                    out << JSONUtils::quote(name) << ": " << val;
                    first = false;
                }
                out << "}";
            }
        },
        [](Registry& registry, VulkanRenderer& renderer, Entity entity, const std::string& json) {
            if (json.find("\"entityType\": \"AnimationController\"") != std::string::npos ||
                json.find("\"entityType\":\"AnimationController\"") != std::string::npos) {
                
                AnimationControllerComponent controller{};
                controller.currentState = JSONUtils::extractStringValue(json, "currentState");
                
                bool hasLocomotion = false;
                if (json.find("\"hasLocomotionSetup\": true") != std::string::npos ||
                    json.find("\"hasLocomotionSetup\":true") != std::string::npos) {
                    hasLocomotion = true;
                }
                
                float speedVal = 0.0f;
                size_t speedPos = json.find("\"speed\"");
                if (speedPos != std::string::npos) {
                    size_t colonPos = json.find(":", speedPos);
                    if (colonPos != std::string::npos) {
                        std::string valStr;
                        for (size_t i = colonPos + 1; i < json.size(); ++i) {
                            char c = json[i];
                            if (std::isdigit(c) || c == '.' || c == '-') {
                                valStr += c;
                            } else if (!valStr.empty()) {
                                break;
                            }
                        }
                        if (!valStr.empty()) {
                            speedVal = std::stof(valStr);
                        }
                    }
                }
                controller.parameters["speed"] = speedVal;

                if (hasLocomotion) {
                    AnimationState idleState;
                    idleState.name = "Idle";
                    idleState.clipName = "idle";
                    idleState.isBlendTree = false;
                    
                    AnimationState moveState;
                    moveState.name = "Movement";
                    moveState.isBlendTree = true;
                    moveState.blendTree.parameterName = "speed";
                    
                    if (auto* animator = registry.get<AnimatorComponent>(entity)) {
                        if (!animator->animations.empty()) idleState.clipName = animator->animations[0].name;
                        if (animator->animations.size() >= 2) {
                            BlendNode nodeWalk{ animator->animations[0].name, 0.0f };
                            BlendNode nodeRun{ animator->animations[1].name, 1.0f };
                            moveState.blendTree.nodes = { nodeWalk, nodeRun };
                        } else if (!animator->animations.empty()) {
                            BlendNode nodeWalk{ animator->animations[0].name, 0.0f };
                            moveState.blendTree.nodes = { nodeWalk };
                        }
                    }
                    
                    controller.states = { idleState, moveState };

                    AnimationTransition toMove;
                    toMove.fromState = "Idle";
                    toMove.toState = "Movement";
                    toMove.crossfadeDuration = 0.3f;
                    TransitionCondition condMove{ "speed", ">", 0.1f };
                    toMove.conditions = { condMove };
                    
                    AnimationTransition toIdle;
                    toIdle.fromState = "Movement";
                    toIdle.toState = "Idle";
                    toIdle.crossfadeDuration = 0.3f;
                    TransitionCondition condIdle2{ "speed", "<", 0.1f };
                    toIdle.conditions = { condIdle2 };
                    
                    controller.transitions = { toMove, toIdle };
                }

                registry.emplace<AnimationControllerComponent>(entity, std::move(controller));
            }
        }
    );

    // 6. IK Solver Component
    reg.registerComponent(
        "IKSolver",
        [](Registry& registry, Entity entity, std::ostream& out, int indent) {
            if (auto* ik = registry.get<IKSolverComponent>(entity)) {
                out << ",\n" << JSONUtils::indent(indent) << "\"entityType\": " << JSONUtils::quote("IKSolver");
                out << ",\n" << JSONUtils::indent(indent) << "\"enabled\": " << (ik->enabled ? "true" : "false");
                out << ",\n" << JSONUtils::indent(indent) << "\"solverType\": " << static_cast<int>(ik->solverType);
                out << ",\n" << JSONUtils::indent(indent) << "\"startJointName\": " << JSONUtils::quote(ik->startJointName);
                out << ",\n" << JSONUtils::indent(indent) << "\"middleJointName\": " << JSONUtils::quote(ik->middleJointName);
                out << ",\n" << JSONUtils::indent(indent) << "\"endJointName\": " << JSONUtils::quote(ik->endJointName);
                out << ",\n" << JSONUtils::indent(indent) << "\"maxIterations\": " << ik->maxIterations;
                out << ",\n" << JSONUtils::indent(indent) << "\"tolerance\": " << ik->tolerance;
                
                out << ",\n" << JSONUtils::indent(indent) << "\"jointChainNames\": [";
                for (size_t i = 0; i < ik->jointChainNames.size(); ++i) {
                    if (i > 0) out << ", ";
                    out << JSONUtils::quote(ik->jointChainNames[i]);
                }
                out << "]";
                
                out << ",\n" << JSONUtils::indent(indent) << "\"targetPosition\": " << JSONUtils::vec3ToJson(ik->targetPosition);
                out << ",\n" << JSONUtils::indent(indent) << "\"polePosition\": " << JSONUtils::vec3ToJson(ik->polePosition);
                out << ",\n" << JSONUtils::indent(indent) << "\"targetWeight\": " << ik->targetWeight;
            }
        },
        [](Registry& registry, VulkanRenderer& renderer, Entity entity, const std::string& json) {
            if (json.find("\"entityType\": \"IKSolver\"") != std::string::npos ||
                json.find("\"entityType\":\"IKSolver\"") != std::string::npos) {
                
                IKSolverComponent ik{};
                ik.enabled = (json.find("\"enabled\": true") != std::string::npos || json.find("\"enabled\":true") != std::string::npos);
                
                float typeVal = 0.0f;
                if (JSONUtils::extractFloatValue(json, "solverType", typeVal)) {
                    ik.solverType = static_cast<int>(typeVal) == 0 ? IKSolverType::TwoBone : IKSolverType::FABRIK;
                }
                
                ik.startJointName = JSONUtils::extractStringValue(json, "startJointName");
                ik.middleJointName = JSONUtils::extractStringValue(json, "middleJointName");
                ik.endJointName = JSONUtils::extractStringValue(json, "endJointName");
                
                float maxIter = 10.0f;
                if (JSONUtils::extractFloatValue(json, "maxIterations", maxIter)) {
                    ik.maxIterations = static_cast<int>(maxIter);
                }
                float tol = 0.001f;
                JSONUtils::extractFloatValue(json, "tolerance", tol);
                ik.tolerance = tol;
                
                // Parse joint chain names array
                size_t arrayStart = json.find("\"jointChainNames\"");
                if (arrayStart != std::string::npos) {
                    size_t braceStart = json.find("[", arrayStart);
                    size_t braceEnd = json.find("]", arrayStart);
                    if (braceStart != std::string::npos && braceEnd != std::string::npos && braceEnd > braceStart) {
                        std::string arrayContent = json.substr(braceStart + 1, braceEnd - braceStart - 1);
                        std::stringstream ss(arrayContent);
                        std::string item;
                        while (std::getline(ss, item, ',')) {
                            size_t q1 = item.find("\"");
                            size_t q2 = item.rfind("\"");
                            if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                                ik.jointChainNames.push_back(item.substr(q1 + 1, q2 - q1 - 1));
                            }
                        }
                    }
                }
                
                float targetPos[3]{};
                if (JSONUtils::extractFloatArray(json, "targetPosition", targetPos, 3)) {
                    ik.targetPosition = glm::vec3(targetPos[0], targetPos[1], targetPos[2]);
                }
                float polePos[3]{};
                if (JSONUtils::extractFloatArray(json, "polePosition", polePos, 3)) {
                    ik.polePosition = glm::vec3(polePos[0], polePos[1], polePos[2]);
                }
                
                float weight = 1.0f;
                if (JSONUtils::extractFloatValue(json, "targetWeight", weight)) {
                    ik.targetWeight = weight;
                }
                
                registry.emplace<IKSolverComponent>(entity, std::move(ik));
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

    // Resolve parent entity links in HierarchyComponent using temporary ParentNameComponent records
    for (auto [entity, parentNameComp] : registry.view<ParentNameComponent>()) {
        Entity parentEntity;
        for (auto [parentCandidate, nameComp] : registry.view<Name>()) {
            if (nameComp.value == parentNameComp.name) {
                parentEntity = parentCandidate;
                break;
            }
        }
        if (parentEntity.getId() != Entity::INVALID_ENTITY) {
            registry.emplace<HierarchyComponent>(entity, HierarchyComponent{ parentEntity });
        }
    }
    
    // Clean up temporary ParentNameComponent storage
    std::vector<Entity> toClean;
    for (auto [entity, parentNameComp] : registry.view<ParentNameComponent>()) {
        toClean.push_back(entity);
    }
    for (Entity e : toClean) {
        registry.remove<ParentNameComponent>(e);
    }

    return !outEntities.empty();
}

bool SceneSerializer::serializePrefab(const std::string& path, Entity rootEntity) {
    if (rootEntity.getId() == Entity::INVALID_ENTITY || !registry.isValid(rootEntity)) {
        return false;
    }

    std::filesystem::path outputPath(path);
    if (outputPath.has_parent_path()) {
        std::filesystem::create_directories(outputPath.parent_path());
    }

    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    // 1. Traverse and gather all entities in the hierarchy recursively
    std::vector<Entity> entities;
    entities.push_back(rootEntity);
    size_t cursor = 0;
    while (cursor < entities.size()) {
        Entity current = entities[cursor];
        for (auto [childEntity, hierarchy] : registry.view<HierarchyComponent>()) {
            if (hierarchy.parent == current) {
                if (std::find(entities.begin(), entities.end(), childEntity) == entities.end()) {
                    entities.push_back(childEntity);
                }
            }
        }
        cursor++;
    }

    out << "{\n";
    out << JSONUtils::indent(1) << "\"prefabType\": " << JSONUtils::quote("EntityPrefab") << ",\n";
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
        out << JSONUtils::indent(3) << "\"id\": " << entity.getId() << ",\n";
        
        // Find if this entity has a parent in the prefab list
        auto* hierarchy = registry.get<HierarchyComponent>(entity);
        if (hierarchy && hierarchy->parent.getId() != Entity::INVALID_ENTITY && registry.isValid(hierarchy->parent)) {
            if (std::find(entities.begin(), entities.end(), hierarchy->parent) != entities.end()) {
                out << JSONUtils::indent(3) << "\"parentId\": " << hierarchy->parent.getId() << ",\n";
            }
        }

        out << JSONUtils::indent(3) << "\"name\": " << JSONUtils::quote(name->value) << ",\n";
        out << JSONUtils::indent(3) << "\"position\": " << JSONUtils::vec3ToJson(transform->position) << ",\n";
        out << JSONUtils::indent(3) << "\"rotation\": " << JSONUtils::vec3ToJson(transform->rotation) << ",\n";
        out << JSONUtils::indent(3) << "\"scale\": " << JSONUtils::vec3ToJson(transform->scale);

        // Dynamically invoke all registered component serializers to append custom properties
        for (const auto& reg : ComponentSerializerRegistry::getInstance().getRegistrations()) {
            if (reg.componentName == "Hierarchy") {
                continue; // Skip the default Hierarchy component serialization to avoid parentName collisions
            }
            reg.serialize(registry, entity, out, 3);
        }

        out << "\n" << JSONUtils::indent(2) << "}";
    }

    out << "\n" << JSONUtils::indent(1) << "]\n";
    out << "}\n";
    return true;
}

Entity SceneSerializer::deserializePrefab(const std::string& path, std::vector<Entity>& loadedEntities, Entity parentEntity) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return Entity();
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string source = buffer.str();

    std::vector<std::string> entityObjects = JSONUtils::extractEntityObjects(source);
    if (entityObjects.empty()) {
        return Entity();
    }

    // Temporary mappings to reconstruct the hierarchy relative to newly spawned entities
    std::unordered_map<int, Entity> originalIdToNewEntity;
    std::unordered_map<Entity, int> newEntityToParentId;
    Entity rootNewEntity;

    for (const std::string& entityJson : entityObjects) {
        std::string name = JSONUtils::extractStringValue(entityJson, "name");
        if (name.empty()) {
            continue;
        }

        // Extract the original serialized ID
        int originalId = -1;
        size_t idPos = entityJson.find("\"id\"");
        if (idPos != std::string::npos) {
            size_t colonPos = entityJson.find(":", idPos);
            if (colonPos != std::string::npos) {
                std::string valStr;
                for (size_t i = colonPos + 1; i < entityJson.size(); ++i) {
                    char c = entityJson[i];
                    if (std::isdigit(c) || c == '-') {
                        valStr += c;
                    } else if (!valStr.empty()) {
                        break;
                    }
                }
                if (!valStr.empty()) {
                    originalId = std::stoi(valStr);
                }
            }
        }

        // Extract the parent ID if present
        int parentId = -1;
        size_t parentIdPos = entityJson.find("\"parentId\"");
        if (parentIdPos != std::string::npos) {
            size_t colonPos = entityJson.find(":", parentIdPos);
            if (colonPos != std::string::npos) {
                std::string valStr;
                for (size_t i = colonPos + 1; i < entityJson.size(); ++i) {
                    char c = entityJson[i];
                    if (std::isdigit(c) || c == '-') {
                        valStr += c;
                    } else if (!valStr.empty()) {
                        break;
                    }
                }
                if (!valStr.empty()) {
                    parentId = std::stoi(valStr);
                }
            }
        }

        Entity entity = registry.create();
        if (entity.getId() == Entity::INVALID_ENTITY) {
            continue;
        }

        // Track the root node (first entity created in the prefab that has no parent ID, or parentId is not in the list)
        if (rootNewEntity.getId() == Entity::INVALID_ENTITY && parentId == -1) {
            rootNewEntity = entity;
        }

        // Store mappings
        if (originalId != -1) {
            originalIdToNewEntity[originalId] = entity;
        }
        if (parentId != -1) {
            newEntityToParentId[entity] = parentId;
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

        // Invoke other registered component deserializers
        for (const auto& reg : ComponentSerializerRegistry::getInstance().getRegistrations()) {
            if (reg.componentName == "Hierarchy") {
                continue; // Skip Hierarchy resolving here, we do it in a custom step below
            }
            reg.deserialize(registry, renderer, entity, entityJson);
        }

        loadedEntities.push_back(entity);
    }

    // Fallback: If no entity was identified as root, pick the first one
    if (rootNewEntity.getId() == Entity::INVALID_ENTITY && !loadedEntities.empty()) {
        rootNewEntity = loadedEntities[0];
    }

    // 2. Resolve hierarchical parent links using our ID mappings
    for (auto& [entity, parentId] : newEntityToParentId) {
        auto it = originalIdToNewEntity.find(parentId);
        if (it != originalIdToNewEntity.end()) {
            registry.emplace<HierarchyComponent>(entity, HierarchyComponent{ it->second });
        }
    }

    // 3. If a parentEntity was provided, attach the root of the prefab under it
    if (parentEntity.getId() != Entity::INVALID_ENTITY && registry.isValid(parentEntity)) {
        if (rootNewEntity.getId() != Entity::INVALID_ENTITY) {
            registry.emplace<HierarchyComponent>(rootNewEntity, HierarchyComponent{ parentEntity });
        }
    }

    return rootNewEntity;
}
