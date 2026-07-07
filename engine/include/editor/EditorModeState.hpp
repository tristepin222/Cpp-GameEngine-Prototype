#pragma once

/**
 * @struct EditorModeState
 * @brief Struct keeping track of active editor states (e.g. fly mode vs editor UI interaction).
 */
struct EditorModeState {
    /** @brief Whether camera fly/control mode is active. */
    bool flyMode = false;
    /** @brief Whether the scene is currently simulating in Play Mode. */
    bool isPlaying = false;
    /** @brief Deferred play trigger. */
    bool pendingPlay = false;
    /** @brief Deferred stop trigger. */
    bool pendingStop = false;
};
