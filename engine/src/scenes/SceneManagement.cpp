#include "scenes/SceneManagement.hpp"
#include "scenes/SceneManager.hpp"
#include <iostream>
#include <mutex>

// ---------------------------------------------------------------------------
// Static members
// ---------------------------------------------------------------------------

SceneManager* SceneManagement::s_sceneManager = nullptr;

// Internal event subscriber lists
static std::vector<std::function<void(const SceneInfo&)>> s_onLoadedCallbacks;
static std::vector<std::function<void(const SceneInfo&)>> s_onUnloadedCallbacks;
static std::mutex s_callbackMutex;

// ---------------------------------------------------------------------------
// Internal setup
// ---------------------------------------------------------------------------

void SceneManagement::setSceneManager(SceneManager* manager) {
    s_sceneManager = manager;

    if (!manager) return;

    // Wire SceneManager event hooks to our global subscriber lists
    manager->onSceneLoadedCallback = [](const SceneInfo& info) {
        std::lock_guard<std::mutex> lock(s_callbackMutex);
        for (auto& cb : s_onLoadedCallbacks) cb(info);
    };
    manager->onSceneUnloadedCallback = [](const SceneInfo& info) {
        std::lock_guard<std::mutex> lock(s_callbackMutex);
        for (auto& cb : s_onUnloadedCallbacks) cb(info);
    };
}

// ---------------------------------------------------------------------------
// loadScene — by name/path
// ---------------------------------------------------------------------------

void SceneManagement::loadScene(const std::string& nameOrPath,
                                 LoadSceneMode mode,
                                 bool discardPendingAsync) {
    if (!s_sceneManager) {
        std::cerr << "[SceneManagement] loadScene called before engine initialized." << std::endl;
        return;
    }
    s_sceneManager->loadScene(nameOrPath, mode, discardPendingAsync);
}

// ---------------------------------------------------------------------------
// loadScene — by id
// ---------------------------------------------------------------------------

void SceneManagement::loadScene(int sceneId,
                                 LoadSceneMode mode,
                                 bool discardPendingAsync) {
    if (!s_sceneManager) {
        std::cerr << "[SceneManagement] loadScene called before engine initialized." << std::endl;
        return;
    }
    s_sceneManager->loadScene(sceneId, mode, discardPendingAsync);
}

// ---------------------------------------------------------------------------
// loadSceneAsync — by name/path
// ---------------------------------------------------------------------------

std::shared_ptr<AsyncOperation> SceneManagement::loadSceneAsync(const std::string& nameOrPath,
                                                                  LoadSceneMode mode,
                                                                  bool discardPendingAsync) {
    if (!s_sceneManager) {
        std::cerr << "[SceneManagement] loadSceneAsync called before engine initialized." << std::endl;
        return nullptr;
    }
    return s_sceneManager->loadSceneAsync(nameOrPath, mode, discardPendingAsync);
}

// ---------------------------------------------------------------------------
// loadSceneAsync — by id
// ---------------------------------------------------------------------------

std::shared_ptr<AsyncOperation> SceneManagement::loadSceneAsync(int sceneId,
                                                                  LoadSceneMode mode,
                                                                  bool discardPendingAsync) {
    if (!s_sceneManager) {
        std::cerr << "[SceneManagement] loadSceneAsync called before engine initialized." << std::endl;
        return nullptr;
    }
    // Resolve id -> path, then dispatch
    SceneInfo info = s_sceneManager->getSceneInfo(sceneId);
    if (info.id == -1) {
        std::cerr << "[SceneManagement] loadSceneAsync: scene id " << sceneId << " not found." << std::endl;
        return nullptr;
    }
    return s_sceneManager->loadSceneAsync(info.path, mode, discardPendingAsync);
}

// ---------------------------------------------------------------------------
// unloadScene
// ---------------------------------------------------------------------------

void SceneManagement::unloadScene(const std::string& nameOrPath) {
    if (!s_sceneManager) return;
    s_sceneManager->unloadScene(nameOrPath);
}

void SceneManagement::unloadScene(int sceneId) {
    if (!s_sceneManager) return;
    s_sceneManager->unloadScene(sceneId);
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void SceneManagement::onSceneLoaded(std::function<void(const SceneInfo&)> callback) {
    std::lock_guard<std::mutex> lock(s_callbackMutex);
    s_onLoadedCallbacks.push_back(std::move(callback));
}

void SceneManagement::onSceneUnloaded(std::function<void(const SceneInfo&)> callback) {
    std::lock_guard<std::mutex> lock(s_callbackMutex);
    s_onUnloadedCallbacks.push_back(std::move(callback));
}

// ---------------------------------------------------------------------------
// Query
// ---------------------------------------------------------------------------

std::vector<SceneInfo> SceneManagement::getLoadedScenes() {
    if (!s_sceneManager) return {};
    return s_sceneManager->getLoadedSceneInfos();
}

SceneInfo SceneManagement::getSceneInfo(int sceneId) {
    if (!s_sceneManager) return {};
    return s_sceneManager->getSceneInfo(sceneId);
}

SceneInfo SceneManagement::getSceneInfo(const std::string& nameOrPath) {
    if (!s_sceneManager) return {};
    return s_sceneManager->getSceneInfo(nameOrPath);
}
