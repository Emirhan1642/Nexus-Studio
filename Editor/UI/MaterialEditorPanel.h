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
    ImNodesContext* m_editorContext;

    void drawNode(Engine::Renderer::ShaderNode& node);
    void handleContextMenu();
    
    // Will be moved to ShaderGraphCompiler soon
    void compileGraph();
};

} // namespace Editor::UI
