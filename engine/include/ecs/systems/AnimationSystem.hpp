#pragma once
#include "ecs/System.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/Skeleton.hpp"
#include "ecs/components/Animator.hpp"
#include "ecs/components/AnimationController.hpp"
#include "ecs/components/IKSolver.hpp"
#include "renderer/VulkanRenderer.hpp"
#include "core/VulkanBuffer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <fstream>

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
        struct JointPose {
            glm::vec3 translation;
            glm::quat rotation;
            glm::vec3 scale;
        };

        /**
         * @brief Updates skeletal poses for all animated entities.
         * @param dt Delta time in seconds.
         */
        void update(float dt) override {
            // 0. Synchronize child animators & controllers with parent animators
            for (auto [entity, hierarchy, animator] : registry.view<HierarchyComponent, AnimatorComponent>()) {
                if (hierarchy.parent.getId() != Entity::INVALID_ENTITY && registry.isValid(hierarchy.parent)) {
                    if (auto* parentAnimator = registry.get<AnimatorComponent>(hierarchy.parent)) {
                        animator.activeAnimationIndex = parentAnimator->activeAnimationIndex;
                        animator.currentTime = parentAnimator->currentTime;
                        animator.playbackSpeed = parentAnimator->playbackSpeed;
                        animator.loop = parentAnimator->loop;
                    }
                }
            }

            for (auto [entity, hierarchy, controller] : registry.view<HierarchyComponent, AnimationControllerComponent>()) {
                if (hierarchy.parent.getId() != Entity::INVALID_ENTITY && registry.isValid(hierarchy.parent)) {
                    if (auto* parentController = registry.get<AnimationControllerComponent>(hierarchy.parent)) {
                        controller.currentState = parentController->currentState;
                        controller.currentStateTime = parentController->currentStateTime;
                        controller.fromState = parentController->fromState;
                        controller.fromStateTime = parentController->fromStateTime;
                        controller.crossfadeProgress = parentController->crossfadeProgress;
                        controller.crossfadeDuration = parentController->crossfadeDuration;
                        controller.isCrossfading = parentController->isCrossfading;
                        controller.parameters = parentController->parameters;
                    }
                }
            }

            // 1. First Pass: Update all active Animation Controllers (State Machines, transitions)
            for (auto [entity, controller, animator] : registry.view<AnimationControllerComponent, AnimatorComponent>()) {
                updateController(controller, animator, dt);
            }

            // 2. Second Pass: Process skeletal transforms, blending, and IK adjustments
            for (auto [entity, skeleton, animator] : registry.view<SkeletonComponent, AnimatorComponent>()) {
                updateEntityAnimation(entity, skeleton, animator, dt);
            }
        }

    private:
        /**
         * @brief Updates high-level state machine progression and checks transitions.
         */
        void updateController(AnimationControllerComponent& controller, AnimatorComponent& animator, float dt) {
            if (controller.currentState.empty() && !controller.states.empty()) {
                controller.currentState = controller.states[0].name;
                controller.currentStateTime = 0.0f;
            }

            if (controller.currentState.empty()) return;

            // Advance play time for current state
            const AnimationState* currState = nullptr;
            for (const auto& state : controller.states) {
                if (state.name == controller.currentState) {
                    currState = &state;
                    break;
                }
            }

            if (currState) {
                controller.currentStateTime += dt * currState->speed;
            }

            // Advance crossfade timing if active
            if (controller.isCrossfading) {
                const AnimationState* fState = nullptr;
                for (const auto& state : controller.states) {
                    if (state.name == controller.fromState) {
                        fState = &state;
                        break;
                    }
                }
                if (fState) {
                    controller.fromStateTime += dt * fState->speed;
                }
                
                controller.crossfadeProgress += dt;
                if (controller.crossfadeProgress >= controller.crossfadeDuration) {
                    controller.isCrossfading = false;
                    controller.fromState.clear();
                }
            }

            // Check transition rules (only if not currently crossfading)
            if (!controller.isCrossfading) {
                for (const auto& trans : controller.transitions) {
                    if (trans.fromState == controller.currentState) {
                        bool allConditionsMet = true;
                        for (const auto& cond : trans.conditions) {
                            float paramVal = 0.0f;
                            auto it = controller.parameters.find(cond.parameterName);
                            if (it != controller.parameters.end()) {
                                paramVal = it->second;
                            }

                            if (cond.op == ">") {
                                if (!(paramVal > cond.value)) allConditionsMet = false;
                            } else if (cond.op == "<") {
                                if (!(paramVal < cond.value)) allConditionsMet = false;
                            } else if (cond.op == "==") {
                                if (!(std::abs(paramVal - cond.value) < 1e-4f)) allConditionsMet = false;
                            } else {
                                allConditionsMet = false;
                            }
                        }

                        if (allConditionsMet && !trans.conditions.empty()) {
                            // Trigger transition!
                            controller.fromState = controller.currentState;
                            controller.fromStateTime = controller.currentStateTime;
                            controller.currentState = trans.toState;
                            controller.currentStateTime = 0.0f;
                            
                            controller.targetState = trans.toState;
                            controller.crossfadeProgress = 0.0f;
                            controller.crossfadeDuration = trans.crossfadeDuration;
                            controller.isCrossfading = true;
                            break; // only transition once per tick
                        }
                    }
                }
            }
        }

        const AnimationClip* findClip(const AnimatorComponent& animator, const std::string& name) {
            for (const auto& clip : animator.animations) {
                if (clip.name == name) return &clip;
            }
            return nullptr;
        }

        void sampleClip(const AnimationClip& clip, float time, const SkeletonComponent& skeleton, std::vector<JointPose>& outPose) {
            for (const auto& channel : clip.channels) {
                if (channel.jointIndex < 0 || channel.jointIndex >= static_cast<int>(skeleton.joints.size())) {
                    continue;
                }
                auto& joint = skeleton.joints[channel.jointIndex];
                outPose[channel.jointIndex].translation = interpolateTranslation(channel.translationKeys, time, joint.bindTranslation);
                outPose[channel.jointIndex].rotation = interpolateRotation(channel.rotationKeys, time, joint.bindRotation);
                outPose[channel.jointIndex].scale = interpolateScale(channel.scaleKeys, time, joint.bindScale);
            }
        }

        void blendPoses(const std::vector<JointPose>& poseA, const std::vector<JointPose>& poseB, float weight, std::vector<JointPose>& outPose) {
            for (size_t i = 0; i < outPose.size(); ++i) {
                outPose[i].translation = glm::mix(poseA[i].translation, poseB[i].translation, weight);
                outPose[i].rotation = glm::slerp(poseA[i].rotation, poseB[i].rotation, weight);
                outPose[i].scale = glm::mix(poseA[i].scale, poseB[i].scale, weight);
            }
        }

        void evaluateStatePose(const AnimationControllerComponent& controller, const AnimationState& state, float stateTime, const AnimatorComponent& animator, const SkeletonComponent& skeleton, std::vector<JointPose>& outPose) {
            if (state.isBlendTree) {
                const auto& tree = state.blendTree;
                if (tree.nodes.empty()) return;

                float paramVal = 0.0f;
                auto it = controller.parameters.find(tree.parameterName);
                if (it != controller.parameters.end()) {
                    paramVal = it->second;
                }

                // Sort nodes by threshold for robust linear 1D interpolation
                std::vector<BlendNode> sortedNodes = tree.nodes;
                std::sort(sortedNodes.begin(), sortedNodes.end(), [](const BlendNode& a, const BlendNode& b) {
                    return a.threshold < b.threshold;
                });

                if (paramVal <= sortedNodes.front().threshold) {
                    if (const AnimationClip* clip = findClip(animator, sortedNodes.front().clipName)) {
                        sampleClip(*clip, stateTime, skeleton, outPose);
                    }
                } else if (paramVal >= sortedNodes.back().threshold) {
                    if (const AnimationClip* clip = findClip(animator, sortedNodes.back().clipName)) {
                        sampleClip(*clip, stateTime, skeleton, outPose);
                    }
                } else {
                    for (size_t i = 0; i < sortedNodes.size() - 1; ++i) {
                        if (paramVal >= sortedNodes[i].threshold && paramVal <= sortedNodes[i+1].threshold) {
                            float t = (paramVal - sortedNodes[i].threshold) / (sortedNodes[i+1].threshold - sortedNodes[i].threshold);
                            
                            std::vector<JointPose> poseA(skeleton.joints.size());
                            std::vector<JointPose> poseB(skeleton.joints.size());
                            for (size_t j = 0; j < skeleton.joints.size(); ++j) {
                                poseA[j] = { skeleton.joints[j].bindTranslation, skeleton.joints[j].bindRotation, skeleton.joints[j].bindScale };
                                poseB[j] = poseA[j];
                            }

                            if (const AnimationClip* clipA = findClip(animator, sortedNodes[i].clipName)) {
                                sampleClip(*clipA, stateTime, skeleton, poseA);
                            }
                            if (const AnimationClip* clipB = findClip(animator, sortedNodes[i+1].clipName)) {
                                sampleClip(*clipB, stateTime, skeleton, poseB);
                            }

                            blendPoses(poseA, poseB, t, outPose);
                            break;
                        }
                    }
                }
            } else {
                if (const AnimationClip* clip = findClip(animator, state.clipName)) {
                    sampleClip(*clip, stateTime, skeleton, outPose);
                }
            }
        }

        int findJointIndex(const SkeletonComponent& skeleton, const std::string& name) {
            for (size_t i = 0; i < skeleton.joints.size(); ++i) {
                if (skeleton.joints[i].name == name) {
                    return static_cast<int>(i);
                }
            }
            return -1;
        }

        glm::quat rotationBetweenVectors(glm::vec3 u, glm::vec3 v) {
            u = glm::normalize(u);
            v = glm::normalize(v);
            float cosTheta = glm::dot(u, v);
            glm::vec3 rotationAxis;

            if (cosTheta < -1.0f + 1e-6f) {
                rotationAxis = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), u);
                if (glm::dot(rotationAxis, rotationAxis) < 0.01f) {
                    rotationAxis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), u);
                }
                rotationAxis = glm::normalize(rotationAxis);
                return glm::angleAxis(glm::radians(180.0f), rotationAxis);
            }

            rotationAxis = glm::cross(u, v);
            float s = std::sqrt((1.0f + cosTheta) * 2.0f);
            float invs = 1.0f / s;

            return glm::quat(
                s * 0.5f,
                rotationAxis.x * invs,
                rotationAxis.y * invs,
                rotationAxis.z * invs
            );
        }

        void solve2BoneIK(SkeletonComponent& skeleton, const IKSolverComponent& ik, std::vector<glm::mat4>& globalTransforms) {
            int idxA = findJointIndex(skeleton, ik.startJointName);
            int idxB = findJointIndex(skeleton, ik.middleJointName);
            int idxC = findJointIndex(skeleton, ik.endJointName);

            if (idxA == -1 || idxB == -1 || idxC == -1) return;

            glm::mat4 origGlobalA = globalTransforms[idxA];
            glm::mat4 origGlobalB = globalTransforms[idxB];
            glm::mat4 origGlobalC = globalTransforms[idxC];

            glm::vec3 P_A = glm::vec3(origGlobalA[3]);
            glm::vec3 P_B = glm::vec3(origGlobalB[3]);
            glm::vec3 P_C = glm::vec3(origGlobalC[3]);
            glm::vec3 P_T = ik.targetPosition;
            glm::vec3 P_P = ik.polePosition;

            float L1 = glm::distance(P_B, P_A);
            float L2 = glm::distance(P_C, P_B);
            float D = glm::distance(P_T, P_A);

            if (L1 < 1e-4f || L2 < 1e-4f) return;

            if (D > L1 + L2 - 0.001f) {
                P_T = P_A + glm::normalize(P_T - P_A) * (L1 + L2 - 0.001f);
                D = L1 + L2 - 0.001f;
            }
            if (D < 0.001f) {
                P_T = P_A + glm::vec3(0.0f, 0.0f, 0.001f);
                D = 0.001f;
            }

            float cosAlpha = (L1 * L1 + D * D - L2 * L2) / (2.0f * L1 * D);
            cosAlpha = glm::clamp(cosAlpha, -1.0f, 1.0f);
            float alpha = std::acos(cosAlpha);

            glm::vec3 d_AT = glm::normalize(P_T - P_A);
            glm::vec3 v_AP = P_P - P_A;
            glm::vec3 N = glm::cross(d_AT, v_AP);
            if (glm::dot(N, N) < 1e-6f) {
                N = glm::cross(d_AT, glm::vec3(0.0f, 1.0f, 0.0f));
                if (glm::dot(N, N) < 1e-6f) {
                    N = glm::cross(d_AT, glm::vec3(1.0f, 0.0f, 0.0f));
                }
            }
            N = glm::normalize(N);
            glm::vec3 B_dir = glm::normalize(glm::cross(N, d_AT));

            glm::vec3 P_B_new = P_A + (d_AT * std::cos(alpha) + B_dir * std::sin(alpha)) * L1;
            glm::vec3 P_C_new = P_T;

            glm::vec3 origDirA = P_B - P_A;
            glm::vec3 targetDirA = P_B_new - P_A;
            glm::quat rotA = rotationBetweenVectors(origDirA, targetDirA);
            glm::mat3 rotPartA = glm::mat3(glm::toMat4(rotA)) * glm::mat3(origGlobalA);
            glm::mat4 newGlobalA = glm::mat4(rotPartA);
            newGlobalA[3] = glm::vec4(P_A, 1.0f);

            glm::vec3 origDirB = P_C - P_B;
            glm::vec3 targetDirB = P_C_new - P_B_new;
            glm::quat rotB = rotationBetweenVectors(origDirB, targetDirB);
            glm::mat3 rotPartB = glm::mat3(glm::toMat4(rotB)) * glm::mat3(origGlobalB);
            glm::mat4 newGlobalB = glm::mat4(rotPartB);
            newGlobalB[3] = glm::vec4(P_B_new, 1.0f);

            glm::mat4 newGlobalC = origGlobalC;
            newGlobalC[3] = glm::vec4(P_C_new, 1.0f);

            float w = glm::clamp(ik.targetWeight, 0.0f, 1.0f);
            globalTransforms[idxA] = origGlobalA * (1.0f - w) + newGlobalA * w;
            globalTransforms[idxB] = origGlobalB * (1.0f - w) + newGlobalB * w;
            globalTransforms[idxC] = origGlobalC * (1.0f - w) + newGlobalC * w;

            auto updateLocal = [&](int idx) {
                int parentIdx = skeleton.joints[idx].parentIndex;
                if (parentIdx != -1) {
                    skeleton.joints[idx].localTransform = glm::inverse(globalTransforms[parentIdx]) * globalTransforms[idx];
                } else {
                    skeleton.joints[idx].localTransform = globalTransforms[idx];
                }
            };

            updateLocal(idxA);
            updateLocal(idxB);
            updateLocal(idxC);

            std::vector<bool> resolved(skeleton.joints.size(), false);
            resolved[idxA] = true;
            resolved[idxB] = true;
            resolved[idxC] = true;

            std::function<void(int)> propagate = [&](int jointIdx) {
                int parentIdx = skeleton.joints[jointIdx].parentIndex;
                if (parentIdx != -1 && resolved[parentIdx] && !resolved[jointIdx]) {
                    globalTransforms[jointIdx] = globalTransforms[parentIdx] * skeleton.joints[jointIdx].localTransform;
                    resolved[jointIdx] = true;
                }
                for (size_t i = 0; i < skeleton.joints.size(); ++i) {
                    if (skeleton.joints[i].parentIndex == jointIdx) {
                        propagate(static_cast<int>(i));
                    }
                }
            };

            propagate(idxA);
            propagate(idxB);
            propagate(idxC);
        }

        void solveFABRIK(SkeletonComponent& skeleton, const IKSolverComponent& ik, std::vector<glm::mat4>& globalTransforms) {
            if (ik.jointChainNames.size() < 2) return;

            std::vector<int> chainIndices;
            for (const auto& name : ik.jointChainNames) {
                int idx = findJointIndex(skeleton, name);
                if (idx != -1) {
                    chainIndices.push_back(idx);
                }
            }

            if (chainIndices.size() < 2) return;

            int n = static_cast<int>(chainIndices.size()) - 1;
            std::vector<glm::vec3> positions(chainIndices.size());
            std::vector<float> boneLengths(n);
            float totalLength = 0.0f;

            for (size_t i = 0; i < chainIndices.size(); ++i) {
                positions[i] = glm::vec3(globalTransforms[chainIndices[i]][3]);
                if (i > 0) {
                    boneLengths[i - 1] = glm::distance(positions[i], positions[i - 1]);
                    totalLength += boneLengths[i - 1];
                }
            }

            glm::vec3 target = ik.targetPosition;
            glm::vec3 origin = positions[0];

            float distToTarget = glm::distance(origin, target);
            if (distToTarget > totalLength) {
                for (int i = 0; i < n; ++i) {
                    glm::vec3 dir = glm::normalize(target - positions[i]);
                    positions[i + 1] = positions[i] + dir * boneLengths[i];
                }
            } else {
                for (int iter = 0; iter < ik.maxIterations; ++iter) {
                    float err = glm::distance(positions[n], target);
                    if (err < ik.tolerance) break;

                    positions[n] = target;
                    for (int i = n - 1; i >= 0; --i) {
                        glm::vec3 dir = glm::normalize(positions[i] - positions[i + 1]);
                        positions[i] = positions[i + 1] + dir * boneLengths[i];
                    }

                    positions[0] = origin;
                    for (int i = 0; i < n; ++i) {
                        glm::vec3 dir = glm::normalize(positions[i + 1] - positions[i]);
                        positions[i + 1] = positions[i] + dir * boneLengths[i];
                    }
                }
            }

            for (int i = 0; i < n; ++i) {
                int idxCurrent = chainIndices[i];
                int idxNext = chainIndices[i + 1];

                glm::mat4 origGlobalCurrent = globalTransforms[idxCurrent];
                // Compute current position of idxNext by evaluating relative to current idxCurrent transform
                glm::mat4 currentGlobalNext = origGlobalCurrent * skeleton.joints[idxNext].localTransform;

                glm::vec3 P_current = glm::vec3(origGlobalCurrent[3]);
                glm::vec3 P_next = glm::vec3(currentGlobalNext[3]);

                glm::vec3 origDir = P_next - P_current;
                glm::vec3 solvedDir = positions[i + 1] - positions[i];

                if (glm::dot(origDir, origDir) < 1e-6f || glm::dot(solvedDir, solvedDir) < 1e-6f) continue;

                glm::quat rot = rotationBetweenVectors(origDir, solvedDir);
                float w = glm::clamp(ik.targetWeight, 0.0f, 1.0f);
                glm::quat blendedRot = glm::slerp(glm::quat(1.0f, 0.0f, 0.0f, 0.0f), rot, w);

                // Rotate the current joint
                glm::mat3 rotPart = glm::mat3(glm::toMat4(blendedRot)) * glm::mat3(origGlobalCurrent);
                glm::mat4 newGlobal = glm::mat4(rotPart);
                newGlobal[3] = glm::vec4(glm::mix(P_current, positions[i], w), 1.0f);

                globalTransforms[idxCurrent] = newGlobal;

                // Update local transform of current joint relative to parent
                int parentIdx = skeleton.joints[idxCurrent].parentIndex;
                if (parentIdx != -1) {
                    skeleton.joints[idxCurrent].localTransform = glm::inverse(globalTransforms[parentIdx]) * globalTransforms[idxCurrent];
                } else {
                    skeleton.joints[idxCurrent].localTransform = globalTransforms[idxCurrent];
                }

                // Recalculate next joint global transform based on the newly rotated current joint
                globalTransforms[idxNext] = globalTransforms[idxCurrent] * skeleton.joints[idxNext].localTransform;
            }

            std::vector<bool> resolved(skeleton.joints.size(), false);
            for (int idx : chainIndices) {
                resolved[idx] = true;
            }

            std::function<void(int)> propagate = [&](int jointIdx) {
                int parentIdx = skeleton.joints[jointIdx].parentIndex;
                if (parentIdx != -1 && resolved[parentIdx] && !resolved[jointIdx]) {
                    globalTransforms[jointIdx] = globalTransforms[parentIdx] * skeleton.joints[jointIdx].localTransform;
                    resolved[jointIdx] = true;
                }
                for (size_t i = 0; i < skeleton.joints.size(); ++i) {
                    if (skeleton.joints[i].parentIndex == jointIdx) {
                        propagate(static_cast<int>(i));
                    }
                }
            };

            for (int idx : chainIndices) {
                propagate(idx);
            }
        }

        /**
         * @brief Updates the animation timing and calculates local/global joint matrices for an entity.
         */
        void updateEntityAnimation(Entity entity, SkeletonComponent& skeleton, AnimatorComponent& animator, float dt) {
            if (skeleton.joints.empty()) return;

            // 1. Initialize temporary pose with bind-pose default TRS values
            std::vector<JointPose> finalPose(skeleton.joints.size());
            for (size_t i = 0; i < skeleton.joints.size(); ++i) {
                finalPose[i] = { skeleton.joints[i].bindTranslation, skeleton.joints[i].bindRotation, skeleton.joints[i].bindScale };
            }

            // 2. Sample poses according to State Machine or fallback to Single Clip Animator
            if (auto* controller = registry.get<AnimationControllerComponent>(entity)) {
                if (!controller->currentState.empty()) {
                    // Sample current state
                    const AnimationState* currState = nullptr;
                    for (const auto& state : controller->states) {
                        if (state.name == controller->currentState) {
                            currState = &state;
                            break;
                        }
                    }

                    if (currState) {
                        evaluateStatePose(*controller, *currState, controller->currentStateTime, animator, skeleton, finalPose);
                    }

                    // Handle crossfading transition
                    if (controller->isCrossfading && !controller->fromState.empty()) {
                        const AnimationState* fState = nullptr;
                        for (const auto& state : controller->states) {
                            if (state.name == controller->fromState) {
                                fState = &state;
                                break;
                            }
                        }

                        if (fState) {
                            std::vector<JointPose> fromPose(skeleton.joints.size());
                            for (size_t i = 0; i < skeleton.joints.size(); ++i) {
                                fromPose[i] = { skeleton.joints[i].bindTranslation, skeleton.joints[i].bindRotation, skeleton.joints[i].bindScale };
                            }
                            evaluateStatePose(*controller, *fState, controller->fromStateTime, animator, skeleton, fromPose);
                            
                            float weight = glm::clamp(controller->crossfadeProgress / controller->crossfadeDuration, 0.0f, 1.0f);
                            blendPoses(fromPose, finalPose, weight, finalPose);
                        }
                    }
                }
            } else {
                // Fallback to single clip playback
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

                    sampleClip(*clip, animator.currentTime, skeleton, finalPose);
                }
            }

            // 3. Write blended pose back to localTransforms
            for (size_t i = 0; i < skeleton.joints.size(); ++i) {
                glm::mat4 tMat = glm::translate(glm::mat4(1.0f), finalPose[i].translation);
                glm::mat4 rMat = glm::toMat4(finalPose[i].rotation);
                glm::mat4 sMat = glm::scale(glm::mat4(1.0f), finalPose[i].scale);
                skeleton.joints[i].localTransform = tMat * rMat * sMat;
            }

            // 4. Resolve Hierarchical Global Transforms (Forward Kinematics)
            std::vector<glm::mat4> globalTransforms(skeleton.joints.size(), glm::mat4(1.0f));
            std::vector<bool> resolved(skeleton.joints.size(), false);
            
            auto resolveJointGlobal = [&](auto& self, int jointIdx) -> glm::mat4 {
                if (jointIdx == -1) return glm::mat4(1.0f);
                if (resolved[jointIdx]) return globalTransforms[jointIdx];

                glm::mat4 parentGlobal = self(self, skeleton.joints[jointIdx].parentIndex);
                globalTransforms[jointIdx] = parentGlobal * skeleton.joints[jointIdx].localTransform;
                resolved[jointIdx] = true;
                return globalTransforms[jointIdx];
            };

            for (size_t i = 0; i < skeleton.joints.size(); ++i) {
                resolveJointGlobal(resolveJointGlobal, static_cast<int>(i));
            }

            // 5. Post-FK: Apply IK solver if present and enabled
            if (auto* ik = registry.get<IKSolverComponent>(entity)) {
                if (ik->enabled) {
                    if (ik->solverType == IKSolverType::TwoBone) {
                        solve2BoneIK(skeleton, *ik, globalTransforms);
                    } else if (ik->solverType == IKSolverType::FABRIK) {
                        solveFABRIK(skeleton, *ik, globalTransforms);
                    }
                }
            }

            // 6. Generate final joint offset matrix palette
            skeleton.jointMatrices.assign(256, glm::mat4(1.0f));
            for (size_t i = 0; i < std::min(skeleton.joints.size(), size_t(256)); ++i) {
                skeleton.jointMatrices[i] = globalTransforms[i] * skeleton.joints[i].inverseBindMatrix;
            }

            // 7. Upload Joint Matrices Palette to GPU Buffer
            VkDeviceSize bufferSize = 256 * sizeof(glm::mat4);

            if (!skeleton.gpuBuffer) {
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

        // --- Keyframe Interpolation Helpers ---

        glm::vec3 interpolateTranslation(const std::vector<Keyframe>& keys, float time, const glm::vec3& defaultValue) {
            if (keys.empty()) return defaultValue;
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

        glm::quat interpolateRotation(const std::vector<KeyframeRot>& keys, float time, const glm::quat& defaultValue) {
            if (keys.empty()) return defaultValue;
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

        glm::vec3 interpolateScale(const std::vector<Keyframe>& keys, float time, const glm::vec3& defaultValue) {
            if (keys.empty()) return defaultValue;
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

