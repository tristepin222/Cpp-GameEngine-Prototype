#pragma once
#include <vector>
#include "../ecs/components/mesh.hpp"
#include "core/VulkanBuffer.hpp"

/**
 * @struct MeshSoA
 * @brief Structure of Arrays representing mesh buffers and geometry information.
 */
struct MeshSoA {
    /** @brief Unique mesh IDs. */
    std::vector<uint32_t> ids;
    /** @brief Arrays of vertices for each mesh. */
    std::vector<std::vector<Vertex>> vertices;
    /** @brief Arrays of indices for each mesh. */
    std::vector<std::vector<uint32_t>> indices;

    /** @brief Allocated Vulkan vertex buffers. */
    std::vector<VulkanBuffer> vertexBuffers;
    /** @brief Allocated Vulkan index buffers. */
    std::vector<VulkanBuffer> indexBuffers;

    /**
     * @brief Pushes a new mesh geometry configuration.
     * @param verts List of vertices.
     * @param inds List of indices.
     * @return The index of the added mesh configuration.
     */
    size_t push(const std::vector<Vertex>& verts, const std::vector<uint32_t>& inds) {
        ids.push_back(ids.size());
        vertices.push_back(verts);
        indices.push_back(inds);

        vertexBuffers.emplace_back(); // placeholder VulkanBuffer
        indexBuffers.emplace_back();

        return ids.size() - 1;
    }

    /**
     * @brief Safely destroys and clears all allocated Vulkan buffers.
     */
    void clear() {
        for (auto& vb : vertexBuffers) {
            vb.destroy();
        }
        for (auto& ib : indexBuffers) {
            ib.destroy();
        }
        vertexBuffers.clear();
        indexBuffers.clear();
        ids.clear();
        vertices.clear();
        indices.clear();
    }
};