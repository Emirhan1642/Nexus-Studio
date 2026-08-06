#pragma once
#include "../../Engine/Renderer/Materials/ShaderGraph.h"
#include <imnodes.h>
#include <unordered_map>

namespace Editor::UI {

struct PinInfo {
    ImVec2 pos;
    ImU32 color;
    bool isInput;
};

class MaterialEditorPanel {
public:
    MaterialEditorPanel();
    ~MaterialEditorPanel();

    void draw(bool* p_open = nullptr);

private:
    Engine::Renderer::ShaderGraph m_graph;
    ImNodesContext* m_editorContext = nullptr;
    std::unordered_map<int, PinInfo> m_pinInfo;

    void drawNode(Engine::Renderer::ShaderNode& node);
    void handleContextMenu();
    void addColorNode();
    void addScalarNode(const char* label, float value);
    void compileGraph();
};

} // namespace Editor::UI
