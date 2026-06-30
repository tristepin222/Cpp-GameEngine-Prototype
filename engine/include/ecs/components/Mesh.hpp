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

        // position
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(Vertex, position);

        // normal
        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].offset = offsetof(Vertex, normal);

        // uv
        attributes[2].binding = 0;
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[2].offset = offsetof(Vertex, uv);

        return attributes;
    }
};

struct Mesh {

    uint32_t id;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    VkBuffer vertexBuffer{ VK_NULL_HANDLE };
    VkDeviceMemory vertexBufferMemory{ VK_NULL_HANDLE };
    VkBuffer indexBuffer{ VK_NULL_HANDLE };
    VkDeviceMemory indexBufferMemory{ VK_NULL_HANDLE };

    Mesh(
        const std::vector<Vertex>& verts = {},
        const std::vector<uint32_t>& inds = {},
        VkBuffer vBuf = VK_NULL_HANDLE,
        VkDeviceMemory vMem = VK_NULL_HANDLE,
        VkBuffer iBuf = VK_NULL_HANDLE,
        VkDeviceMemory iMem = VK_NULL_HANDLE
    ) : vertices(verts), indices(inds), vertexBuffer(vBuf), vertexBufferMemory(vMem),
        indexBuffer(iBuf), indexBufferMemory(iMem) {
    }
};
