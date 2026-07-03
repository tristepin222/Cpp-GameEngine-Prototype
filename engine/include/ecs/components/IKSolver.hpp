#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

/**
 * @enum IKSolverType
 * @brief Selects the mathematics used to compute joint alignment.
 */
enum class IKSolverType {
    TwoBone, // Closed-form analytical solver for elbows/knees
    FABRIK   // Iterative solver for arbitrary joint chains (e.g. spine, tail)
};

/**
 * @struct IKSolverComponent
 * @brief Specifies target joint chains and goal targets for analytical 2-Bone or iterative FABRIK IK.
 */
struct IKSolverComponent {
    IKSolverType solverType = IKSolverType::TwoBone;
    
    // For 2-Bone Solver
    std::string startJointName;  // e.g. "thigh.L"
    std::string middleJointName; // e.g. "shin.L"
    std::string endJointName;    // e.g. "foot.L"
    
    // For FABRIK Solver
    std::vector<std::string> jointChainNames; // List of joint names from base to tip
    int maxIterations = 10;
    float tolerance = 0.001f;
    
    glm::vec3 targetPosition{ 0.0f };            // Target location in world space
    glm::vec3 polePosition{ 0.0f, 0.0f, 1.0f };   // bend helper vector
    
    float targetWeight = 1.0f;                    // Blend weight between raw FK (0.0) and IK (1.0)
    bool enabled = false;
};
