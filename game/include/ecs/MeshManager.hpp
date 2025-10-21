#pragma once
#include "../../src/core/Buffer.hpp"
#include "../ecs/components/Mesh.hpp"
#include <vulkan/vulkan.h>
#include <vector>

struct MeshManager {
    std::vector<Vertex> vertices;        // CPU-side vertex data (accumulated)
    std::vector<uint32_t> indices;       // CPU-side index data (accumulated)
    std::vector<Mesh> meshes;            // Metadata for each mesh (offsets + counts)

    VulkanBuffer globalVertexBuffer;     // GPU buffer containing all vertices
    VulkanBuffer globalIndexBuffer;      // GPU buffer containing all indices

    VkDevice device{};
    VkPhysicalDevice physicalDevice{};

    MeshManager() = default;
    MeshManager(VkDevice dev, VkPhysicalDevice phys)
        : device(dev), physicalDevice(phys) {}

    // --- Adds a mesh to CPU-side data arrays ---
    Mesh& addMesh(const std::vector<Vertex>& v, const std::vector<uint32_t>& i) {
        Mesh mesh{};
        mesh.vertexOffset = static_cast<uint32_t>(vertices.size());
        mesh.indexOffset = static_cast<uint32_t>(indices.size());
        mesh.vertexCount = static_cast<uint32_t>(v.size());
        mesh.indexCount = static_cast<uint32_t>(i.size());

        vertices.insert(vertices.end(), v.begin(), v.end());
        indices.insert(indices.end(), i.begin(), i.end());
        meshes.push_back(mesh);

        return meshes.back();
    }

    // --- Uploads all accumulated meshes to GPU in one go ---
    void uploadToGPU(VkQueue transferQueue, VkCommandPool commandPool) {
        if (vertices.empty() || indices.empty()) return;

        VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
        VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();

        // Create staging buffers (CPU visible)
        VulkanBuffer vertexStaging = VulkanBuffer::createBuffer(
            device, physicalDevice,
            vertexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        VulkanBuffer indexStaging = VulkanBuffer::createBuffer(
            device, physicalDevice,
            indexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        // Map and copy data
        void* data;
        vkMapMemory(device, vertexStaging.getMemory(), 0, vertexBufferSize, 0, &data);
        memcpy(data, vertices.data(), static_cast<size_t>(vertexBufferSize));
        vkUnmapMemory(device, vertexStaging.getMemory());

        vkMapMemory(device, indexStaging.getMemory(), 0, indexBufferSize, 0, &data);
        memcpy(data, indices.data(), static_cast<size_t>(indexBufferSize));
        vkUnmapMemory(device, indexStaging.getMemory());

        // Create GPU-local buffers
        globalVertexBuffer = VulkanBuffer::createBuffer(
            device, physicalDevice,
            vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        globalIndexBuffer = VulkanBuffer::createBuffer(
            device, physicalDevice,
            indexBufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        // Copy staging -> device local
        VulkanBuffer::copyBuffer(device, transferQueue, commandPool,
            vertexStaging.get(), globalVertexBuffer.get(), vertexBufferSize);

        VulkanBuffer::copyBuffer(device, transferQueue, commandPool,
            indexStaging.get(), globalIndexBuffer.get(), indexBufferSize);

        // Cleanup staging buffers (we don't need them anymore)
        vertexStaging.destroy();
        indexStaging.destroy();
    }

    void cleanup() {
        globalVertexBuffer.destroy();
        globalIndexBuffer.destroy();
        vertices.clear();
        indices.clear();
        meshes.clear();
    }

    const Mesh& getMesh(uint32_t id) const { return meshes[id]; }
};
