#include "core/Application.hpp"
#include "GameMetadataComponent.hpp"
#include <iostream>

/**
 * @class SandboxApp
 * @brief Subclass of Engine::Application defining game-specific initialization.
 */
class SandboxApp : public Engine::Application {
public:
    SandboxApp() : Application() {}

    void onStart() override {
        // Register custom game-level component with serialization registry
        registerComponent<GameMetadataComponent>("GameMetadata");
    }
};

/**
 * @brief Entry point for sandbox game application.
 * @return EXIT_SUCCESS on successful exit.
 */
int main() {
    try {
        SandboxApp app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Uncaught engine exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
