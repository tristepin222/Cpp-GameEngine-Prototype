#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;

    Vertex(const glm::vec3& pos = {}, const glm::vec3& n = {}, const glm::vec2& u = {})
        : position(pos), normal(n), uv(u) {
    }

    // --- Vulkan binding description ---
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // per-vertex data
        return binding;
    }

    // --- Vulkan attribute descriptions ---
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributes(3);
        attributes[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) };
        attributes[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) };
        attributes[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) };
        return attributes;
    }
};

struct Mesh {
    uint32_t vertexCount = 0;   // Number of vertices
    uint32_t indexCount = 0;    // Number of indices
    uint32_t vertexOffset = 0;  // Offset in global vertex buffer
    uint32_t indexOffset = 0;   // Offset in global index buffer
};