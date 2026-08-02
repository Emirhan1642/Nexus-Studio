#include "ExplorerPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "SelectionManager.h"
#include "../Undo/UndoStack.h"
#include "Engine/Core/DataModel/DataModel.h"
#include "Engine/Core/Reflection/TypeRegistry.h"
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Scripting/Script.h"
#include "IconRegistry.h"
#include "NexusTheme.h"
#include <map>

// ─── Renk kısayolları ───────────────────────────────────────────────────────
static ImU32 COL(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a) {
    return IM_COL32((hex>>16)&0xFF,(hex>>8)&0xFF,hex&0xFF,(uint8_t)(a*255));
}

// ─── Aktif tab durumu ───────────────────────────────────────────────────────
static int s_activeTab = 0; // 0=Explorer, 1=World, 2=History

// ─── Sınıf → ikon/renk tablosu ─────────────────────────────────────────────
struct ClassMeta { const char* iconKey; const char* emojiFallback; ImU32 barColor; };

static ClassMeta getClassMeta(const std::string& cls) {
    if (cls == "Workspace") return {"icon_world",    "🌍", COLA(0x9CA3AF, 1.0f)};
    if (cls == "Part")      return {"icon_part",     "🧊", COLA(0x00d2ff, 1.0f)}; // Cyan
    if (cls == "MeshPart")  return {"icon_mesh",     "📦", COLA(0xf59e0b, 1.0f)}; // Orange
    if (cls == "Script")    return {"icon_script",   "📜", COLA(0x2dd4bf, 1.0f)}; // Teal
    if (cls == "Camera")    return {"icon_camera",   "📹", COLA(0xf97316, 1.0f)}; // Orange Red
    if (cls == "DirectionalLight") return {"icon_light","☀️", COLA(0xfacc15, 1.0f)}; // Yellow
    if (cls == "Skybox")    return {"icon_sky",      "☁️", COLA(0x22d3ee, 1.0f)}; // Cyan 400
    if (cls == "Model")     return {"icon_model",    "🏃", COLA(0x2dd4bf, 1.0f)}; // Teal 400
    if (cls == "Bone")      return {"icon_bone",     "🦴", COLA(0x9ca3af, 1.0f)}; // Gray
    if (cls == "Folder")    return {"icon_folder",   "📂", COLA(0x9ca3af, 1.0f)}; // Gray
    if (cls == "ParticleSystem") return {"icon_fx",  "✨", COLA(0xfcd34d, 1.0f)}; // Amber 300
    if (cls == "Lighting")  return {"icon_light2",   "💡", COLA(0x9ca3af, 1.0f)}; // Gray
    if (cls == "ServerScriptService") return {"icon_svr","🛠️", COLA(0x9ca3af, 1.0f)};
    if (cls == "SoundService") return {"icon_snd",   "🔊", COLA(0x9ca3af, 1.0f)};
    if (cls == "Manager")   return {"icon_mgr",      "⚙️", COLA(0x3b82f6, 1.0f)}; // Blue 500
    if (cls == "Item")      return {"icon_item",     "🔮", COLA(0xa855f7, 1.0f)}; // Purple 500
    return {"icon_generic", "⚙️", COLA(0x6b7280, 1.0f)}; // Gray
}

void ExplorerPanel::draw() {
    auto& T = NexusTheme::instance();
    
    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Explorer", nullptr, ImGuiWindowFlags_NoScrollbar);

    float width = ImGui::GetWindowWidth();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ─────────────────────────────────────────────────────────────────────────
    // HEADER TABS (h=28)
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.panel);
    ImGui::BeginChild("##ExplHdr", ImVec2(width, 28), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(p.x, p.y + 27), ImVec2(p.x + width, p.y + 27), COL(T.border));

        struct Tab { const char* label; const char* icon; float w; };
        Tab tabs[] = { {"Explorer", "▼", 85.0f}, {"World", "🌍", 65.0f}, {"History", "🕒", 75.0f} };
        
        float cx = p.x;
        
        ImGuiIO& io = ImGui::GetIO();
        if (io.Fonts->Fonts.Size > 3) ImGui::PushFont(io.Fonts->Fonts[3]); // Medium Font
        
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0,0));

        for (int i=0; i<3; i++) {
            bool active = (i == 0);
            ImVec2 r(cx, p.y);
            
            ImGui::SetCursorScreenPos(r);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, T.panelHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, T.panelHover);
            
            if (active) dl->AddRectFilled(r, ImVec2(r.x + tabs[i].w, r.y + 28), COL(T.bg));

            ImGui::Button(tabs[i].label, ImVec2(tabs[i].w, 28));
            if (ImGui::IsItemClicked()) { /* Switch tab logic */ }

            ImU32 textCol = active ? IM_COL32_WHITE : COL(T.textMuted);
            char labelBuf[64]; snprintf(labelBuf, sizeof(labelBuf), "%s %s", tabs[i].icon, tabs[i].label);
            dl->AddText(ImVec2(r.x + 8, r.y + 6), textCol, labelBuf);
            
            if (active)
                dl->AddLine(ImVec2(r.x, r.y+1), ImVec2(r.x+tabs[i].w, r.y+1), COL(T.accent), 2.0f);
            
            ImGui::PopStyleColor(3);
            cx += tabs[i].w;
            ImGui::SameLine(0,0);
        }
        ImGui::PopStyleVar(3);

        // Action buttons
        ImGui::SetCursorPos(ImVec2(width - 48, 2));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Text, T.textMuted);
        
        if (ImGui::Button("+", ImVec2(24, 24))) { ImGui::OpenPopup("ExplorerInsertPopup"); }
        ImGui::SameLine(0, 0);
        if (ImGui::Button("•••", ImVec2(24, 24))) { }
        
        ImGui::PopStyleColor(2);

        if (io.Fonts->Fonts.Size > 3) ImGui::PopFont();
        
        if (ImGui::BeginPopup("ExplorerInsertPopup")) {
            drawInsertObjectMenu(DataModel::instance());
            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ─────────────────────────────────────────────────────────────────────────
    // TREE BODY
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.panel);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6)); 
    
    ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts->Fonts.Size > 1) ImGui::PushFont(io.Fonts->Fonts[1]); // Small Font
    
    ImGui::BeginChild("##ExplBody", ImVec2(width, ImGui::GetContentRegionAvail().y), false, 0);

    static bool firstFrame = true;
    if (firstFrame) {
        firstFrame = false;
    }
    
    for (auto& child : DataModel::instance()->getChildren()) {
        drawInstanceNode(child);
    }

    ImGui::EndChild();
    if (io.Fonts->Fonts.Size > 1) ImGui::PopFont();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
}

void ExplorerPanel::drawInstanceNode(const std::shared_ptr<Instance>& inst) {
    auto& T = NexusTheme::instance();
    bool isSelected   = (inst == SelectionManager::instance().getSelected());
    const auto& cls   = inst->getClassName();
    ClassMeta meta    = getClassMeta(cls);
    bool hasChildren  = !inst->getChildren().empty();

    static std::map<void*, bool> s_treeState;
    if ((cls == "Workspace" || cls == "Model" || cls == "Folder") && s_treeState.find(inst.get()) == s_treeState.end()) {
        s_treeState[inst.get()] = true; // Auto open main nodes
    }
    bool& open = s_treeState[inst.get()];

    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    float rowHeight = 24.0f; 

    // Row interaction
    ImGui::PushID(inst.get());
    ImGui::InvisibleButton("##row", ImVec2(width, rowHeight));
    bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        SelectionManager::instance().select(inst);
    }
    if (ImGui::IsItemClicked(1)) {
        SelectionManager::instance().select(inst);
        ImGui::OpenPopup("Context");
    }
    
    if (hovered && ImGui::IsMouseDoubleClicked(0) && hasChildren) {
        open = !open;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 rowMin = pos;
    ImVec2 rowMax = ImVec2(pos.x + width, pos.y + rowHeight);

    if (isSelected) {
        dl->AddRectFilled(rowMin, rowMax, COLA(0x00d2ff, 0.15f), 4.0f);
    } else if (hovered) {
        dl->AddRectFilled(rowMin, rowMax, COL(T.panelHover), 4.0f);
    }

    float cx = rowMin.x + 4.0f;

    // Arrow
    if (hasChildren) {
        const char* arrow = open ? "▼" : "▶";
        dl->AddText(ImVec2(cx, rowMin.y + 4.0f), isSelected ? COL(T.accent) : COL(T.textMuted), arrow);
        if (hovered && ImGui::IsMouseClicked(0)) {
            if (ImGui::GetMousePos().x < cx + 16.0f) {
                open = !open;
            }
        }
    }
    cx += 16.0f;

    // The little colored bar
    dl->AddRectFilled(ImVec2(cx, rowMin.y + 6.0f), ImVec2(cx + 4.0f, rowMin.y + 18.0f), meta.barColor, 2.0f);
    cx += 8.0f;

    // PNG Icon
    ImTextureID iconTex = IconRegistry::instance().get(meta.iconKey);
    if (iconTex) {
        ImU32 iconCol = isSelected ? IM_COL32_WHITE : COL(T.textPrimary);
        dl->AddImage(iconTex, ImVec2(cx, rowMin.y + 4.0f), ImVec2(cx + 16.0f, rowMin.y + 20.0f), ImVec2(0,0), ImVec2(1,1), iconCol);
    }
    cx += 24.0f;

    // Label
    ImU32 textCol = isSelected ? IM_COL32_WHITE : COL(T.textMuted);
    dl->AddText(ImVec2(cx, rowMin.y + 4.0f), textCol, inst->name.c_str());

    // Right side badge if selected
    if (isSelected) {
        const char* clsTag = "PBR"; 
        ImVec2 size = ImGui::CalcTextSize(clsTag);
        dl->AddRectFilled(ImVec2(rowMax.x - size.x - 12.0f, rowMin.y + 4.0f), 
                          ImVec2(rowMax.x - 4.0f, rowMax.y - 4.0f), 
                          COLA(0x00d2ff, 0.2f), 4.0f);
        dl->AddText(ImVec2(rowMax.x - size.x - 8.0f, rowMin.y + 4.0f), COL(T.accent), clsTag);
    }

    if (ImGui::BeginPopup("Context")) {
        drawInsertObjectMenu(inst);
        ImGui::Separator();
        if (ImGui::MenuItem("Rename"))   {}
        if (ImGui::MenuItem("Duplicate")){}
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
            inst->setParent(nullptr);
            SelectionManager::instance().clear();
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();

    if (open && hasChildren) {
        ImGui::Indent(16.0f); 
        
        ImVec2 lineP1 = ImVec2(pos.x + 8.0f, pos.y + rowHeight);
        
        for (auto& child : inst->getChildren()) {
            drawInstanceNode(child);
        }
        
        float childrenHeightEnd = ImGui::GetCursorScreenPos().y;
        dl->AddLine(lineP1, ImVec2(lineP1.x, childrenHeightEnd - rowHeight * 0.5f), COLA(0x242424, 1.0f), 1.0f);
        
        ImGui::Unindent(14.0f);
    }
}

void ExplorerPanel::drawInsertObjectMenu(const std::shared_ptr<Instance>& parent) {
    if (ImGui::BeginMenu("Insert Object")) {
        if (ImGui::MenuItem("Part")) {
            auto obj = std::make_shared<Part>();
            UndoStack::instance().pushCreateCommand(obj, parent);
        }
        if (ImGui::MenuItem("Script")) {
            auto obj = std::make_shared<Script>();
            UndoStack::instance().pushCreateCommand(obj, parent);
        }
        ImGui::EndMenu();
    }
}
