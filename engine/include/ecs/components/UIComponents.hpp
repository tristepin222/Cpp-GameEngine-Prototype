#pragma once
#include <string>
#include <glm/glm.hpp>

namespace Engine {

// @reflect
    struct CanvasComponent {
        // @reflect
        bool isScreenSpace = true;
    };

// @reflect
    struct RectTransform {
        // @reflect
        glm::vec2 anchorMin{0.5f, 0.5f};
        // @reflect
        glm::vec2 anchorMax{0.5f, 0.5f};
        // @reflect
        glm::vec2 anchoredPosition{0.0f, 0.0f};
        // @reflect
        glm::vec2 sizeDelta{100.0f, 100.0f};
        // @reflect
        glm::vec2 pivot{0.5f, 0.5f};
    };

// @reflect
    struct UIPanelComponent {
        // @reflect
        glm::vec4 color{0.15f, 0.15f, 0.15f, 0.8f};
        // @reflect
        float borderRadius = 4.0f;
    };

// @reflect
    struct UIImageComponent {
        // @reflect
        std::string texturePath;
        // @reflect
        glm::vec4 tintColor{1.0f, 1.0f, 1.0f, 1.0f};
    };

// @reflect
    struct UITextComponent {
        // @reflect
        std::string text = "New Text";
        // @reflect
        glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
        // @reflect
        float fontSize = 14.0f;
        // @reflect
        bool alignCenter = false;
    };

// @reflect
    struct UIButtonComponent {
        // @reflect
        std::string label = "Button";
        // @reflect
        glm::vec4 normalColor{0.15f, 0.40f, 0.70f, 1.0f};
        // @reflect
        glm::vec4 hoverColor{0.20f, 0.50f, 0.80f, 1.0f};
        // @reflect
        glm::vec4 pressedColor{0.10f, 0.30f, 0.60f, 1.0f};
        // @reflect
        glm::vec4 textColor{1.0f, 1.0f, 1.0f, 1.0f};
        bool isClicked = false;
        // @reflect
        std::string clickEventName;
    };

    enum class GridConstraint {
        Flexible = 0,
        FixedColumnCount = 1,
        FixedRowCount = 2
    };

// @reflect
    struct UIGridLayoutGroupComponent {
        // @reflect
        glm::vec2 cellSize{100.0f, 100.0f};
        // @reflect
        glm::vec2 spacing{10.0f, 10.0f};
        // @reflect
        glm::vec4 padding{5.0f, 5.0f, 5.0f, 5.0f}; // Top, Right, Bottom, Left
        GridConstraint constraint = GridConstraint::FixedColumnCount;
        int constraintCount = 3; // Max columns or max rows depending on constraint
    };

// @reflect
    struct UILayoutGroupComponent {
        // @reflect
        bool isVertical = true;
        // @reflect
        float spacing = 8.0f;
        // @reflect
        glm::vec4 padding{4.0f, 4.0f, 4.0f, 4.0f}; // Top, Right, Bottom, Left
        bool childForceExpand = false;
    };

// @reflect
    struct UIScrollRectComponent {
        // @reflect
        bool horizontal = false;
        // @reflect
        bool vertical = true;
        // @reflect
        glm::vec2 scrollPosition{0.0f, 0.0f};
        // @reflect
        float scrollSpeed = 25.0f;
    };

// @reflect
    struct UISliderComponent {
        // @reflect
        float value = 0.5f;
        // @reflect
        float minValue = 0.0f;
        // @reflect
        float maxValue = 1.0f;
        // @reflect
        glm::vec4 backgroundColor{0.2f, 0.2f, 0.25f, 1.0f};
        // @reflect
        glm::vec4 fillColor{0.2f, 0.6f, 0.9f, 1.0f};
        // @reflect
        glm::vec4 handleColor{0.95f, 0.95f, 0.98f, 1.0f};
    };

// @reflect
    struct UIToggleComponent {
        // @reflect
        bool isOn = true;
        // @reflect
        std::string label = "Toggle Option";
        // @reflect
        glm::vec4 boxColor{0.2f, 0.2f, 0.25f, 1.0f};
        // @reflect
        glm::vec4 checkmarkColor{0.2f, 0.8f, 0.4f, 1.0f};
        // @reflect
        glm::vec4 textColor{1.0f, 1.0f, 1.0f, 1.0f};
    };


} // namespace Engine

#include "meta/ComponentReflection.hpp"
REGISTER_COMPONENT(Engine::CanvasComponent, "UI");
REGISTER_COMPONENT(Engine::RectTransform, "UI");
REGISTER_COMPONENT(Engine::UIPanelComponent, "UI");
REGISTER_COMPONENT(Engine::UIImageComponent, "UI");
REGISTER_COMPONENT(Engine::UITextComponent, "UI");
REGISTER_COMPONENT(Engine::UIButtonComponent, "UI");
REGISTER_COMPONENT(Engine::UIGridLayoutGroupComponent, "UI");
REGISTER_COMPONENT(Engine::UILayoutGroupComponent, "UI");
REGISTER_COMPONENT(Engine::UIScrollRectComponent, "UI");
REGISTER_COMPONENT(Engine::UISliderComponent, "UI");
REGISTER_COMPONENT(Engine::UIToggleComponent, "UI");


