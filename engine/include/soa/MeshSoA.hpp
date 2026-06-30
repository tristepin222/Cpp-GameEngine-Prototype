#pragma once
#include <vector>
#include "../ecs/components/mesh.hpp"
#include "../../src/core/VulkanBuffer.hpp"

struct MeshSoA {
    std::vector<uint32_t> ids;
    std::vector<std::vector<Vertex>> vertices;
    std::vector<std::vector<uint32_t>> indices;

    // Use VulkanBuffer instead of raw VkBuffer
    std::vector<VulkanBuffer> vertexBuffers;
    std::vector<VulkanBuffer> indexBuffers;

    size_t push(const std::vector<Vertex>& verts, const std::vector<uint32_t>& inds) {
        ids.push_back(ids.size());
        vertices.push_back(verts);
        indices.push_back(inds);

        vertexBuffers.emplace_back(); // placeholder VulkanBuffer
        indexBuffers.emplace_back();

        return ids.size() - 1;
    }
};