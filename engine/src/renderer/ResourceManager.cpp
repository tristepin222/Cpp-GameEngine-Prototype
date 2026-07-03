#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "renderer/ResourceManager.hpp"
#include "renderer/VulkanRenderer.hpp"
#include "core/VulkanBuffer.hpp"
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <stdexcept>
#include <fstream>
#include <algorithm>

// --- Helper Functions for Vulkan Image Operations ---

static void transitionImageLayout(VulkanRenderer& renderer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = renderer.beginSingleUseCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("Unsupported image layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    renderer.endSingleUseCommands(commandBuffer);
}

static void copyBufferToImage(VulkanRenderer& renderer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = renderer.beginSingleUseCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    renderer.endSingleUseCommands(commandBuffer);
}

static void createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, ResourceManager& resManager) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan Image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = resManager.findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

// --- ResourceManager Implementation ---

uint32_t ResourceManager::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}

void ResourceManager::createTextureImageView(VkDevice device, Texture& texture) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &texture.imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image view!");
    }
}

void ResourceManager::createTextureSampler(VkDevice device, Texture& texture) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &texture.sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler!");
    }
}

void ResourceManager::createTextureImage(const std::string& path, VulkanRenderer& renderer, Texture& texture) {
    stbi_set_flip_vertically_on_load(true);
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("Failed to load texture file: " + path);
    }

    VkDeviceSize imageSize = texWidth * texHeight * 4;
    texture.width = texWidth;
    texture.height = texHeight;

    VulkanBuffer stagingBuffer;
    stagingBuffer.create(
        renderer.device.getDevice(),
        renderer.device.getPhysicalDevice(),
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    stagingBuffer.uploadData(pixels, imageSize);
    stbi_image_free(pixels);

    createImage(
        renderer.device.getDevice(),
        renderer.device.getPhysicalDevice(),
        texWidth,
        texHeight,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        texture.image,
        texture.memory,
        *this
    );

    transitionImageLayout(renderer, texture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(renderer, stagingBuffer.get(), texture.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    transitionImageLayout(renderer, texture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    stagingBuffer.destroy();
}

Texture* ResourceManager::loadTexture(const std::string& path, VulkanRenderer& renderer) {
    if (path.empty()) return &defaultWhiteTexture;

    auto it = textureCache.find(path);
    if (it != textureCache.end()) {
        return it->second.get();
    }

    auto texture = std::make_unique<Texture>();
    texture->path = path;

    try {
        createTextureImage(path, renderer, *texture);
        createTextureImageView(renderer.device.getDevice(), *texture);
        createTextureSampler(renderer.device.getDevice(), *texture);
        
        // Allocate and write descriptor set
        renderer.descriptors.allocateTextureDescriptorSet(texture->descriptorSet, texture->imageView, texture->sampler);
    } catch (const std::exception& e) {
        std::cerr << "[ResourceManager] Error loading texture " << path << ": " << e.what() << std::endl;
        return &defaultWhiteTexture; // Fallback
    }

    Texture* ptr = texture.get();
    textureCache[path] = std::move(texture);
    return ptr;
}

void ResourceManager::createDefaultWhiteTexture(VulkanRenderer& renderer) {
    defaultWhiteTexture.path = "";
    defaultWhiteTexture.width = 1;
    defaultWhiteTexture.height = 1;

    uint32_t whitePixel = 0xFFFFFFFF; // RGBA white
    VkDeviceSize imageSize = sizeof(whitePixel);

    VulkanBuffer stagingBuffer;
    stagingBuffer.create(
        renderer.device.getDevice(),
        renderer.device.getPhysicalDevice(),
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    stagingBuffer.uploadData(&whitePixel, imageSize);

    createImage(
        renderer.device.getDevice(),
        renderer.device.getPhysicalDevice(),
        1, 1,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        defaultWhiteTexture.image,
        defaultWhiteTexture.memory,
        *this
    );

    transitionImageLayout(renderer, defaultWhiteTexture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(renderer, stagingBuffer.get(), defaultWhiteTexture.image, 1, 1);
    transitionImageLayout(renderer, defaultWhiteTexture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    stagingBuffer.destroy();

    createTextureImageView(renderer.device.getDevice(), defaultWhiteTexture);
    createTextureSampler(renderer.device.getDevice(), defaultWhiteTexture);

    renderer.descriptors.allocateTextureDescriptorSet(defaultWhiteTexture.descriptorSet, defaultWhiteTexture.imageView, defaultWhiteTexture.sampler);
}

static void traverseNodes(cgltf_data* data, cgltf_node* node, const glm::mat4& parentTransform, Mesh& mesh, int targetPrimIndex, int& currentPrimIndex) {
    glm::mat4 local(1.0f);
    cgltf_node_transform_local(node, &local[0][0]);
    glm::mat4 global = parentTransform * local;

    if (node->mesh) {
        cgltf_mesh* gltfMesh = node->mesh;
        for (cgltf_size j = 0; j < gltfMesh->primitives_count; ++j) {
            cgltf_primitive& prim = gltfMesh->primitives[j];
            
            int primIdx = currentPrimIndex++;
            if (targetPrimIndex != -1 && primIdx != targetPrimIndex) {
                continue;
            }

            if (node->name) {
                mesh.nodeName = node->name;
            } else if (node->mesh && node->mesh->name) {
                mesh.nodeName = node->mesh->name;
            } else {
                mesh.nodeName = "MeshPart_" + std::to_string(primIdx);
            }

            // Find parent bone name by traversing up ancestors in glTF
            cgltf_node* ancestor = node->parent;
            while (ancestor) {
                bool isJoint = false;
                for (cgltf_size s = 0; s < data->skins_count; ++s) {
                    cgltf_skin& skin = data->skins[s];
                    for (cgltf_size jk = 0; jk < skin.joints_count; ++jk) {
                        if (skin.joints[jk] == ancestor) {
                            isJoint = true;
                            break;
                        }
                    }
                    if (isJoint) break;
                }
                if (isJoint && ancestor->name) {
                    mesh.parentBoneName = ancestor->name;
                    break;
                }
                ancestor = ancestor->parent;
            }

            size_t vert_start = mesh.vertices.size();

            cgltf_accessor* pos_accessor = nullptr;
            cgltf_accessor* norm_accessor = nullptr;
            cgltf_accessor* uv_accessor = nullptr;
            cgltf_accessor* joints_accessor = nullptr;
            cgltf_accessor* weights_accessor = nullptr;

            for (cgltf_size k = 0; k < prim.attributes_count; ++k) {
                cgltf_attribute& attr = prim.attributes[k];
                if (attr.type == cgltf_attribute_type_position) pos_accessor = attr.data;
                else if (attr.type == cgltf_attribute_type_normal) norm_accessor = attr.data;
                else if (attr.type == cgltf_attribute_type_texcoord) uv_accessor = attr.data;
                else if (attr.type == cgltf_attribute_type_joints) joints_accessor = attr.data;
                else if (attr.type == cgltf_attribute_type_weights) weights_accessor = attr.data;
            }

            if (!pos_accessor) continue;

            bool isSkinned = (joints_accessor != nullptr && weights_accessor != nullptr);
            mesh.isSkinned = isSkinned;

            size_t vert_count = pos_accessor->count;
            for (size_t k = 0; k < vert_count; ++k) {
                Vertex vert{};

                float pos[3]{};
                cgltf_accessor_read_float(pos_accessor, k, pos, 3);
                glm::vec3 position = glm::vec3(pos[0], pos[1], pos[2]);

                glm::vec3 normal(0.0f);
                if (norm_accessor) {
                    float norm[3]{};
                    cgltf_accessor_read_float(norm_accessor, k, norm, 3);
                    normal = glm::vec3(norm[0], norm[1], norm[2]);
                }

                // If not skinned, pre-transform static vertices by global node matrix
                if (!isSkinned) {
                    position = glm::vec3(global * glm::vec4(position, 1.0f));
                    if (norm_accessor) {
                        normal = glm::normalize(glm::vec3(glm::transpose(glm::inverse(global)) * glm::vec4(normal, 0.0f)));
                    }
                }

                vert.position = position;
                vert.normal = normal;

                if (uv_accessor) {
                    float uv[2]{};
                    cgltf_accessor_read_float(uv_accessor, k, uv, 2);
                    vert.uv = glm::vec2(uv[0], uv[1]);
                }

                if (joints_accessor) {
                    cgltf_uint joints[4]{};
                    cgltf_accessor_read_uint(joints_accessor, k, joints, 4);
                    vert.boneIDs = glm::ivec4(
                        static_cast<int>(joints[0]),
                        static_cast<int>(joints[1]),
                        static_cast<int>(joints[2]),
                        static_cast<int>(joints[3])
                    );
                }

                if (weights_accessor) {
                    float weights[4]{};
                    cgltf_accessor_read_float(weights_accessor, k, weights, 4);
                    vert.boneWeights = glm::vec4(weights[0], weights[1], weights[2], weights[3]);
                }



                mesh.vertices.push_back(vert);
            }

            // Read indices
            cgltf_accessor* index_accessor = prim.indices;
            if (index_accessor) {
                for (cgltf_size k = 0; k < index_accessor->count; ++k) {
                    mesh.indices.push_back(static_cast<uint32_t>(cgltf_accessor_read_index(index_accessor, k) + vert_start));
                }
            } else {
                for (size_t k = 0; k < vert_count; ++k) {
                    mesh.indices.push_back(static_cast<uint32_t>(vert_start + k));
                }
            }
        }
    }

    for (cgltf_size i = 0; i < node->children_count; ++i) {
        traverseNodes(data, node->children[i], global, mesh, targetPrimIndex, currentPrimIndex);
    }
}

Mesh ResourceManager::loadMesh(const std::string& path, VulkanRenderer& renderer, int primitiveIndex) {
    std::string cacheKey = path;
    if (primitiveIndex >= 0) {
        cacheKey = path + "#" + std::to_string(primitiveIndex);
    }

    auto it = meshCache.find(cacheKey);
    if (it != meshCache.end()) {
        return it->second;
    }

    Mesh mesh{};
    mesh.gltfPath = path;
    mesh.primitiveIndex = primitiveIndex;

    cgltf_options options{};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success) {
        throw std::runtime_error("Failed to parse glTF file: " + path);
    }

    result = cgltf_load_buffers(&options, data, path.c_str());
    if (result != cgltf_result_success) {
        cgltf_free(data);
        throw std::runtime_error("Failed to load glTF buffers: " + path);
    }

    int currentPrimIndex = 0;

    // Traverse scenes and nodes hierarchically
    if (data->scene) {
        for (cgltf_size i = 0; i < data->scene->nodes_count; ++i) {
            traverseNodes(data, data->scene->nodes[i], glm::mat4(1.0f), mesh, primitiveIndex, currentPrimIndex);
        }
    } else {
        for (cgltf_size i = 0; i < data->scenes_count; ++i) {
            cgltf_scene& scene = data->scenes[i];
            for (cgltf_size j = 0; j < scene.nodes_count; ++j) {
                traverseNodes(data, scene.nodes[j], glm::mat4(1.0f), mesh, primitiveIndex, currentPrimIndex);
            }
        }
    }

    cgltf_free(data);

    // Upload loaded geometry to renderer
    const size_t meshID = renderer.meshSoA.push(mesh.vertices, mesh.indices);
    renderer.uploadMesh(meshID);

    mesh.vertexBuffer = renderer.meshSoA.vertexBuffers[meshID].get();
    mesh.indexBuffer = renderer.meshSoA.indexBuffers[meshID].get();
    mesh.id = static_cast<uint32_t>(meshID);

    meshCache[cacheKey] = mesh;
    return mesh;
}

static void countPrimsRecursive(cgltf_node* node, int& count) {
    if (node->mesh) {
        count += static_cast<int>(node->mesh->primitives_count);
    }
    for (cgltf_size c = 0; c < node->children_count; ++c) {
        countPrimsRecursive(node->children[c], count);
    }
}

int ResourceManager::getMeshPrimitiveCount(const std::string& path) {
    cgltf_options options{};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success) {
        return 0;
    }
    
    int count = 0;
    for (cgltf_size i = 0; i < data->scenes_count; ++i) {
        cgltf_scene& scene = data->scenes[i];
        for (cgltf_size j = 0; j < scene.nodes_count; ++j) {
            countPrimsRecursive(scene.nodes[j], count);
        }
    }
    
    cgltf_free(data);
    return count;
}

void ResourceManager::cleanup(VkDevice device) {
    if (device == VK_NULL_HANDLE) return;

    // Free default white texture
    if (defaultWhiteTexture.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, defaultWhiteTexture.sampler, nullptr);
    }
    if (defaultWhiteTexture.imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, defaultWhiteTexture.imageView, nullptr);
    }
    if (defaultWhiteTexture.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, defaultWhiteTexture.image, nullptr);
    }
    if (defaultWhiteTexture.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, defaultWhiteTexture.memory, nullptr);
    }

    // Free cached textures
    for (auto& [_, texture] : textureCache) {
        if (texture->sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, texture->sampler, nullptr);
        }
        if (texture->imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, texture->imageView, nullptr);
        }
        if (texture->image != VK_NULL_HANDLE) {
            vkDestroyImage(device, texture->image, nullptr);
        }
        if (texture->memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, texture->memory, nullptr);
        }
    }
    textureCache.clear();
    meshCache.clear();
}

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

bool ResourceManager::loadSkeletonAndAnimations(const std::string& path, SkeletonComponent& skeleton, AnimatorComponent& animator) {
    cgltf_options options{};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success) {
        return false;
    }

    result = cgltf_load_buffers(&options, data, path.c_str());
    if (result != cgltf_result_success) {
        cgltf_free(data);
        return false;
    }

    if (data->skins_count == 0) {
        cgltf_free(data);
        return false;
    }

    cgltf_skin& skin = data->skins[0];
    std::unordered_map<cgltf_node*, int> nodeToJointIndex;

    // 1. Load Joint Hierarchies & IBMs
    skeleton.joints.resize(skin.joints_count);
    for (cgltf_size i = 0; i < skin.joints_count; ++i) {
        cgltf_node* jointNode = skin.joints[i];
        Joint& joint = skeleton.joints[i];
        joint.name = jointNode->name ? jointNode->name : "Joint_" + std::to_string(i);
        joint.parentIndex = -1;

        nodeToJointIndex[jointNode] = static_cast<int>(i);

        // Read Inverse Bind Matrix (IBM)
        if (skin.inverse_bind_matrices) {
            cgltf_accessor_read_float(skin.inverse_bind_matrices, i, &joint.inverseBindMatrix[0][0], 16);
        } else {
            joint.inverseBindMatrix = glm::mat4(1.0f);
        }

        // Read Local Transform matrix using cgltf helper (handles both matrix and TRS)
        glm::mat4 local(1.0f);
        cgltf_node_transform_local(jointNode, &local[0][0]);

        // Accumulate transforms of any non-joint parent nodes (like "Armature" or "Root")
        // up to the scene root or the first joint parent, and premultiply it.
        glm::mat4 nonJointParentTransform(1.0f);
        cgltf_node* parentNode = jointNode->parent;
        while (parentNode) {
            // Stop if the parent node is actually a joint in our skin
            if (nodeToJointIndex.find(parentNode) != nodeToJointIndex.end()) {
                break;
            }
            glm::mat4 parentLocal(1.0f);
            cgltf_node_transform_local(parentNode, &parentLocal[0][0]);
            nonJointParentTransform = parentLocal * nonJointParentTransform;
            parentNode = parentNode->parent;
        }

        joint.localTransform = nonJointParentTransform * local;

        // Decompose the compiled localTransform matrix to extract bind TRS values
        glm::vec3 skew{};
        glm::vec4 perspective{};
        glm::decompose(joint.localTransform, joint.bindScale, joint.bindRotation, joint.bindTranslation, skew, perspective);
    }

    // Resolve Parent Indices
    for (cgltf_size i = 0; i < skin.joints_count; ++i) {
        cgltf_node* jointNode = skin.joints[i];
        if (jointNode->parent) {
            auto it = nodeToJointIndex.find(jointNode->parent);
            if (it != nodeToJointIndex.end()) {
                skeleton.joints[i].parentIndex = it->second;
            }
        }
    }



    // 2. Load Animation Clips & Channels
    animator.animations.resize(data->animations_count);
    for (cgltf_size i = 0; i < data->animations_count; ++i) {
        cgltf_animation& gltfAnim = data->animations[i];
        AnimationClip& clip = animator.animations[i];
        clip.name = gltfAnim.name ? gltfAnim.name : "Animation_" + std::to_string(i);
        clip.duration = 0.0f;

        // Temporary map to group channels by jointIndex
        std::unordered_map<int, AnimationChannel> jointChannels;

        for (cgltf_size j = 0; j < gltfAnim.channels_count; ++j) {
            cgltf_animation_channel& channel = gltfAnim.channels[j];
            cgltf_node* targetNode = channel.target_node;
            if (!targetNode) continue;

            auto it = nodeToJointIndex.find(targetNode);
            if (it == nodeToJointIndex.end()) continue; // Channel targets a node outside our joint set

            int jointIdx = it->second;
            AnimationChannel& animChannel = jointChannels[jointIdx];
            animChannel.jointIndex = jointIdx;

            cgltf_animation_sampler* sampler = channel.sampler;
            size_t keyframeCount = sampler->input->count;

            for (size_t k = 0; k < keyframeCount; ++k) {
                float time = 0.0f;
                cgltf_accessor_read_float(sampler->input, k, &time, 1);
                clip.duration = std::max(clip.duration, time);

                float val[4]{};
                if (channel.target_path == cgltf_animation_path_type_translation) {
                    cgltf_accessor_read_float(sampler->output, k, val, 3);
                    animChannel.translationKeys.push_back({ time, glm::vec3(val[0], val[1], val[2]) });
                } else if (channel.target_path == cgltf_animation_path_type_rotation) {
                    cgltf_accessor_read_float(sampler->output, k, val, 4);
                    animChannel.rotationKeys.push_back({ time, glm::quat(val[3], val[0], val[1], val[2]) });
                } else if (channel.target_path == cgltf_animation_path_type_scale) {
                    cgltf_accessor_read_float(sampler->output, k, val, 3);
                    animChannel.scaleKeys.push_back({ time, glm::vec3(val[0], val[1], val[2]) });
                }
            }
        }

        // Move the grouped channels to clip.channels
        for (auto& [jointIdx, animChannel] : jointChannels) {
            // Sort keyframes by time
            std::sort(animChannel.translationKeys.begin(), animChannel.translationKeys.end(), [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
            std::sort(animChannel.rotationKeys.begin(), animChannel.rotationKeys.end(), [](const KeyframeRot& a, const KeyframeRot& b) { return a.time < b.time; });
            std::sort(animChannel.scaleKeys.begin(), animChannel.scaleKeys.end(), [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });



            clip.channels.push_back(std::move(animChannel));
        }


    }

    if (!animator.animations.empty()) {
        animator.activeAnimationIndex = 0; // Default active clip
    }

    cgltf_free(data);
    return true;
}
