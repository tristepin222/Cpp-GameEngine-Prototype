#pragma once
#include "ecs/System.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Skeleton.hpp"
#include "ecs/components/Animator.hpp"
#include "renderer/VulkanRenderer.hpp"
#include "core/VulkanBuffer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

/**
 * @class AnimationSystem
 * @brief System that processes active skeletal animations, interpolating poses and resolving joint trees.
 */
class AnimationSystem : public System {
public:
    AnimationSystem(Registry& reg, VulkanRenderer& renderer)
        : registry(reg), renderer(renderer) {}

        /**
         * @brief Updates skeletal poses for all animated entities.
         * @param dt Delta time in seconds.
         */
        void update(float dt) override {
            for (auto [entity, skeleton, animator] : registry.view<SkeletonComponent, AnimatorComponent>()) {
                updateEntityAnimation(skeleton, animator, dt);
            }
        }

    private:
        /**
         * @brief Updates the animation timing and calculates local/global joint matrices for an entity.
         */
        void updateEntityAnimation(SkeletonComponent& skeleton, AnimatorComponent& animator, float dt) {
            if (skeleton.joints.empty()) return;

            // 1. Advance Playback Time
            AnimationClip* clip = nullptr;
            if (!animator.animations.empty() && 
                animator.activeAnimationIndex >= 0 && 
                animator.activeAnimationIndex < static_cast<int>(animator.animations.size())) {
                clip = &animator.animations[animator.activeAnimationIndex];
            }

            if (clip) {
                animator.currentTime += dt * animator.playbackSpeed;
                if (animator.loop) {
                    if (clip->duration > 0.0f) {
                        animator.currentTime = std::fmod(animator.currentTime, clip->duration);
                    } else {
                        animator.currentTime = 0.0f;
                    }
                } else {
                    animator.currentTime = std::min(animator.currentTime, clip->duration);
                }

                // 2. Interpolate Keyframes to rebuild local transforms
                for (const auto& channel : clip->channels) {
                    if (channel.jointIndex < 0 || channel.jointIndex >= static_cast<int>(skeleton.joints.size())) {
                        continue;
                    }

                    glm::vec3 translation = interpolateTranslation(channel.translationKeys, animator.currentTime);
                    glm::quat rotation = interpolateRotation(channel.rotationKeys, animator.currentTime);
                    glm::vec3 scale = interpolateScale(channel.scaleKeys, animator.currentTime);

                    glm::mat4 tMat = glm::translate(glm::mat4(1.0f), translation);
                    glm::mat4 rMat = glm::toMat4(rotation);
                    glm::mat4 sMat = glm::scale(glm::mat4(1.0f), scale);

                    skeleton.joints[channel.jointIndex].localTransform = tMat * rMat * sMat;
                }
            }

            // 3. Resolve Hierarchical Global Transforms (Forward Kinematics)
            std::vector<glm::mat4> globalTransforms(skeleton.joints.size(), glm::mat4(1.0f));
            for (size_t i = 0; i < skeleton.joints.size(); ++i) {
                const auto& joint = skeleton.joints[i];
                if (joint.parentIndex == -1) {
                    globalTransforms[i] = joint.localTransform;
                } else {
                    globalTransforms[i] = globalTransforms[joint.parentIndex] * joint.localTransform;
                }
            }

            // 4. Generate Final Joint Offset Matrix Palette
            skeleton.jointMatrices.resize(skeleton.joints.size());
            for (size_t i = 0; i < skeleton.joints.size(); ++i) {
                skeleton.jointMatrices[i] = globalTransforms[i] * skeleton.joints[i].inverseBindMatrix;
            }

            // 5. Upload Joint Matrices Palette to GPU Buffer
            if (!skeleton.jointMatrices.empty()) {
                VkDeviceSize bufferSize = skeleton.jointMatrices.size() * sizeof(glm::mat4);

                if (!skeleton.gpuBuffer || skeleton.gpuBuffer->getSize() < bufferSize) {
                    if (skeleton.gpuBuffer) {
                        skeleton.gpuBuffer->destroy();
                    }
                    skeleton.gpuBuffer = std::make_shared<VulkanBuffer>();
                    skeleton.gpuBuffer->create(
                        renderer.device.getDevice(),
                        renderer.device.getPhysicalDevice(),
                        bufferSize,
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                    );

                    renderer.descriptors.allocateJointsDescriptorSet(
                        skeleton.descriptorSet,
                        skeleton.gpuBuffer->get(),
                        bufferSize
                    );
                }

                skeleton.gpuBuffer->uploadData(skeleton.jointMatrices.data(), bufferSize);
            }
        }

        // --- Keyframe Interpolation Helpers ---

        glm::vec3 interpolateTranslation(const std::vector<Keyframe>& keys, float time) {
            if (keys.empty()) return glm::vec3(0.0f);
            if (keys.size() == 1 || time <= keys.front().time) return keys.front().value;
            if (time >= keys.back().time) return keys.back().value;

            size_t index = 0;
            for (size_t i = 0; i < keys.size() - 1; ++i) {
                if (time >= keys[i].time && time <= keys[i+1].time) {
                    index = i;
                    break;
                }
            }

            const auto& k1 = keys[index];
            const auto& k2 = keys[index+1];
            float factor = (time - k1.time) / (k2.time - k1.time);
            return glm::mix(k1.value, k2.value, factor);
        }

        glm::quat interpolateRotation(const std::vector<KeyframeRot>& keys, float time) {
            if (keys.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            if (keys.size() == 1 || time <= keys.front().time) return keys.front().value;
            if (time >= keys.back().time) return keys.back().value;

            size_t index = 0;
            for (size_t i = 0; i < keys.size() - 1; ++i) {
                if (time >= keys[i].time && time <= keys[i+1].time) {
                    index = i;
                    break;
                }
            }

            const auto& k1 = keys[index];
            const auto& k2 = keys[index+1];
            float factor = (time - k1.time) / (k2.time - k1.time);
            return glm::slerp(k1.value, k2.value, factor);
        }

        glm::vec3 interpolateScale(const std::vector<Keyframe>& keys, float time) {
            if (keys.empty()) return glm::vec3(1.0f);
            if (keys.size() == 1 || time <= keys.front().time) return keys.front().value;
            if (time >= keys.back().time) return keys.back().value;

            size_t index = 0;
            for (size_t i = 0; i < keys.size() - 1; ++i) {
                if (time >= keys[i].time && time <= keys[i+1].time) {
                    index = i;
                    break;
                }
            }

            const auto& k1 = keys[index];
            const auto& k2 = keys[index+1];
            float factor = (time - k1.time) / (k2.time - k1.time);
            return glm::mix(k1.value, k2.value, factor);
        }

        Registry& registry;
        VulkanRenderer& renderer;
    };

