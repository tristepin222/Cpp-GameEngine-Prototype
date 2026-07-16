#include "ecs/systems/TilemapSystem.hpp"
#include "scenes/TilesetAsset.hpp"
#include "ecs/components/Transform.hpp"
#include "ecs/components/Name.hpp"
#include "ecs/components/Hierarchy.hpp"
#include "ecs/components/RigidBody.hpp"
#include "ecs/components/Collider.hpp"
#include "ecs/components/Material.hpp"
#include "ecs/components/Mesh.hpp"
#include "renderer/ResourceManager.hpp"
#include <iostream>
#include <algorithm>

namespace Engine {

    TilemapSystem::TilemapSystem(Registry& reg, VulkanRenderer& rend)
        : registry(reg), renderer(rend) {
        registry.subscribeToAdded<TilemapComponent>([this](Entity e) {
            if (auto* tilemap = registry.get<TilemapComponent>(e)) {
                tilemap->isDirty = true;
            }
        });
    }

    void TilemapSystem::update(float dt) {
        for (auto [entity, tilemap] : registry.view<TilemapComponent>()) {
            if (tilemap.isDirty) {
                rebuildTilemap(entity, tilemap);
            }
        }
    }

    void TilemapSystem::rebuildTilemap(Entity entity, TilemapComponent& tilemap) {
        // 1. Load the TilesetAsset from disk (cached after first load)
        if (tilemap.tilesetPath.empty()) return;

        TilesetAsset* tileset = loadOrGetTileset(tilemap.tilesetPath, renderer);
        if (!tileset || !tileset->atlas.valid) return;
        if (tileset->tiles.empty()) return;

        // 2. Build single-mesh geometry — all tiles use the atlas, so one draw call
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        for (int y = 0; y < tilemap.height; ++y) {
            for (int x = 0; x < tilemap.width; ++x) {
                int cellIdx = y * tilemap.width + x;
                if (cellIdx >= static_cast<int>(tilemap.tiles.size())) continue;
                int tileIdx = tilemap.tiles[cellIdx];
                if (tileIdx < 0 || tileIdx >= static_cast<int>(tileset->tiles.size())) continue;

                // UV rectangle for this tile inside the atlas
                const glm::vec4& uv = tileset->tiles[tileIdx].atlasUV;
                float u0 = uv.x, v0 = uv.y, u1 = uv.z, v1 = uv.w;

                // World-space quad corners for this cell
                float tx0 = x * tilemap.tileSize;
                float ty0 = y * tilemap.tileSize;
                float tx1 = (x + 1) * tilemap.tileSize;
                float ty1 = (y + 1) * tilemap.tileSize;

                uint32_t vOff = static_cast<uint32_t>(vertices.size());
                glm::vec3 normal(0.0f, 0.0f, 1.0f);

                vertices.push_back(Vertex(glm::vec3(tx0, ty0, 0.f), normal, glm::vec2(u0, v1))); // BL
                vertices.push_back(Vertex(glm::vec3(tx1, ty0, 0.f), normal, glm::vec2(u1, v1))); // BR
                vertices.push_back(Vertex(glm::vec3(tx1, ty1, 0.f), normal, glm::vec2(u1, v0))); // TR
                vertices.push_back(Vertex(glm::vec3(tx0, ty1, 0.f), normal, glm::vec2(u0, v0))); // TL

                indices.push_back(vOff + 0); indices.push_back(vOff + 1); indices.push_back(vOff + 2);
                indices.push_back(vOff + 2); indices.push_back(vOff + 3); indices.push_back(vOff + 0);
            }
        }

        // 3. Upload geometry to the GPU
        auto* mesh = registry.get<Mesh>(entity);
        if (!mesh) {
            registry.emplace<Mesh>(entity, Mesh{});
            mesh = registry.get<Mesh>(entity);
        }

        mesh->vertices = std::move(vertices);
        mesh->indices  = std::move(indices);

        bool alreadyUploaded = (mesh->vertexBuffer != VK_NULL_HANDLE)
                             && (mesh->id < static_cast<uint32_t>(renderer.meshSoA.ids.size()));
        if (alreadyUploaded) {
            // Overwrite existing SoA entry and re-upload
            renderer.meshSoA.vertices[mesh->id] = mesh->vertices;
            renderer.meshSoA.indices[mesh->id]  = mesh->indices;
            renderer.uploadMesh(mesh->id);
            mesh->vertexBuffer = renderer.meshSoA.vertexBuffers[mesh->id].get();
            mesh->indexBuffer  = renderer.meshSoA.indexBuffers[mesh->id].get();
        } else {
            const size_t meshID = renderer.meshSoA.push(mesh->vertices, mesh->indices);
            renderer.uploadMesh(meshID);
            mesh->id           = static_cast<uint32_t>(meshID);
            mesh->vertexBuffer = renderer.meshSoA.vertexBuffers[meshID].get();
            mesh->indexBuffer  = renderer.meshSoA.indexBuffers[meshID].get();
        }

        // 4. Configure material — bind the atlas descriptor set
        auto* mat = registry.get<Material>(entity);
        if (!mat) {
            registry.emplace<Material>(entity, Material{});
            mat = registry.get<Material>(entity);
        }
        mat->color         = glm::vec4(1.f);
        mat->texturePath   = "tileset_atlas:" + tilemap.tilesetPath;
        mat->descriptorSet = tileset->atlas.descriptorSet;
        mat->filterMode    = TextureFilterMode::Nearest;

        PipelineHandle pipeline = renderer.createPipelineForShaders(
            renderer.resolveShaderPath("build/shaders/unlit.vert.spv"),
            renderer.resolveShaderPath("build/shaders/unlit.frag.spv")
        );
        mat->pipeline       = pipeline.pipeline;
        mat->pipelineLayout = pipeline.layout;

        if (!registry.get<Transform>(entity)) {
            registry.emplace<Transform>(entity, Transform{});
        }

        // 5. Rebuild static colliders for solid tiles
        std::vector<Entity> toDestroy;
        for (auto [child, hierarchy] : registry.view<HierarchyComponent>()) {
            if (hierarchy.parent == entity) {
                if (auto* nameComp = registry.get<Name>(child)) {
                    if (nameComp->value.rfind("TileCollider_", 0) == 0) {
                        toDestroy.push_back(child);
                    }
                }
            }
        }
        for (Entity child : toDestroy) registry.destroy(child);

        for (int y = 0; y < tilemap.height; ++y) {
            for (int x = 0; x < tilemap.width; ++x) {
                int cellIdx = y * tilemap.width + x;
                if (cellIdx >= static_cast<int>(tilemap.tiles.size())) continue;
                int tileIdx = tilemap.tiles[cellIdx];
                if (tileIdx < 0 || tileIdx >= static_cast<int>(tileset->tiles.size())) continue;
                if (!tileset->tiles[tileIdx].isSolid) continue;

                Entity colEnt = registry.create();
                registry.emplace<Name>(colEnt, Name{ "TileCollider_" + std::to_string(x) + "_" + std::to_string(y) });

                float cx = x * tilemap.tileSize + tilemap.tileSize * 0.5f;
                float cy = y * tilemap.tileSize + tilemap.tileSize * 0.5f;
                Transform t{};
                t.position = glm::vec3(cx, cy, 0.f);
                registry.emplace<Transform>(colEnt, std::move(t));

                registry.emplace<HierarchyComponent>(colEnt, HierarchyComponent{ entity });

                RigidBodyComponent rb{};
                rb.type = RigidBodyType::Static;
                registry.emplace<RigidBodyComponent>(colEnt, std::move(rb));

                ColliderComponent col{};
                col.shape   = ColliderShape::AABB;
                col.extents = glm::vec3(tilemap.tileSize * 0.5f, tilemap.tileSize * 0.5f, tilemap.tileSize * 0.5f);
                registry.emplace<ColliderComponent>(colEnt, std::move(col));
            }
        }

        tilemap.isDirty = false;
    }

} // namespace Engine
