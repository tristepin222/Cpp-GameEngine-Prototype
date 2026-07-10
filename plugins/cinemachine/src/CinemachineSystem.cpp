#include "CinemachineSystem.hpp"
#include "ecs/components/Camera.hpp"
#include "ecs/components/EditorCamera.hpp"
#include "ecs/components/Transform.hpp"
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <algorithm>

#include "ecs/components/Name.hpp"

CinemachineSystem::CinemachineSystem(Registry& reg, EditorModeState& mode)
    : registry(reg), editorMode(mode) {}

void CinemachineSystem::update(float dt) {
    // Resolve targets by name on load or if target IDs are invalid/incorrect
    for (auto [entity, vcam] : registry.view<CinemachineVirtualCamera>()) {
        // 1. Resolve follow target
        bool needResolveFollow = false;
        if (vcam.followTarget.getId() == Entity::INVALID_ENTITY || !registry.isValid(vcam.followTarget)) {
            needResolveFollow = true;
        } else {
            auto* nameComp = registry.get<Name>(vcam.followTarget);
            if (!nameComp || nameComp->value != vcam.followTargetName) {
                needResolveFollow = true;
            }
        }
        if (needResolveFollow && !vcam.followTargetName.empty()) {
            for (auto [ent, nameComp] : registry.view<Name>()) {
                if (nameComp.value == vcam.followTargetName) {
                    vcam.followTarget = ent;
                    break;
                }
            }
        }

        // 2. Resolve LookAt target
        bool needResolveLookAt = false;
        if (vcam.lookAtTarget.getId() == Entity::INVALID_ENTITY || !registry.isValid(vcam.lookAtTarget)) {
            needResolveLookAt = true;
        } else {
            auto* nameComp = registry.get<Name>(vcam.lookAtTarget);
            if (!nameComp || nameComp->value != vcam.lookAtTargetName) {
                needResolveLookAt = true;
            }
        }
        if (needResolveLookAt && !vcam.lookAtTargetName.empty()) {
            for (auto [ent, nameComp] : registry.view<Name>()) {
                if (nameComp.value == vcam.lookAtTargetName) {
                    vcam.lookAtTarget = ent;
                    break;
                }
            }
        }
    }

    // Only update and control the camera during Play Mode
    if (!editorMode.isPlaying) {
        return;
    }

    // 1. Find the highest priority active virtual camera
    Entity activeVcamEntity = Entity();
    int highestPriority = -999999;
    CinemachineVirtualCamera* activeVcam = nullptr;
    Transform* activeVcamTransform = nullptr;

    for (auto [entity, vcam, transform] : registry.view<CinemachineVirtualCamera, Transform>()) {
        if (vcam.active && vcam.priority > highestPriority) {
            highestPriority = vcam.priority;
            activeVcamEntity = entity;
            activeVcam = &vcam;
            activeVcamTransform = &transform;
        }
    }

    if (!activeVcam) {
        return; // No virtual camera active
    }

    // 2. Resolve the virtual camera's tracking target position and rotation
    glm::vec3 targetPos = activeVcamTransform->position;
    glm::vec3 targetRot = activeVcamTransform->rotation;

    // Follow Target
    if (activeVcam->followTarget.getId() != Entity::INVALID_ENTITY && registry.isValid(activeVcam->followTarget)) {
        if (auto* targetTrans = registry.get<Transform>(activeVcam->followTarget)) {
            // Compute offset in target's orientation
            glm::quat targetRotQuat = glm::quat(glm::radians(targetTrans->rotation));
            glm::vec3 worldOffset = targetRotQuat * activeVcam->followOffset;
            targetPos = targetTrans->position + worldOffset;
        }
    }

    // Initialize currentPosition if not done yet
    if (!activeVcam->initialized) {
        activeVcam->currentPosition = targetPos;
        activeVcam->currentRotationEuler = targetRot;
        activeVcam->initialized = true;
    }

    // Apply Damping to position
    if (activeVcam->followDamping > 0.0f) {
        float blendFactor = glm::clamp(dt * (5.0f / activeVcam->followDamping), 0.0f, 1.0f);
        activeVcam->currentPosition = glm::mix(activeVcam->currentPosition, targetPos, blendFactor);
    } else {
        activeVcam->currentPosition = targetPos;
    }

    // LookAt Target (aiming)
    if (activeVcam->lookAtTarget.getId() != Entity::INVALID_ENTITY && registry.isValid(activeVcam->lookAtTarget)) {
        if (auto* lookAtTrans = registry.get<Transform>(activeVcam->lookAtTarget)) {
            glm::vec3 targetDir = lookAtTrans->position - activeVcam->currentPosition;
            float dirLen = glm::length(targetDir);
            if (dirLen > 1e-3f) {
                targetDir = glm::normalize(targetDir);
                
                // Derive pitch and yaw matching Camera view matrix decomposition
                float pitchRad = asin(targetDir.y);
                float yawRad = atan2(targetDir.z, targetDir.x);
                
                targetRot.x = glm::degrees(pitchRad);
                targetRot.y = glm::degrees(yawRad);
                targetRot.z = 0.0f; // Roll is usually 0
            }
        }
    }

    // Apply Damping to rotation
    if (activeVcam->lookAtDamping > 0.0f) {
        float blendFactor = glm::clamp(dt * (5.0f / activeVcam->lookAtDamping), 0.0f, 1.0f);
        
        // Handle angle wrap-around for smooth interpolation
        float diffX = targetRot.x - activeVcam->currentRotationEuler.x;
        float diffY = targetRot.y - activeVcam->currentRotationEuler.y;
        
        // Wrap differences to [-180, 180]
        diffX = fmod(diffX + 180.0f, 360.0f);
        if (diffX < 0.0f) diffX += 360.0f;
        diffX -= 180.0f;
        
        diffY = fmod(diffY + 180.0f, 360.0f);
        if (diffY < 0.0f) diffY += 360.0f;
        diffY -= 180.0f;

        activeVcam->currentRotationEuler.x += diffX * blendFactor;
        activeVcam->currentRotationEuler.y += diffY * blendFactor;
        activeVcam->currentRotationEuler.z = targetRot.z;
    } else {
        activeVcam->currentRotationEuler = targetRot;
    }

    // Write back resolved values to the virtual camera's transform component so it can be inspected
    activeVcamTransform->position = activeVcam->currentPosition;
    activeVcamTransform->rotation = activeVcam->currentRotationEuler;

    // 3. Handle Cinemachine Brain Blending transitions
    if (activeVcamEntity != lastActiveCameraEntity) {
        if (lastActiveCameraEntity.getId() != Entity::INVALID_ENTITY && registry.isValid(lastActiveCameraEntity)) {
            // Start a new blend
            isBlending = true;
            blendTimer = 0.0f;
            
            // Start blend from the last camera's final position and rotation
            if (auto* lastTrans = registry.get<Transform>(lastActiveCameraEntity)) {
                blendStartPos = lastTrans->position;
                blendStartRot = lastTrans->rotation;
            } else {
                blendStartPos = activeVcamTransform->position;
                blendStartRot = activeVcamTransform->rotation;
            }
        }
        lastActiveCameraEntity = activeVcamEntity;
    }

    glm::vec3 finalPos = activeVcamTransform->position;
    glm::vec3 finalRot = activeVcamTransform->rotation;

    if (isBlending) {
        blendTimer += dt;
        float t = glm::clamp(blendTimer / blendDuration, 0.0f, 1.0f);
        
        // Smoothstep (Ease-In-Out) blending
        float smoothT = t * t * (3.0f - 2.0f * t);

        finalPos = glm::mix(blendStartPos, activeVcamTransform->position, smoothT);
        
        // Interpolate angles smoothly
        float diffX = activeVcamTransform->rotation.x - blendStartRot.x;
        float diffY = activeVcamTransform->rotation.y - blendStartRot.y;
        diffX = fmod(diffX + 180.0f, 360.0f);
        if (diffX < 0) diffX += 360.0f;
        diffX -= 180.0f;

        diffY = fmod(diffY + 180.0f, 360.0f);
        if (diffY < 0) diffY += 360.0f;
        diffY -= 180.0f;

        finalRot.x = blendStartRot.x + diffX * smoothT;
        finalRot.y = blendStartRot.y + diffY * smoothT;
        finalRot.z = glm::mix(blendStartRot.z, activeVcamTransform->rotation.z, smoothT);

        if (t >= 1.0f) {
            isBlending = false;
        }
    }

    // 4. Update the main scene camera (first active non-editor camera in registry)
    for (auto [entity, cam, transform] : registry.view<Camera, Transform>()) {
        if (!registry.has<EditorCamera>(entity)) {
            transform.position = finalPos;
            transform.rotation = finalRot;
            cam.fov = activeVcam->fov;
            break; // Only update one main camera
        }
    }
}
