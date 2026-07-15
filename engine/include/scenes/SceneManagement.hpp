#pragma once
#include <string>
#include <memory>
#include <atomic>
#include <functional>
#include <vector>
#include "core/EngineAPI.hpp"

class SceneManager;

/**
 * @enum LoadSceneMode
 * @brief Determines whether loading a scene replaces or adds to the current set.
 */
enum class LoadSceneMode {
    Single,   ///< Unload all current scenes, then load the new one.
    Additive  ///< Load the new scene alongside existing ones.
};

/**
 * @struct SceneInfo
 * @brief Lightweight descriptor for a known or loaded scene.
 */
struct ENGINE_API SceneInfo {
    int id = -1;                ///< Unique integer ID assigned at load time.
    std::string name;           ///< Scene base name (e.g. "level1").
    std::string path;           ///< Full scene file path (e.g. "assets/scenes/level1.json").
    bool isLoaded = false;      ///< True if the scene is currently active in the world.
};

/**
 * @struct AsyncOperation
 * @brief Handle returned from loadSceneAsync(). Tracks async load progress.
 */
struct ENGINE_API AsyncOperation {
    std::atomic<float> progress{0.0f};  ///< 0.0 (start) to 1.0 (complete).
    std::atomic<bool>  isDone{false};   ///< True when the scene is fully loaded.
    SceneInfo sceneInfo;                ///< Info about the scene being loaded.

    /** @brief Optional per-operation callback fired when load completes. */
    std::function<void(const SceneInfo&)> onComplete;
};

/**
 * @class SceneManagement
 * @brief Static Unity-style API for scene loading, unloading, and event subscriptions.
 *
 * Usage:
 * @code
 *   // Load by name, path, or id
 *   SceneManagement::loadScene("level1");
 *   SceneManagement::loadScene("assets/scenes/level1.json");
 *   SceneManagement::loadScene(2);
 *
 *   // Additive loading
 *   SceneManagement::loadScene("hud", LoadSceneMode::Additive);
 *
 *   // Async loading with progress tracking
 *   auto op = SceneManagement::loadSceneAsync("level2");
 *   if (op->isDone) { ... }
 *   op->onComplete = [](const SceneInfo& info) { ... };
 *
 *   // Unload a specific scene
 *   SceneManagement::unloadScene("level1");
 *   SceneManagement::unloadScene(2);
 *
 *   // Subscribe to global events
 *   SceneManagement::onSceneLoaded([](const SceneInfo& info) { ... });
 *   SceneManagement::onSceneUnloaded([](const SceneInfo& info) { ... });
 *
 *   // Query
 *   auto scenes = SceneManagement::getLoadedScenes();
 *   SceneInfo info = SceneManagement::getSceneInfo("level1");
 * @endcode
 */
class ENGINE_API SceneManagement {
public:

    // -------------------------------------------------------------------------
    // Load — deferred to start of next frame, safe to call from update()
    // -------------------------------------------------------------------------

    /**
     * @brief Load a scene by name or path.
     * @param nameOrPath Scene name (e.g. "level1") or path (e.g. "assets/scenes/level1.json").
     * @param mode Single replaces all scenes; Additive adds alongside existing ones.
     * @param discardPendingAsync If true, cancels any in-flight async loads before proceeding.
     */
    static void loadScene(const std::string& nameOrPath,
                          LoadSceneMode mode = LoadSceneMode::Single,
                          bool discardPendingAsync = true);

    /**
     * @brief Load a scene by its integer ID (from a previously loaded scene).
     * @param sceneId The numeric ID of the scene to load/reload.
     * @param mode Single replaces all scenes; Additive adds alongside existing ones.
     * @param discardPendingAsync If true, cancels any in-flight async loads before proceeding.
     */
    static void loadScene(int sceneId,
                          LoadSceneMode mode = LoadSceneMode::Single,
                          bool discardPendingAsync = true);

    // -------------------------------------------------------------------------
    // Async Load
    // -------------------------------------------------------------------------

    /**
     * @brief Load a scene asynchronously by name or path.
     * @param nameOrPath Scene name or path.
     * @param mode Single or Additive.
     * @param discardPendingAsync If true, cancels any in-flight async loads before proceeding.
     * @return Shared handle to track load progress.
     */
    static std::shared_ptr<AsyncOperation> loadSceneAsync(const std::string& nameOrPath,
                                                           LoadSceneMode mode = LoadSceneMode::Single,
                                                           bool discardPendingAsync = true);

    /**
     * @brief Load a scene asynchronously by its integer ID.
     * @param sceneId The numeric ID of the scene to reload.
     * @param mode Single or Additive.
     * @param discardPendingAsync If true, cancels any in-flight async loads before proceeding.
     * @return Shared handle to track load progress.
     */
    static std::shared_ptr<AsyncOperation> loadSceneAsync(int sceneId,
                                                           LoadSceneMode mode = LoadSceneMode::Single,
                                                           bool discardPendingAsync = true);

    // -------------------------------------------------------------------------
    // Unload
    // -------------------------------------------------------------------------

    /**
     * @brief Unload a scene by its name or path.
     * @param nameOrPath Scene name (e.g. "level1") or full path.
     */
    static void unloadScene(const std::string& nameOrPath);

    /**
     * @brief Unload a scene by its integer ID.
     * @param sceneId Numeric scene ID.
     */
    static void unloadScene(int sceneId);

    // -------------------------------------------------------------------------
    // Events
    // -------------------------------------------------------------------------

    /**
     * @brief Subscribe a callback to be fired whenever any scene finishes loading.
     * @param callback Receives the SceneInfo of the loaded scene.
     */
    static void onSceneLoaded(std::function<void(const SceneInfo&)> callback);

    /**
     * @brief Subscribe a callback to be fired whenever any scene is unloaded.
     * @param callback Receives the SceneInfo of the unloaded scene.
     */
    static void onSceneUnloaded(std::function<void(const SceneInfo&)> callback);

    // -------------------------------------------------------------------------
    // Query
    // -------------------------------------------------------------------------

    /**
     * @brief Returns all currently loaded scenes.
     */
    static std::vector<SceneInfo> getLoadedScenes();

    /**
     * @brief Get scene info by integer ID.
     */
    static SceneInfo getSceneInfo(int sceneId);

    /**
     * @brief Get scene info by name or path.
     */
    static SceneInfo getSceneInfo(const std::string& nameOrPath);

    // -------------------------------------------------------------------------
    // Internal engine API
    // -------------------------------------------------------------------------

    /** @brief Called once at engine boot to wire the SceneManager. */
    static void setSceneManager(SceneManager* manager);

private:
    static SceneManager* s_sceneManager;
};
