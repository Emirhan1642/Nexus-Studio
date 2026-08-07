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

    void drawContents();

private:
    Engine::Renderer::ShaderGraph m_graph;
    ImNodesContext* m_editorContext = nullptr;
    std::unordered_map<int, PinInfo> m_pinInfo;
    float m_zoomScale = 1.0f;

    void drawNode(Engine::Renderer::ShaderNode& node);
    void handleContextMenu();
    void addColorNode();
    void addScalarNode(const char* label, float value);
    void compileGraph();
};

} // namespace Editor::UI
