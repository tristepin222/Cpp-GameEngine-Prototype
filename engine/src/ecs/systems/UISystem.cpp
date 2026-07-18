#include "ecs/systems/UISystem.hpp"
#include "ecs/components/Hierarchy.hpp"
#include "renderer/ResourceManager.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace Engine {

    void UISystem::draw() {
        ImGuiIO& io = ImGui::GetIO();
        glm::vec2 viewportPos(0.0f);
        glm::vec2 viewportSize(io.DisplaySize.x, io.DisplaySize.y);

        if (editorActive) {
            float leftWidth = glm::clamp(io.DisplaySize.x * 0.20f, 260.0f, 400.0f);
            float rightWidth = glm::clamp(io.DisplaySize.x * 0.22f, 320.0f, 460.0f);
            float topY = 22.0f;
            viewportPos = glm::vec2(leftWidth, topY);
            viewportSize = glm::vec2(io.DisplaySize.x - leftWidth - rightWidth, io.DisplaySize.y - topY);
        }

        if (viewportSize.x <= 0.f || viewportSize.y <= 0.f) return;

        // Configure transparent canvas overlay window
        ImGui::SetNextWindowPos(ImVec2(viewportPos.x, viewportPos.y));
        ImGui::SetNextWindowSize(ImVec2(viewportSize.x, viewportSize.y));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_NoBackground |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoSavedSettings;

        if (!isPlaying) {
            flags |= ImGuiWindowFlags_NoInputs;
        }

        ImGui::Begin("##GameUI_CanvasWindow", nullptr, flags);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // 1. Gather all Canvas root elements
        for (auto [canvasEnt, canvas] : registry.view<CanvasComponent>()) {
            // Find child elements under this Canvas hierarchy
            for (auto [ent, rect] : registry.view<RectTransform>()) {
                bool isRoot = true;
                if (auto* hierarchy = registry.get<HierarchyComponent>(ent)) {
                    if (hierarchy->parent.getId() != Entity::INVALID_ENTITY && registry.isValid(hierarchy->parent)) {
                        // If parent has a RectTransform component, then it's a child widget, not a root widget
                        if (registry.has<RectTransform>(hierarchy->parent)) {
                            isRoot = false;
                        }
                    }
                }

                if (isRoot) {
                    drawWidget(ent, viewportPos, viewportSize, drawList);
                }
            }
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void UISystem::drawWidget(Entity entity, const glm::vec2& viewportPos, const glm::vec2& viewportSize, ImDrawList* drawList) {
        if (!registry.isValid(entity)) return;

        // Calculate widget bounds relative to viewport
        glm::vec4 rect = getRect(entity, viewportSize);
        float absX = viewportPos.x + rect.x;
        float absY = viewportPos.y + rect.y;
        float w = rect.z;
        float h = rect.w;

        // Panel Component
        if (auto* panel = registry.get<UIPanelComponent>(entity)) {
            drawList->AddRectFilled(
                ImVec2(absX, absY),
                ImVec2(absX + w, absY + h),
                ImColor(panel->color.r, panel->color.g, panel->color.b, panel->color.a),
                panel->borderRadius
            );
        }

        // Image Component
        if (auto* img = registry.get<UIImageComponent>(entity)) {
            if (!img->texturePath.empty()) {
                Texture* tex = renderer.resourceManager->loadTexture(img->texturePath, renderer);
                if (tex && tex->descriptorSet != VK_NULL_HANDLE) {
                    drawList->AddImage(
                        (ImTextureID)tex->descriptorSet,
                        ImVec2(absX, absY),
                        ImVec2(absX + w, absY + h),
                        ImVec2(0.0f, 0.0f),
                        ImVec2(1.0f, 1.0f),
                        ImColor(img->tintColor.r, img->tintColor.g, img->tintColor.b, img->tintColor.a)
                    );
                } else {
                    // Fallback grey box
                    drawList->AddRectFilled(
                        ImVec2(absX, absY),
                        ImVec2(absX + w, absY + h),
                        ImColor(1.0f, 1.0f, 1.0f, 0.15f)
                    );
                }
            } else {
                // Color fill overlay
                drawList->AddRectFilled(
                    ImVec2(absX, absY),
                    ImVec2(absX + w, absY + h),
                    ImColor(img->tintColor.r, img->tintColor.g, img->tintColor.b, img->tintColor.a)
                );
            }
        }

        // Text Component
        if (auto* txt = registry.get<UITextComponent>(entity)) {
            if (!txt->text.empty()) {
                float fontScale = txt->fontSize / 14.0f;
                ImGui::SetWindowFontScale(fontScale);

                ImVec2 textSize = ImGui::CalcTextSize(txt->text.c_str());
                float textX = absX;
                float textY = absY;

                if (txt->alignCenter) {
                    textX = absX + (w - textSize.x) * 0.5f;
                    textY = absY + (h - textSize.y) * 0.5f;
                }

                ImGui::SetCursorScreenPos(ImVec2(textX, textY));
                ImGui::TextColored(
                    ImVec4(txt->color.r, txt->color.g, txt->color.b, txt->color.a),
                    "%s", txt->text.c_str()
                );
                ImGui::SetWindowFontScale(1.0f);
            }
        }

        // Button Component
        if (auto* btn = registry.get<UIButtonComponent>(entity)) {
            ImGui::SetCursorScreenPos(ImVec2(absX, absY));

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(btn->normalColor.r, btn->normalColor.g, btn->normalColor.b, btn->normalColor.a));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(btn->hoverColor.r, btn->hoverColor.g, btn->hoverColor.b, btn->hoverColor.a));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(btn->pressedColor.r, btn->pressedColor.g, btn->pressedColor.b, btn->pressedColor.a));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(btn->textColor.r, btn->textColor.g, btn->textColor.b, btn->textColor.a));

            std::string btnId = btn->label + "##uibtn_" + std::to_string(entity.getId());
            if (ImGui::Button(btnId.c_str(), ImVec2(w, h))) {
                if (isPlaying) {
                    btn->isClicked = true;
                }
            }
            ImGui::PopStyleColor(4);
        }

        // Draw children hierarchically
        std::vector<Entity> children;
        for (auto [child, hierarchy] : registry.view<HierarchyComponent>()) {
            if (hierarchy.parent == entity && registry.has<RectTransform>(child)) {
                children.push_back(child);
            }
        }

        // Render children
        for (Entity child : children) {
            drawWidget(child, viewportPos, viewportSize, drawList);
        }
    }

    glm::vec4 UISystem::getRect(Entity entity, const glm::vec2& viewportSize) {
        auto* rectTransform = registry.get<RectTransform>(entity);
        if (!rectTransform) return glm::vec4(0.0f);

        // Parent container dimensions
        glm::vec4 parentRect(0.0f, 0.0f, viewportSize.x, viewportSize.y);
        if (auto* hierarchy = registry.get<HierarchyComponent>(entity)) {
            if (hierarchy->parent.getId() != Entity::INVALID_ENTITY && registry.isValid(hierarchy->parent)) {
                if (registry.has<RectTransform>(hierarchy->parent)) {
                    parentRect = getRect(hierarchy->parent, viewportSize);
                }
            }
        }

        float px = parentRect.x;
        float py = parentRect.y;
        float pw = parentRect.z;
        float ph = parentRect.w;

        float aminX = rectTransform->anchorMin.x;
        float aminY = rectTransform->anchorMin.y;
        float amaxX = rectTransform->anchorMax.x;
        float amaxY = rectTransform->anchorMax.y;

        float anchorMinX_parent = px + aminX * pw;
        float anchorMinY_parent = py + aminY * ph;
        float anchorMaxX_parent = px + amaxX * pw;
        float anchorMaxY_parent = py + amaxY * ph;

        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

        // Horizontal sizing
        if (std::abs(aminX - amaxX) < 0.0001f) {
            w = rectTransform->sizeDelta.x;
            float anchorX = anchorMinX_parent;
            x = anchorX + rectTransform->anchoredPosition.x - rectTransform->pivot.x * w;
        } else {
            float leftMargin = rectTransform->anchoredPosition.x;
            float rightMargin = rectTransform->sizeDelta.x;
            float x0 = anchorMinX_parent + leftMargin;
            float x1 = anchorMaxX_parent - rightMargin;
            w = std::max(0.0f, x1 - x0);
            x = x0;
        }

        // Vertical sizing
        if (std::abs(aminY - amaxY) < 0.0001f) {
            h = rectTransform->sizeDelta.y;
            float anchorY = anchorMinY_parent;
            y = anchorY + rectTransform->anchoredPosition.y - rectTransform->pivot.y * h;
        } else {
            float topMargin = rectTransform->anchoredPosition.y;
            float bottomMargin = rectTransform->sizeDelta.y;
            float y0 = anchorMinY_parent + topMargin;
            float y1 = anchorMaxY_parent - bottomMargin;
            h = std::max(0.0f, y1 - y0);
            y = y0;
        }

        return glm::vec4(x, y, w, h);
    }

} // namespace Engine
