#pragma once
#include <glm/glm.hpp>

/**
 * @struct InputComponent
 * @brief Component representing input state for an entity (e.g., player or camera).
 */
struct InputComponent {
    /** @brief Movement vector (forward/back, left/right, up/down). */
    glm::vec3 movement{ 0 }; // forward/back, left/right, up/down
    /** @brief Look delta vector (mouse movement yaw/pitch). */
    glm::vec2 look{ 0 };     // mouse delta
};
