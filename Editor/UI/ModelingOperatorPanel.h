#pragma once
#include <imgui.h>

namespace Editor::UI {

class ModelingOperatorPanel {
public:
    static void render(const ImVec2& viewportPos, const ImVec2& viewportSize);
};

} // namespace Editor::UI
