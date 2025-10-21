// Renderer.h (or a separate Uniforms.h)
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstddef> // for offsetof

struct CameraUBO {
    glm::mat4 view;
    glm::mat4 proj;
};


struct PushConstants {
    glm::mat4 model;       // 64 bytes
    glm::vec4 color;       // 16 bytes
    glm::mat4 viewProj;    // 64 bytes
    glm::vec3 camPos;      // 12 bytes
    float scale;           // 4 bytes
    float fade;            // 4 bytes

    // optional padding to satisfy std140 alignment if needed
    float padding[2];    // pad to 16-byte multiple if necessary
};

// ---------------------------------------------------------
// Structure grouping entities that share same mesh/material
// ---------------------------------------------------------
// 

struct RenderBatch {
    uint32_t meshID;
    uint32_t materialID;
    size_t firstIndex;   // offset into batchEntityIndices
    size_t count;        // number of entities in this batch
};

// RAII wrapper for descriptor pool
class DescriptorPool {
public:
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    DescriptorPool(VkDevice dev, uint32_t framesInFlight) : device(dev) {
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, framesInFlight }
        };
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = framesInFlight;
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor pool");
    }

    ~DescriptorPool() {
        if (pool) vkDestroyDescriptorPool(device, pool, nullptr);
    }

    operator VkDescriptorPool() const { return pool; }
};

struct InstanceDataSoA {
    std::vector<glm::mat4> models;
    std::vector<uint32_t> materialIDs;
    std::vector<uint32_t> meshIDs;

    void clear() {
        models.clear();
        materialIDs.clear();
        meshIDs.clear();
    }

    void push(const InstanceData& inst) {
        models.push_back(inst.modelMatrix);
        materialIDs.push_back(inst.materialID);
        meshIDs.push_back(inst.meshID);
    }

    size_t size() const { return models.size(); }
};