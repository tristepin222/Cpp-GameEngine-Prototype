#include "scenes/SceneManager.hpp"
#include "scenes/SceneManagement.hpp"
#include "scenes/Scene.hpp"
#include "scenes/DefaultScene.hpp"
#include "scenes/SceneSerializer.hpp"
#include "ecs/Registry.hpp"
#include "renderer/VulkanRenderer.hpp"
#include "core/JobSystem.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string makeSceneName(const std::string& path) {
    std::filesystem::path p(path);
    return p.stem().string(); // "assets/scenes/level1.json" -> "level1"
}

/**
 * @brief Resolves a name-or-path string to a canonical scene file path.
 *        "level1"                     -> "assets/scenes/level1.json"
 *        "assets/scenes/level1.json"  -> "assets/scenes/level1.json"
 */
std::string SceneManager::resolvePath(const std::string& nameOrPath) const {
    // Already looks like a path (contains '/' or ends in .json)
    if (nameOrPath.find('/') != std::string::npos ||
        nameOrPath.find('\\') != std::string::npos ||
        nameOrPath.find(".json") != std::string::npos) {
        return nameOrPath;
    }
    return "assets/scenes/" + nameOrPath + ".json";
}

std::string SceneManager::pathToName(const std::string& path) const {
    return makeSceneName(path);
}

int SceneManager::findSceneIdByPath(const std::string& path) const {
    std::string name = makeSceneName(path);
    for (const auto& entry : loadedScenes) {
        if (entry.path == path || entry.name == name) {
            return entry.id;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Context
// ---------------------------------------------------------------------------

void SceneManager::setContext(Registry* reg, VulkanRenderer* rend) {
    registry = reg;
    renderer = rend;
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void SceneManager::fireSceneLoaded(const SceneInfo& info) {
    std::cout << "[SceneManager] Scene loaded: " << info.name << " (id=" << info.id << ")" << std::endl;
    if (onSceneLoadedCallback) onSceneLoadedCallback(info);
}

void SceneManager::fireSceneUnloaded(const SceneInfo& info) {
    std::cout << "[SceneManager] Scene unloaded: " << info.name << " (id=" << info.id << ")" << std::endl;
    if (onSceneUnloadedCallback) onSceneUnloadedCallback(info);
}

// ---------------------------------------------------------------------------
// Async cancellation
// ---------------------------------------------------------------------------

void SceneManager::cancelAllAsyncOps() {
    std::lock_guard<std::mutex> asyncLock(activeAsyncMutex);
    // Mark all in-flight ops as cancelled (isDone = true, progress = -1 as sentinel)
    for (auto& op : activeAsyncOps) {
        if (op && !op->isDone) {
            op->progress.store(-1.0f); // sentinel: cancelled
            op->isDone.store(true);
        }
    }
    activeAsyncOps.clear();

    // Clear the pending async merge queue
    std::lock_guard<std::mutex> mergeLock(asyncMergeMutex);
    pendingAsyncMerges.clear();
}

bool SceneManager::hasActiveAsyncLoads() const {
    std::lock_guard<std::mutex> lock(activeAsyncMutex);
    for (const auto& op : activeAsyncOps) {
        if (op && !op->isDone) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Unload internals
// ---------------------------------------------------------------------------

void SceneManager::unloadSceneByPath(const std::string& path) {
    std::string name = makeSceneName(path);
    auto it = std::find_if(loadedScenes.begin(), loadedScenes.end(),
        [&](const LoadedSceneEntry& e) {
            return e.path == path || e.name == name;
        });

    if (it == loadedScenes.end()) {
        std::cerr << "[SceneManager] unloadScene: scene not found: " << path << std::endl;
        return;
    }

    SceneInfo info{ it->id, it->name, it->path, true };
    if (it->scene) it->scene->unload();
    loadedScenes.erase(it);
    fireSceneUnloaded(info);
}

void SceneManager::unloadAllScenes() {
    // Fire events and unload in reverse order
    for (auto it = loadedScenes.rbegin(); it != loadedScenes.rend(); ++it) {
        SceneInfo info{ it->id, it->name, it->path, true };
        if (it->scene) it->scene->unload();
        fireSceneUnloaded(info);
    }
    loadedScenes.clear();
}

// ---------------------------------------------------------------------------
// Spawn helpers (main thread only)
// ---------------------------------------------------------------------------

SceneInfo SceneManager::spawnScene(const std::string& path, LoadSceneMode mode) {
    if (!registry || !renderer) {
        std::cerr << "[SceneManager] Cannot spawn scene — context not set." << std::endl;
        return {};
    }

    if (mode == LoadSceneMode::Single) {
        unloadAllScenes();
    }

    int id = nextSceneId++;
    std::string name = makeSceneName(path);

    auto scene = std::make_unique<Engine::DefaultScene>(*registry, *renderer, path);
    scene->load();

    SceneInfo info{ id, name, path, true };
    loadedScenes.push_back({ id, name, path, std::move(scene) });
    fireSceneLoaded(info);
    return info;
}

SceneInfo SceneManager::spawnSceneFromContent(const std::string& path,
                                               const std::string& jsonContent,
                                               LoadSceneMode mode) {
    if (!registry || !renderer) {
        std::cerr << "[SceneManager] Cannot spawn scene — context not set." << std::endl;
        return {};
    }

    if (mode == LoadSceneMode::Single) {
        unloadAllScenes();
    }

    int id = nextSceneId++;
    std::string name = makeSceneName(path);

    // Create a scene but don't call load() since we already have the JSON content
    auto scene = std::make_unique<Engine::DefaultScene>(*registry, *renderer, path);

    // Manually deserialize from the pre-loaded content
    SceneSerializer serializer(*registry, *renderer);
    std::vector<Entity> entities;
    if (serializer.deserializeFromString(jsonContent, entities)) {
        for (Entity e : entities) {
            scene->trackEntity(e);
        }
    }

    SceneInfo info{ id, name, path, true };
    loadedScenes.push_back({ id, name, path, std::move(scene) });
    fireSceneLoaded(info);
    return info;
}

// ---------------------------------------------------------------------------
// Public loadScene (deferred)
// ---------------------------------------------------------------------------

void SceneManager::loadScene(const std::string& path,
                              LoadSceneMode mode,
                              bool discardPendingAsync) {
    if (discardPendingAsync) {
        cancelAllAsyncOps();
    }
    pendingLoads.push_back({ resolvePath(path), mode, discardPendingAsync, nullptr });
}

void SceneManager::loadScene(int sceneId,
                              LoadSceneMode mode,
                              bool discardPendingAsync) {
    // Find the path for this scene id
    for (const auto& entry : loadedScenes) {
        if (entry.id == sceneId) {
            loadScene(entry.path, mode, discardPendingAsync);
            return;
        }
    }
    std::cerr << "[SceneManager] loadScene(id): scene id " << sceneId << " not found." << std::endl;
}

// ---------------------------------------------------------------------------
// Public loadSceneAsync
// ---------------------------------------------------------------------------

std::shared_ptr<AsyncOperation> SceneManager::loadSceneAsync(const std::string& path,
                                                              LoadSceneMode mode,
                                                              bool discardPendingAsync) {
    if (discardPendingAsync) {
        cancelAllAsyncOps();
    }

    std::string resolvedPath = resolvePath(path);
    auto op = std::make_shared<AsyncOperation>();
    op->sceneInfo.path = resolvedPath;
    op->sceneInfo.name = makeSceneName(resolvedPath);

    {
        std::lock_guard<std::mutex> lock(activeAsyncMutex);
        activeAsyncOps.push_back(op);
    }

    // Background: read file into string
    Engine::JobSystem::getInstance().pushJob([this, resolvedPath, mode, op]() {
        // Check if already cancelled
        if (op->isDone) return;

        op->progress.store(0.1f);

        std::ifstream in(resolvedPath);
        if (!in.is_open()) {
            std::cerr << "[SceneManager] loadSceneAsync: cannot open " << resolvedPath << std::endl;
            op->isDone.store(true);
            return;
        }

        op->progress.store(0.3f);

        std::ostringstream buf;
        buf << in.rdbuf();
        std::string content = buf.str();

        op->progress.store(0.5f);

        // Queue the merge back to main thread
        if (!op->isDone) { // check not cancelled
            std::lock_guard<std::mutex> lock(asyncMergeMutex);
            pendingAsyncMerges.push_back({ resolvedPath, std::move(content), mode, op });
        }
    });

    return op;
}

// ---------------------------------------------------------------------------
// Public unloadScene
// ---------------------------------------------------------------------------

void SceneManager::unloadScene(const std::string& nameOrPath) {
    pendingUnloadPaths.push_back(resolvePath(nameOrPath));
}

void SceneManager::unloadScene(int sceneId) {
    for (const auto& entry : loadedScenes) {
        if (entry.id == sceneId) {
            pendingUnloadPaths.push_back(entry.path);
            return;
        }
    }
    std::cerr << "[SceneManager] unloadScene(id): scene id " << sceneId << " not found." << std::endl;
}

// ---------------------------------------------------------------------------
// Legacy changeScene (backward compat — used by editor)
// ---------------------------------------------------------------------------

void SceneManager::changeScene(std::unique_ptr<Scene> nextScene) {
    unloadAllScenes();

    if (nextScene) {
        int id = nextSceneId++;
        nextScene->load();
        SceneInfo info{ id, "scene", "", true };
        loadedScenes.push_back({ id, "scene", "", std::move(nextScene) });
        fireSceneLoaded(info);
    }
}

// ---------------------------------------------------------------------------
// Flush helpers
// ---------------------------------------------------------------------------

void SceneManager::flushPendingUnloads() {
    for (const std::string& path : pendingUnloadPaths) {
        unloadSceneByPath(path);
    }
    pendingUnloadPaths.clear();
}

void SceneManager::flushPendingLoads() {
    for (const PendingLoad& pl : pendingLoads) {
        spawnScene(pl.path, pl.mode);
    }
    pendingLoads.clear();
}

void SceneManager::flushPendingAsyncMerges() {
    std::vector<PendingAsyncMerge> merges;
    {
        std::lock_guard<std::mutex> lock(asyncMergeMutex);
        merges.swap(pendingAsyncMerges);
    }

    for (auto& merge : merges) {
        auto& op = merge.asyncOp;
        // Check if cancelled (progress sentinel = -1)
        if (op && op->progress.load() < 0.0f) continue;

        op->progress.store(0.6f);
        SceneInfo info = spawnSceneFromContent(merge.path, merge.jsonContent, merge.mode);
        op->progress.store(1.0f);
        op->sceneInfo = info;
        op->isDone.store(true);

        if (op->onComplete) {
            op->onComplete(info);
        }

        // Remove from active ops
        std::lock_guard<std::mutex> lock(activeAsyncMutex);
        activeAsyncOps.erase(
            std::remove_if(activeAsyncOps.begin(), activeAsyncOps.end(),
                [&](const std::shared_ptr<AsyncOperation>& a) { return a == op; }),
            activeAsyncOps.end());
    }
}

// ---------------------------------------------------------------------------
// Update (main loop tick)
// ---------------------------------------------------------------------------

void SceneManager::update(float dt) {
    flushPendingUnloads();
    flushPendingLoads();
    flushPendingAsyncMerges();

    for (auto& entry : loadedScenes) {
        if (entry.scene) entry.scene->update(dt);
    }
}

// ---------------------------------------------------------------------------
// Query
// ---------------------------------------------------------------------------

Scene* SceneManager::getCurrentScene() const {
    if (loadedScenes.empty()) return nullptr;
    return loadedScenes.front().scene.get();
}

std::vector<SceneInfo> SceneManager::getLoadedSceneInfos() const {
    std::vector<SceneInfo> result;
    for (const auto& e : loadedScenes) {
        result.push_back({ e.id, e.name, e.path, true });
    }
    return result;
}

SceneInfo SceneManager::getSceneInfo(int id) const {
    for (const auto& e : loadedScenes) {
        if (e.id == id) return { e.id, e.name, e.path, true };
    }
    return {};
}

SceneInfo SceneManager::getSceneInfo(const std::string& nameOrPath) const {
    std::string resolved = resolvePath(nameOrPath);
    std::string name = makeSceneName(resolved);
    for (const auto& e : loadedScenes) {
        if (e.path == resolved || e.name == name) {
            return { e.id, e.name, e.path, true };
        }
    }
    return {};
}
