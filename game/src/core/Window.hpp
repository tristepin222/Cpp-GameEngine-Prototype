#pragma once
#include <glfw3.h>
#include <stdexcept>
#include <functional>

class Window {
public:
    using ResizeCallback = std::function<void(int, int)>;

    Window(int width, int height, const char* title)
        : width(width), height(height)
    {
        if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!window) throw std::runtime_error("Failed to create GLFW window");

        // Store pointer for callbacks (set externally by App)
        glfwSetWindowUserPointer(window, this);

        // Default resize callback
        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int newWidth, int newHeight) {
            auto self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(w));
            if (self && self->resizeCallback)
                self->resizeCallback(newWidth, newHeight);
            });
    }

    ~Window() {
        if (window) glfwDestroyWindow(window);
        glfwTerminate();
    }

    void pollEvents() { glfwPollEvents(); }
    bool shouldClose() const { return glfwWindowShouldClose(window); }
    GLFWwindow* getHandle() const { return window; }

    void setResizeCallback(ResizeCallback cb) { resizeCallback = std::move(cb); }
    void getFramebufferSize(int& w, int& h) const { glfwGetFramebufferSize(window, &w, &h); }

private:
    GLFWwindow* window = nullptr;
    int width, height;
    ResizeCallback resizeCallback;
};
