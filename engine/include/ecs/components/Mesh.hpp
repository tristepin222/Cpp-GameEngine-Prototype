#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#include <string>

/**
 * @struct Vertex
 * @brief Represents a single vertex in a 3D model, containing position, normal, and UV coordinates.
 */
struct Vertex {
    /** @brief Position of the vertex in 3D space. */
    glm::vec3 position;
    /** @brief Normal vector of the vertex for lighting calculations. */
    glm::vec3 normal;
    /** @brief Texture coordinate (UV) of the vertex. */
    glm::vec2 uv;
    /** @brief Bone IDs that influence this vertex (Max 4). */
    glm::ivec4 boneIDs;
    /** @brief Bone weight influences (sums to 1.0). */
    glm::vec4 boneWeights;

    /**
     * @brief Construct a new Vertex object.
     * @param pos Position of the vertex.
     * @param n Normal vector of the vertex.
     * @param u Texture coordinate of the vertex.
     * @param bones Bone IDs.
     * @param weights Bone weights.
     */
    Vertex(const glm::vec3& pos = {}, const glm::vec3& n = {}, const glm::vec2& u = {},
           const glm::ivec4& bones = glm::ivec4(0), const glm::vec4& weights = glm::vec4(0.0f))
        : position(pos), normal(n), uv(u), boneIDs(bones), boneWeights(weights) {
    }

    // --- Vulkan binding description ---
    /**
     * @brief Retrieves the Vulkan vertex input binding description.
     * @return The binding description.
     */
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // per-vertex data
        return binding;
    }

    // --- Vulkan attribute descriptions ---
    /**
     * @brief Retrieves the Vulkan vertex input attribute descriptions.
     * @return A vector containing the attribute descriptions.
     */
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributes(5);

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

        // boneIDs
        attributes[3].binding = 0;
        attributes[3].location = 3;
        attributes[3].format = VK_FORMAT_R32G32B32A32_SINT; // ivec4 format
        attributes[3].offset = offsetof(Vertex, boneIDs);

        // boneWeights
        attributes[4].binding = 0;
        attributes[4].location = 4;
        attributes[4].format = VK_FORMAT_R32G32B32A32_SFLOAT; // vec4 format
        attributes[4].offset = offsetof(Vertex, boneWeights);

        return attributes;
    }
};

/**
 * @struct Mesh
 * @brief Represents a mesh component that holds geometry data and its Vulkan buffers.
 */
struct Mesh {

    /** @brief Unique identifier for this mesh. */
    uint32_t id;

    /** @brief Target glTF/glb file path. */
    std::string gltfPath;
    /** @brief Index of the primitive submesh in glTF. */
    int primitiveIndex = -1;
    /** @brief Name of the node associated with this mesh. */
    std::string nodeName;
    /** @brief Name of the parent bone/joint in glTF. */
    std::string parentBoneName;

    /** @brief CPU-side vertex data. */
    std::vector<Vertex> vertices;
    /** @brief CPU-side index data. */
    std::vector<uint32_t> indices;

    /** @brief Vulkan buffer holding the vertex data. */
    VkBuffer vertexBuffer{ VK_NULL_HANDLE };
    /** @brief GPU memory backing the vertex buffer. */
    VkDeviceMemory vertexBufferMemory{ VK_NULL_HANDLE };
    /** @brief Vulkan buffer holding the index data. */
    VkBuffer indexBuffer{ VK_NULL_HANDLE };
    /** @brief GPU memory backing the index buffer. */
    VkDeviceMemory indexBufferMemory{ VK_NULL_HANDLE };

    /** @brief Whether this mesh has skin weights and is deformed on GPU. */
    bool isSkinned = false;

    /**
     * @brief Construct a new Mesh object.
     * @param verts List of vertices.
     * @param inds List of indices.
     * @param vBuf Vulkan vertex buffer.
     * @param vMem GPU memory for vertex buffer.
     * @param iBuf Vulkan index buffer.
     * @param iMem GPU memory for index buffer.
     */
    Mesh(
        const std::vector<Vertex>& verts = {},
        const std::vector<uint32_t>& inds = {},
        VkBuffer vBuf = VK_NULL_HANDLE,
        VkDeviceMemory vMem = VK_NULL_HANDLE,
        VkBuffer iBuf = VK_NULL_HANDLE,
        VkDeviceMemory iMem = VK_NULL_HANDLE
    ) : vertices(verts), indices(inds), vertexBuffer(vBuf), vertexBufferMemory(vMem),
        indexBuffer(iBuf), indexBufferMemory(iMem), isSkinned(false) {
    }
};
