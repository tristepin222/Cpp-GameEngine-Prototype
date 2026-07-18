#pragma once
#include <string>
#include <glm/glm.hpp>

namespace Engine {

    struct CanvasComponent {
        bool isScreenSpace = true;
    };

    struct RectTransform {
        glm::vec2 anchorMin{0.5f, 0.5f};
        glm::vec2 anchorMax{0.5f, 0.5f};
        glm::vec2 anchoredPosition{0.0f, 0.0f};
        glm::vec2 sizeDelta{100.0f, 100.0f};
        glm::vec2 pivot{0.5f, 0.5f};
    };

    struct UIPanelComponent {
        glm::vec4 color{0.15f, 0.15f, 0.15f, 0.8f};
        float borderRadius = 4.0f;
    };

    struct UIImageComponent {
        std::string texturePath;
        glm::vec4 tintColor{1.0f, 1.0f, 1.0f, 1.0f};
    };

    struct UITextComponent {
        std::string text = "New Text";
        glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
        float fontSize = 14.0f;
        bool alignCenter = false;
    };

    struct UIButtonComponent {
        std::string label = "Button";
        glm::vec4 normalColor{0.15f, 0.40f, 0.70f, 1.0f};
        glm::vec4 hoverColor{0.20f, 0.50f, 0.80f, 1.0f};
        glm::vec4 pressedColor{0.10f, 0.30f, 0.60f, 1.0f};
        glm::vec4 textColor{1.0f, 1.0f, 1.0f, 1.0f};
        bool isClicked = false;
        std::string clickEventName;
    };

} // namespace Engine
