#pragma once
#include "../../Engine/Renderer/Materials/ShaderGraph.h"
#include <imnodes.h>

namespace Editor::UI {

class MaterialEditorPanel {
public:
    MaterialEditorPanel();
    ~MaterialEditorPanel();

    void draw(bool* p_open = nullptr);

private:
    Engine::Renderer::ShaderGraph m_graph;
    ImNodesContext* m_editorContext = nullptr;

    void drawNode(Engine::Renderer::ShaderNode& node);
    void handleContextMenu();
    void addColorNode();
    void addScalarNode(const char* label, float value);
    void compileGraph();
};

} // namespace Editor::UI
