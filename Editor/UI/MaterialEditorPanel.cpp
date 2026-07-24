#include "MaterialEditorPanel.h"
#include "../../Engine/Renderer/Materials/ShaderGraphCompiler.h"
#include "../../Engine/Renderer/Renderer.h"
#include <imgui.h>
#include <iostream>

namespace Editor::UI {

MaterialEditorPanel::MaterialEditorPanel() {
    m_editorContext = ImNodes::CreateContext();
    
    // Add a default output node
    Engine::Renderer::ShaderNode outputNode;
    outputNode.id = m_graph.generateId();
    outputNode.name = "Material Output";
    outputNode.type = Engine::Renderer::NodeType::Output;
    
    Engine::Renderer::ShaderPin albedoPin;
    albedoPin.id = m_graph.generateId();
    albedoPin.nodeId = outputNode.id;
    albedoPin.name = "Albedo";
    albedoPin.type = Engine::Renderer::PinType::Vec3;
    albedoPin.kind = Engine::Renderer::PinKind::Input;
    outputNode.inputs.push_back(albedoPin);
    
    m_graph.nodes.push_back(outputNode);
}

MaterialEditorPanel::~MaterialEditorPanel() {
    ImNodes::DestroyContext(m_editorContext);
}

void MaterialEditorPanel::draw(bool* p_open) {
    if (!ImGui::Begin("Material Editor", p_open)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Compile Shader")) {
        compileGraph();
    }

    ImGui::Separator();

    ImNodes::SetCurrentContext(m_editorContext);
    ImNodes::BeginNodeEditor();
    
    handleContextMenu();

    for (auto& node : m_graph.nodes) {
        drawNode(node);
    }

    for (auto& link : m_graph.links) {
        ImNodes::Link(link.id, link.startPinId, link.endPinId);
    }

    ImNodes::EndNodeEditor();
    
    // Handle link creation
    int startPinId, endPinId;
    if (ImNodes::IsLinkCreated(&startPinId, &endPinId)) {
        Engine::Renderer::ShaderLink link;
        link.id = m_graph.generateId();
        link.startPinId = startPinId;
        link.endPinId = endPinId;
        m_graph.links.push_back(link);
    }
    
    // Handle link deletion
    int destroyedLinkId;
    if (ImNodes::IsLinkDestroyed(&destroyedLinkId)) {
        m_graph.removeLink(destroyedLinkId);
    }

    ImGui::End();
}

void MaterialEditorPanel::drawNode(Engine::Renderer::ShaderNode& node) {
    ImNodes::BeginNode(node.id);
    ImNodes::BeginNodeTitleBar();
    ImGui::TextUnformatted(node.name.c_str());
    ImNodes::EndNodeTitleBar();

    for (auto& pin : node.inputs) {
        ImNodes::BeginInputAttribute(pin.id);
        ImGui::TextUnformatted(pin.name.c_str());
        ImNodes::EndInputAttribute();
    }
    
    for (auto& pin : node.outputs) {
        ImNodes::BeginOutputAttribute(pin.id);
        ImGui::Indent(40);
        ImGui::TextUnformatted(pin.name.c_str());
        ImGui::Unindent(40);
        ImNodes::EndOutputAttribute();
    }

    ImNodes::EndNode();
}

void MaterialEditorPanel::handleContextMenu() {
    if (ImGui::BeginPopupContextWindow("ContextMenu")) {
        if (ImGui::MenuItem("Add Color Node")) {
            Engine::Renderer::ShaderNode node;
            node.id = m_graph.generateId();
            node.name = "Color";
            node.type = Engine::Renderer::NodeType::Color;
            
            Engine::Renderer::ShaderPin outPin;
            outPin.id = m_graph.generateId();
            outPin.nodeId = node.id;
            outPin.name = "RGB";
            outPin.type = Engine::Renderer::PinType::Vec3;
            outPin.kind = Engine::Renderer::PinKind::Output;
            
            node.outputs.push_back(outPin);
            m_graph.nodes.push_back(node);
        }
        ImGui::EndPopup();
    }
}

void MaterialEditorPanel::compileGraph() {
    std::cout << "Compiling shader graph...\n";
    Engine::Renderer::ShaderGraphCompiler compiler;
    uint16_t newProgramHandle;
    if (compiler.compileGraph(&m_graph, newProgramHandle)) {
        std::cout << "Shader compiled successfully. Setting as active material...\n";
        // Let's set it as a global override material in RendererSystem just for the MVP test
        bgfx::ProgramHandle handle = {newProgramHandle};
        Engine::Renderer::RendererSystem::instance().setOverrideMaterial(handle);
    } else {
        std::cerr << "Shader compilation failed.\n";
    }
}

} // namespace Editor::UI
