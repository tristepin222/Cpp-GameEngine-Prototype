#pragma once
#include "core/Application.hpp"

/**
 * @class SandboxApp
 * @brief Subclass of Engine::Application defining game-specific initialization.
 */
class SandboxApp : public Engine::Application {
public:
    SandboxApp();

    void onStart() override;
};
