#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>

class Scene;
class Registry;
class VulkanRenderer;
#include "scenes/SceneManagement.hpp"

/**
 * @struct LoadedSceneEntry
 * @brief Internal record linking a Scene instance with its metadata.
 */
struct LoadedSceneEntry {
    int id = -1;
    std::string name;
    std::string path;
    std::unique_ptr<Scene> scene;
};

/**
 * @struct PendingLoad
 * @brief A queued synchronous or async scene load request.
 */
struct PendingLoad {
    std::string path;
    LoadSceneMode mode;
    bool discardPendingAsync = true;
    std::shared_ptr<AsyncOperation> asyncOp; ///< Non-null for async loads.
};

/**
 * @struct PendingAsyncMerge
 * @brief A completed async I/O load ready to be merged onto the main thread.
 */
struct PendingAsyncMerge {
    std::string path;
    std::string jsonContent;  ///< Raw JSON read from disk on background thread.
    LoadSceneMode mode;
    std::shared_ptr<AsyncOperation> asyncOp;
};

/**
 * @class SceneManager
 * @brief Manages a set of simultaneously loaded Scenes.
 *        Supports Single and Additive loading, async loads via JobSystem,
 *        explicit per-scene unloading, and deferred scene transitions.
 */
class SceneManager {
public:

    // -------------------------------------------------------------------------
    // Synchronous (deferred to next frame)
    // -------------------------------------------------------------------------

    /**
     * @brief Queue a scene to load next frame by path.
     * @param path Scene file path.
     * @param mode Single or Additive.
     * @param discardPendingAsync Cancel in-flight async ops when true.
     */
    void loadScene(const std::string& path,
                   LoadSceneMode mode,
                   bool discardPendingAsync = true);

    /**
     * @brief Queue a scene to load next frame by its existing id.
     * @param sceneId Numeric scene ID (must exist in loadedScenes).
     * @param mode Single or Additive.
     * @param discardPendingAsync Cancel in-flight async ops when true.
     */
    void loadScene(int sceneId,
                   LoadSceneMode mode,
                   bool discardPendingAsync = true);

    // -------------------------------------------------------------------------
    // Async load
    // -------------------------------------------------------------------------

    /**
     * @brief Start an async scene load. File I/O runs on a JobSystem worker;
     *        entity spawning is merged on the main thread next frame.
     * @param path Scene file path.
     * @param mode Single or Additive.
     * @param discardPendingAsync Cancel in-flight async ops when true.
     * @return Shared AsyncOperation handle for tracking progress.
     */
    std::shared_ptr<AsyncOperation> loadSceneAsync(const std::string& path,
                                                    LoadSceneMode mode,
                                                    bool discardPendingAsync = true);

    // -------------------------------------------------------------------------
    // Unload
    // -------------------------------------------------------------------------

    /**
     * @brief Immediately queue an unload for the scene with the given path.
     */
    void unloadScene(const std::string& path);

    /**
     * @brief Immediately queue an unload for the scene with the given ID.
     */
    void unloadScene(int sceneId);

    // -------------------------------------------------------------------------
    // Legacy / backward-compatible API
    // -------------------------------------------------------------------------

    /**
     * @brief Direct scene swap (used by editor and legacy code). Unloads all
     *        current scenes and activates nextScene immediately.
     */
    void changeScene(std::unique_ptr<Scene> nextScene);

    // -------------------------------------------------------------------------
    // Update & Context
    // -------------------------------------------------------------------------

    /**
     * @brief Called each frame. Flushes all pending loads, unloads, and async merges.
     * @param dt Delta time in seconds.
     */
    void update(float dt);

    /**
     * @brief Provide registry and renderer pointers needed for scene construction.
     */
    void setContext(Registry* reg, VulkanRenderer* rend);

    // -------------------------------------------------------------------------
    // Events — set by SceneManagement
    // -------------------------------------------------------------------------

    std::function<void(const SceneInfo&)> onSceneLoadedCallback;
    std::function<void(const SceneInfo&)> onSceneUnloadedCallback;

    // -------------------------------------------------------------------------
    // Query
    // -------------------------------------------------------------------------

    /** @brief Returns the first loaded scene (oldest), or nullptr. Backward-compatible. */
    Scene* getCurrentScene() const;

    /** @brief Returns all currently loaded scene entries. */
    std::vector<SceneInfo> getLoadedSceneInfos() const;

    /** @brief Returns info for a scene by id, or invalid SceneInfo (id=-1) if not found. */
    SceneInfo getSceneInfo(int id) const;

    /** @brief Returns info for a scene by path/name, or invalid SceneInfo if not found. */
    SceneInfo getSceneInfo(const std::string& nameOrPath) const;

    /** @brief True if any async loads are currently in flight. */
    bool hasActiveAsyncLoads() const;

private:
    // --- Internals ---
    Registry*       registry = nullptr;
    VulkanRenderer* renderer = nullptr;

    std::atomic<int> nextSceneId{0};

    /** Currently active scenes. Accessed only on main thread. */
    std::vector<LoadedSceneEntry> loadedScenes;

    // Pending operations queued from loadScene() calls (main thread write, main thread flush)
    std::vector<PendingLoad>       pendingLoads;
    std::vector<std::string>       pendingUnloadPaths;

    // Async merge queue — written by background threads, read on main thread
    std::vector<PendingAsyncMerge> pendingAsyncMerges;
    mutable std::mutex             asyncMergeMutex;

    // Track in-flight async ops so we can cancel them
    std::vector<std::shared_ptr<AsyncOperation>> activeAsyncOps;
    mutable std::mutex activeAsyncMutex;

    // --- Helpers ---
    std::string resolvePath(const std::string& nameOrPath) const;
    std::string pathToName(const std::string& path) const;
    int         findSceneIdByPath(const std::string& path) const;

    void flushPendingUnloads();
    void flushPendingLoads();
    void flushPendingAsyncMerges();
    void unloadSceneByPath(const std::string& path);
    void unloadAllScenes();

    /** Spawns a DefaultScene from path and registers it with a new id. */
    SceneInfo spawnScene(const std::string& path, LoadSceneMode mode);

    /** Spawns a scene from pre-loaded JSON string content. */
    SceneInfo spawnSceneFromContent(const std::string& path,
                                    const std::string& jsonContent,
                                    LoadSceneMode mode);

    void fireSceneLoaded(const SceneInfo& info);
    void fireSceneUnloaded(const SceneInfo& info);
    void cancelAllAsyncOps();
};
