#include <iostream>
#include "core/Buffer.hpp"
#include  "core/App.hpp"


int main() {
    try {
        App app;
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
