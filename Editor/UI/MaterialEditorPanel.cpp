#include "MaterialEditorPanel.h"
#include "../../Engine/Renderer/Materials/ShaderGraphCompiler.h"
#include "../../Engine/Renderer/Renderer.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <imnodes.h>
#include <iostream>
#include "IconRegistry.h"
#include "NexusTheme.h"
#include "SelectionManager.h"
#include "../../Engine/Core/DataModel/Part.h"

// ─── Renk kısayolları ───────────────────────────────────────────────────────
static ImU32 COL(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a) {
    return IM_COL32((hex>>16)&0xFF,(hex>>8)&0xFF,hex&0xFF,(uint8_t)(a*255));
}

// ─── ImNodes stil uygulaması ─────────────────────────────────────────────────
// HTML'deki renkleri ImNodes'a yansıt: cyan aktif, gradient master node
static void applyImNodesStyle() {
    ImNodesStyle& s = ImNodes::GetStyle();
    s.Colors[ImNodesCol_NodeBackground]         = COLA(0x0e0e0e, 1.0f);
    s.Colors[ImNodesCol_NodeBackgroundHovered]  = COLA(0x171717, 1.0f);
    s.Colors[ImNodesCol_NodeBackgroundSelected] = COLA(0x171717, 1.0f);
    s.Colors[ImNodesCol_NodeOutline]            = COLA(0x171717, 1.0f);
    s.Colors[ImNodesCol_TitleBar]               = COLA(0x171717, 1.0f);
    s.Colors[ImNodesCol_TitleBarHovered]        = COLA(0x242424, 1.0f);
    s.Colors[ImNodesCol_TitleBarSelected]       = COLA(0x242424, 1.0f);
    s.Colors[ImNodesCol_Link]                   = COLA(0x00d2ff, 1.0f);
    s.Colors[ImNodesCol_LinkHovered]            = COLA(0x00d2ff, 0.8f);
    s.Colors[ImNodesCol_LinkSelected]           = COLA(0x00d2ff, 1.0f);
    s.Colors[ImNodesCol_Pin]                    = COLA(0x00d2ff, 1.0f);
    s.Colors[ImNodesCol_PinHovered]             = COLA(0xffffff, 1.0f);
    s.Colors[ImNodesCol_BoxSelector]            = COLA(0x00d2ff, 0.15f);
    s.Colors[ImNodesCol_BoxSelectorOutline]     = COLA(0x00d2ff, 0.5f);
    s.Colors[ImNodesCol_GridBackground]         = COLA(0x080808, 1.0f);
    s.Colors[ImNodesCol_GridLine]               = COLA(0x171717, 1.0f);
    s.NodeCornerRounding = 10.0f;
    s.NodePadding = ImVec2(8.0f, 6.0f);
    s.NodeBorderThickness   = 2.0f;
    
    s.Flags &= ~ImNodesStyleFlags_GridLines;
}

namespace Editor::UI {

MaterialEditorPanel::MaterialEditorPanel() {
    m_editorContext = ImNodes::CreateContext();
    applyImNodesStyle();

    // ── Default output node (HTML: PBR Master Node) ─────────────────────────
    Engine::Renderer::ShaderNode outputNode;
    outputNode.id   = m_graph.generateId();
    outputNode.name = "PBR Master Node";
    outputNode.type = Engine::Renderer::NodeType::Output;

    struct PinDef { const char* name; Engine::Renderer::PinType type; };
    static const PinDef inputPins[] = {
        {"Base Color", Engine::Renderer::PinType::Vec3},
        {"Normal Map", Engine::Renderer::PinType::Vec3},
        {"Roughness",  Engine::Renderer::PinType::Float},
        {"Metallic",   Engine::Renderer::PinType::Float},
        {"Emissive Color", Engine::Renderer::PinType::Vec3},
    };
    for (auto& pd : inputPins) {
        Engine::Renderer::ShaderPin pin;
        pin.id     = m_graph.generateId();
        pin.nodeId = outputNode.id;
        pin.name   = pd.name;
        pin.type   = pd.type;
        pin.kind   = Engine::Renderer::PinKind::Input;
        outputNode.inputs.push_back(pin);
    }
    m_graph.nodes.push_back(outputNode);

    // ── TextureSample node ─────────────────────────────────────────────────
    Engine::Renderer::ShaderNode texNode;
    texNode.id   = m_graph.generateId();
    texNode.name = "TextureSample (2D)";
    texNode.type = Engine::Renderer::NodeType::Color;
    {
        Engine::Renderer::ShaderPin out;
        out.id     = m_graph.generateId();
        out.nodeId = texNode.id;
        out.name   = "RGB";
        out.type   = Engine::Renderer::PinType::Vec3;
        out.kind   = Engine::Renderer::PinKind::Output;
        texNode.outputs.push_back(out);
    }
    m_graph.nodes.push_back(texNode);

    // ── Scalar Roughness node ──────────────────────────────────────────────
    Engine::Renderer::ShaderNode roughNode;
    roughNode.id   = m_graph.generateId();
    roughNode.name = "Scalar (Roughness)";
    roughNode.type = Engine::Renderer::NodeType::Color;
    {
        Engine::Renderer::ShaderPin out;
        out.id     = m_graph.generateId();
        out.nodeId = roughNode.id;
        out.name   = "0.10";
        out.type   = Engine::Renderer::PinType::Float;
        out.kind   = Engine::Renderer::PinKind::Output;
        roughNode.outputs.push_back(out);
    }
    m_graph.nodes.push_back(roughNode);

    // ── Scalar Metalness node ──────────────────────────────────────────────
    Engine::Renderer::ShaderNode metalNode;
    metalNode.id   = m_graph.generateId();
    metalNode.name = "Scalar (Metalness)";
    metalNode.type = Engine::Renderer::NodeType::Color;
    {
        Engine::Renderer::ShaderPin out;
        out.id     = m_graph.generateId();
        out.nodeId = metalNode.id;
        out.name   = "0.85";
        out.type   = Engine::Renderer::PinType::Float;
        out.kind   = Engine::Renderer::PinKind::Output;
        metalNode.outputs.push_back(out);
    }
    m_graph.nodes.push_back(metalNode);

    // Default bağlantılar (HTML'deki renkli SVG path'lere karşılık)
    auto link = [&](int from, int to) {
        Engine::Renderer::ShaderLink l;
        l.id         = m_graph.generateId();
        l.startPinId = from;
        l.endPinId   = to;
        m_graph.links.push_back(l);
    };
    // texNode.RGB → outputNode.Base Color
    link(texNode.outputs[0].id,   outputNode.inputs[0].id);
    // roughNode → Roughness
    link(roughNode.outputs[0].id, outputNode.inputs[2].id);
    // metalNode → Metallic
    link(metalNode.outputs[0].id, outputNode.inputs[3].id);
}

MaterialEditorPanel::~MaterialEditorPanel() {
    ImNodes::DestroyContext(m_editorContext);
}

void MaterialEditorPanel::draw(bool* p_open) {
    auto& T = NexusTheme::instance();

    ImGuiWindowClass window_class;
    ImGui::SetNextWindowClass(&window_class);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (!ImGui::Begin("Material Editor", p_open)) {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    float       width = ImGui::GetWindowWidth();

    // ─────────────────────────────────────────────────────────────────────────
    // HEADER (h=24)
    // HTML: h-6 bg-studio-panel border-b flex items-center justify-between
    //       "🎨 Shader Graph: PBR_Gold.mat" + "Unsaved *" badge
    //       "+ Add Node" + "Compile Shader"
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.panel);
    ImGui::BeginChild("##MatHdr", ImVec2(width, 28), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 base = ImGui::GetCursorScreenPos();
        dl->AddLine({base.x,base.y+27},{base.x+width,base.y+27}, COL(T.border));

        // 🎨 Shader Graph: (accent)
        dl->AddText({base.x+8, base.y+6}, COL(T.accent), "\xf0\x9f\x8e\xa8 Shader Graph:");

        float labelW = ImGui::CalcTextSize("\xf0\x9f\x8e\xa8 Shader Graph:").x;
        // PBR_Gold.mat (white)
        dl->AddText({base.x+12+labelW, base.y+6}, COL(T.textPrimary), "PBR_Gold.mat");

        float nameW = ImGui::CalcTextSize("PBR_Gold.mat").x;
        // Unsaved badge (HTML: bg-amber-500/20, text-amber-300, border-amber-500/40)
        float badgeX = base.x + 16 + labelW + nameW;
        const char* unsaved = "Unsaved Changes *";
        float bw = ImGui::CalcTextSize(unsaved).x + 10;
        dl->AddRectFilled({badgeX,base.y+5},{badgeX+bw,base.y+21},COLA(0xF59E0B,0.20f),4.0f);
        dl->AddRect({badgeX,base.y+5},{badgeX+bw,base.y+21},COLA(0xF59E0B,0.40f),4.0f);
        dl->AddText({badgeX+5,base.y+6}, COLA(0xFCD34D,1.0f), unsaved);

        // Sağ taraf: "+ Add Node" + "Compile Shader"
        float compileW = 110;
        float addW     = 80;
        float rx       = width - compileW - addW - 16;

        // + Add Node (ghost buton)
        ImGui::SetCursorScreenPos({base.x+rx, base.y+4});
        ImGui::PushStyleColor(ImGuiCol_Button,        COL(T.bg));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL(T.panelHover));
        ImGui::PushStyleColor(ImGuiCol_Text,          COL(T.textMuted));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6,2));
        if (ImGui::Button("+ Add Node  v", ImVec2(addW, 20)))
            ImGui::OpenPopup("##addNode");
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);

        if (ImGui::BeginPopup("##addNode")) {
            if (ImGui::MenuItem("Color Node"))    addColorNode();
            if (ImGui::MenuItem("Scalar Node"))   addScalarNode("Scalar", 0.5f);
            if (ImGui::MenuItem("Texture Sample")){}
            ImGui::EndPopup();
        }

        // Compile Shader (accent)
        ImGui::SetCursorScreenPos({base.x+rx+addW+6, base.y+4});
        ImGui::PushStyleColor(ImGuiCol_Button,        COL(T.accent));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COLA(0x00b8e0,1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0,0,0,1));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8,2));
        // Glow
        ImVec2 btnP = ImGui::GetCursorScreenPos();
        dl->AddRectFilled({btnP.x-2,btnP.y-2},{btnP.x+compileW+2,btnP.y+24},
                          COLA(0x00d2ff,0.15f),6.0f);
        if (ImGui::Button("Compile Shader", ImVec2(compileW, 20)))
            compileGraph();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ─────────────────────────────────────────────────────────────────────────
    // NODE EDITOR CANVAS
    // HTML: flex-1 node-grid (radial-gradient dots #262626, 18px)
    // ─────────────────────────────────────────────────────────────────────────
    ImNodes::SetCurrentContext(m_editorContext);

    ImNodes::BeginNodeEditor();

    // ── Özel Dot Grid Çizimi ──────────────────────────────────────────────────
    ImDrawList* dl_grid = ImGui::GetWindowDrawList();
    ImVec2 panning = ImNodes::EditorContextGetPanning();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    float spacing = 24.0f; // GridSpacing from ImNodes
    ImU32 dotColor = COLA(0x242424, 1.0f); // Aynı koyulukta nokta rengi

    float offsetX = fmodf(panning.x, spacing);
    if (offsetX < 0) offsetX += spacing;
    float offsetY = fmodf(panning.y, spacing);
    if (offsetY < 0) offsetY += spacing;

    for (float x = offsetX; x < winSize.x; x += spacing) {
        for (float y = offsetY; y < winSize.y; y += spacing) {
            dl_grid->AddRectFilled(ImVec2(winPos.x + x, winPos.y + y), ImVec2(winPos.x + x + 2.0f, winPos.y + y + 2.0f), dotColor);
        }
    }
    // ─────────────────────────────────────────────────────────────────────────

    handleContextMenu();

    for (auto& node : m_graph.nodes)
        drawNode(node);

    ImNodes::PushColorStyle(ImNodesCol_Link, COLA(0x000000, 0.0f));
    ImNodes::PushColorStyle(ImNodesCol_LinkHovered, COLA(0x000000, 0.0f));
    ImNodes::PushColorStyle(ImNodesCol_LinkSelected, COLA(0x000000, 0.0f));

    for (auto& link : m_graph.links)
        ImNodes::Link(link.id, link.startPinId, link.endPinId);

    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();
    ImNodes::PopColorStyle();

    // ── Özel Gradient Curve Çizimi ───────────────────────────────────────────
    ImDrawList* dl_links = ImGui::GetWindowDrawList();
    for (auto& link : m_graph.links) {
        if (m_pinInfo.count(link.startPinId) && m_pinInfo.count(link.endPinId)) {
            PinInfo p0 = m_pinInfo[link.startPinId];
            PinInfo p1 = m_pinInfo[link.endPinId];

            if (p0.isInput && !p1.isInput) std::swap(p0, p1);

            ImVec2 startPos = p0.pos;
            ImVec2 endPos = p1.pos;
            ImU32 startCol = p0.color;
            ImU32 endCol = p1.color;

            float dist = std::abs(endPos.x - startPos.x) * 0.5f;
            if (dist < 50.0f) dist = 50.0f;
            ImVec2 cp0 = ImVec2(startPos.x + dist, startPos.y);
            ImVec2 cp1 = ImVec2(endPos.x - dist, endPos.y);

            int segments = 60;
            float thickness = 8.0f; // Pin çapı
            
            int r0 = (startCol >> 0) & 0xFF;
            int g0 = (startCol >> 8) & 0xFF;
            int b0 = (startCol >> 16) & 0xFF;
            int r1 = (endCol >> 0) & 0xFF;
            int g1 = (endCol >> 8) & 0xFF;
            int b1 = (endCol >> 16) & 0xFF;

            ImVec2 prev_p = startPos;
            
            // Başlangıç noktasına kapak
            dl_links->AddCircleFilled(startPos, thickness * 0.4f, startCol);

            for (int i = 1; i <= segments; ++i) {
                float t = (float)i / segments;
                float u = 1.0f - t;
                
                float w0 = u * u * u;
                float w1 = 3 * u * u * t;
                float w2 = 3 * u * t * t;
                float w3 = t * t * t;
                
                ImVec2 p(
                    w0 * startPos.x + w1 * cp0.x + w2 * cp1.x + w3 * endPos.x,
                    w0 * startPos.y + w1 * cp0.y + w2 * cp1.y + w3 * endPos.y
                );
                
                int r = r0 + (r1 - r0) * t;
                int g = g0 + (g1 - g0) * t;
                int b = b0 + (b1 - b0) * t;
                ImU32 col = IM_COL32(r, g, b, 255);
                
                dl_links->AddLine(prev_p, p, col, thickness);
                dl_links->AddCircleFilled(p, thickness * 0.4f, col); // Eklem yerindeki boşlukları kapat (taşmaması için 0.4)
                prev_p = p;
            }
        }
    }
    // ─────────────────────────────────────────────────────────────────────────

    ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomRight);
    ImNodes::EndNodeEditor();

    // ── Bağlantı oluşturma ────────────────────────────────────────────────────
    int startPinId, endPinId;
    if (ImNodes::IsLinkCreated(&startPinId, &endPinId)) {
        Engine::Renderer::ShaderLink l;
        l.id         = m_graph.generateId();
        l.startPinId = startPinId;
        l.endPinId   = endPinId;
        m_graph.links.push_back(l);
    }

    int destroyedId;
    if (ImNodes::IsLinkDestroyed(&destroyedId))
        m_graph.removeLink(destroyedId);

    ImGui::End();
    ImGui::PopStyleVar();
}

// ─── Node çizimi ─────────────────────────────────────────────────────────────
void MaterialEditorPanel::drawNode(Engine::Renderer::ShaderNode& node) {
    auto& T = NexusTheme::instance();

    ImNodes::BeginNode(node.id);

    // Node genişliğini 160px'e sabitle
    ImGui::Dummy(ImVec2(160, 0));

    // Başlık
    ImNodes::BeginNodeTitleBar();
    ImGui::TextUnformatted(node.name.c_str());
    
    // Sağ köşedeki indikatör karesini çiz
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 minPos = ImGui::GetItemRectMin();
    ImVec2 maxPos = ImGui::GetItemRectMax();
    
    ImU32 indicatorCol = COLA(0x88FF51, 1.0f);
    if (node.name == "PBR Master Node") indicatorCol = COLA(0x5174FF, 1.0f);
    else if (node.name == "Scalar (Roughness)") indicatorCol = COLA(0x0033FF, 1.0f);
    else if (node.name == "Scalar (Metalness)") indicatorCol = COLA(0xD351FF, 1.0f);
    
    float sqSize = 9.0f;
    float rightX = minPos.x - 8.0f + 160.0f;
    float topY = minPos.y + (maxPos.y - minPos.y) * 0.5f - sqSize * 0.5f;
    dl->AddRectFilled(ImVec2(rightX - 10.0f - sqSize, topY), ImVec2(rightX - 10.0f, topY + sqSize), indicatorCol, 3.0f);
    
    ImNodes::EndNodeTitleBar();

    ImVec2 nodePos = ImNodes::GetNodeScreenSpacePos(node.id);
    ImVec2 nodeDim = ImNodes::GetNodeDimensions(node.id);

    // Input pinleri
    for (auto& pin : node.inputs) {
        ImU32 pinCol = COLA(0x82D9FF, 1.0f); // default input
        if (pin.name == "Normal Map")   pinCol = COLA(0xD351FF, 1.0f);
        
        ImNodes::PushColorStyle(ImNodesCol_Pin, pinCol);
        ImNodes::BeginInputAttribute(pin.id);
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImGui::TextColored(ImVec4(1,1,1,1), "%s", pin.name.c_str());
        ImNodes::EndInputAttribute();
        ImNodes::PopColorStyle();

        PinInfo pInfo;
        pInfo.pos = ImVec2(nodePos.x, cursorPos.y + ImGui::GetTextLineHeight() * 0.5f);
        pInfo.color = pinCol;
        pInfo.isInput = true;
        m_pinInfo[pin.id] = pInfo;
    }

    // Output pinleri
    for (auto& pin : node.outputs) {
        bool isScalarOut = (node.type != Engine::Renderer::NodeType::Output &&
                            pin.type == Engine::Renderer::PinType::Float);

        ImU32 pinCol = COLA(0xFFE47B, 1.0f); // default output yellow
        if (isScalarOut) pinCol = COLA(0x82D9FF, 1.0f);
        
        ImNodes::PushColorStyle(ImNodesCol_Pin, pinCol);
        ImNodes::BeginOutputAttribute(pin.id);
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        if (isScalarOut)
            ImGui::Text("Value: %.2f", std::stof(pin.name.empty() ? "0" : pin.name));
        else
            ImGui::Text("%s", pin.name.c_str());
        ImNodes::EndOutputAttribute();
        ImNodes::PopColorStyle();

        PinInfo pInfo;
        pInfo.pos = ImVec2(nodePos.x + nodeDim.x, cursorPos.y + ImGui::GetTextLineHeight() * 0.5f);
        pInfo.color = pinCol;
        pInfo.isInput = false;
        m_pinInfo[pin.id] = pInfo;
    }

    ImNodes::EndNode();
}

// ─── Context menu ─────────────────────────────────────────────────────────────
void MaterialEditorPanel::handleContextMenu() {
    if (ImGui::BeginPopupContextWindow("##NodeCtx")) {
        if (ImGui::MenuItem("Add Color Node"))    addColorNode();
        if (ImGui::MenuItem("Add Scalar (0.5)"))  addScalarNode("Scalar", 0.5f);
        ImGui::EndPopup();
    }
}

// ─── Node ekleme yardımcıları ─────────────────────────────────────────────────
void MaterialEditorPanel::addColorNode() {
    Engine::Renderer::ShaderNode node;
    node.id   = m_graph.generateId();
    node.name = "Color";
    node.type = Engine::Renderer::NodeType::Color;

    Engine::Renderer::ShaderPin out;
    out.id     = m_graph.generateId();
    out.nodeId = node.id;
    out.name   = "RGB";
    out.type   = Engine::Renderer::PinType::Vec3;
    out.kind   = Engine::Renderer::PinKind::Output;
    node.outputs.push_back(out);
    m_graph.nodes.push_back(node);
}

void MaterialEditorPanel::addScalarNode(const char* label, float value) {
    Engine::Renderer::ShaderNode node;
    node.id   = m_graph.generateId();
    node.name = label;
    node.type = Engine::Renderer::NodeType::Color;

    Engine::Renderer::ShaderPin out;
    out.id     = m_graph.generateId();
    out.nodeId = node.id;
    char vbuf[16]; snprintf(vbuf,sizeof(vbuf),"%.2f",value);
    out.name   = vbuf;
    out.type   = Engine::Renderer::PinType::Float;
    out.kind   = Engine::Renderer::PinKind::Output;
    node.outputs.push_back(out);
    m_graph.nodes.push_back(node);
}

void MaterialEditorPanel::compileGraph() {
    std::cout << "[MaterialEditor] Compiling shader graph...\n";
    Engine::Renderer::ShaderGraphCompiler compiler;
    uint16_t newProgramHandle;
    if (compiler.compileGraph(&m_graph, newProgramHandle)) {
        std::cout << "[MaterialEditor] Compiled OK.\n";
        bgfx::ProgramHandle h = {newProgramHandle};

        // Eğer seçili bir nesne varsa ve bu bir Part ise, sadece ona uygula
        auto selected = SelectionManager::instance().getSelected();
        if (selected && selected->getClassName() == "Part") {
            auto part = std::dynamic_pointer_cast<Part>(selected);
            if (part) {
                part->setCustomShader(newProgramHandle);
                std::cout << "[MaterialEditor] Applied shader to selected Part: " << part->name << "\n";
            }
        } else {
            // Varsayılan olarak tüm sahneyi override et
            Engine::Renderer::RendererSystem::instance().setOverrideMaterial(h);
            std::cout << "[MaterialEditor] Applied override shader to entire scene.\n";
        }
    } else {
        std::cerr << "[MaterialEditor] Compile FAILED.\n";
    }
}

} // namespace Editor::UI
