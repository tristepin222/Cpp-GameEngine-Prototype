#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

/**
 * @struct Keyframe
 * @brief Keyframe storing a 3D vector (translation or scale) at a specific timestamp.
 */
struct Keyframe {
    /** @brief Time offset in seconds. */
    float time = 0.0f;
    /** @brief Vector value. */
    glm::vec3 value = glm::vec3(0.0f);
};

/**
 * @struct KeyframeRot
 * @brief Keyframe storing a quaternion rotation at a specific timestamp.
 */
struct KeyframeRot {
    /** @brief Time offset in seconds. */
    float time = 0.0f;
    /** @brief Quaternion rotation value. */
    glm::quat value = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
};

/**
 * @struct AnimationChannel
 * @brief Tracks keyframe sequences (translation, rotation, scale) affecting a specific joint node.
 */
struct AnimationChannel {
    /** @brief Target joint index in the SkeletonComponent. */
    int jointIndex = -1;
    /** @brief Target joint name identifier. */
    std::string jointName;
    /** @brief List of translation keyframes. */
    std::vector<Keyframe> translationKeys;
    /** @brief List of rotation keyframes. */
    std::vector<KeyframeRot> rotationKeys;
    /** @brief List of scale keyframes. */
    std::vector<Keyframe> scaleKeys;
};

/**
 * @struct AnimationClip
 * @brief Collection of channels representing a single skeletal animation state.
 */
struct AnimationClip {
    /** @brief Name of the clip (e.g. "Walk", "Run"). */
    std::string name;
    /** @brief Total duration of the animation in seconds. */
    float duration = 0.0f;
    /** @brief Bone channels mapping keyframe states. */
    std::vector<AnimationChannel> channels;
};

/**
 * @struct AnimatorComponent
 * @brief Animator controller managing active clips and tracking playback timing.
 */
struct AnimatorComponent {
    /** @brief List of all clips loaded from the model asset. */
    std::vector<AnimationClip> animations;
    /** @brief Index of the active clip in playback. */
    int activeAnimationIndex = -1;
    /** @brief Current playback time offset in seconds. */
    float currentTime = 0.0f;
    /** @brief Playback multiplier speed (e.g. 1.0f = default, 2.0f = fast). */
    float playbackSpeed = 1.0f;
    /** @brief Should the clip restart on completion. */
    bool loop = true;
};
