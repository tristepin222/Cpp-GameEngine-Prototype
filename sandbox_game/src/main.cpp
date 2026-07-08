#include "SandboxApp.hpp"
#include <iostream>

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
