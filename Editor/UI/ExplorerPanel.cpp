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

// ─── Sınıf → ikon/renk tablosu ─────────────────────────────────────────────
struct ClassMeta { const char* iconKey; const char* emojiFallback; ImU32 barColor; };

static ClassMeta getClassMeta(const std::string& cls) {
    if (cls == "Workspace") return {"icon_world",    "[W]", COLA(0x9CA3AF, 1.0f)};
    if (cls == "Part")      return {"icon_part",     "[P]", COLA(0x00d2ff, 1.0f)}; // Cyan
    if (cls == "MeshPart")  return {"icon_mesh",     "[M]", COLA(0xf59e0b, 1.0f)}; // Orange
    if (cls == "Script")    return {"icon_script",   "[S]", COLA(0x2dd4bf, 1.0f)}; // Teal
    if (cls == "Camera")    return {"icon_camera",   "[C]", COLA(0xf97316, 1.0f)}; // Orange
    if (cls == "DirectionalLight") return {"icon_light","[L]", COLA(0xBBFF00, 1.0f)}; // LimeYellow
    if (cls == "Skybox")    return {"icon_sky",      "[S]", COLA(0x22d3ee, 1.0f)}; // Cyan 400
    if (cls == "Model")     return {"icon_model",    "[M]", COLA(0xA7FF71, 1.0f)}; // LimeGreen
    if (cls == "Bone")      return {"icon_bone",     "[B]", COLA(0x9ca3af, 1.0f)}; // Gray
    if (cls == "Folder")    return {"icon_folder",   "[F]", COLA(0xA7FF71, 1.0f)}; // LimeGreen
    if (cls == "ParticleSystem") return {"icon_fx",  "[P]", COLA(0xFFDD6C, 1.0f)}; // ParticleYellow
    if (cls == "Lighting")  return {"icon_light2",   "[L]", COLA(0x9ca3af, 1.0f)}; // Gray
    if (cls == "ServerScriptService") return {"icon_svr","[S]", COLA(0xFFE47B, 1.0f)}; // Yellow
    if (cls == "SoundService") return {"icon_snd",   "[S]", COLA(0x9ca3af, 1.0f)};
    if (cls == "Manager")   return {"icon_mgr",      "[M]", COLA(0x3b82f6, 1.0f)}; // Blue 500
    if (cls == "Item")      return {"icon_item",     "[I]", COLA(0xa855f7, 1.0f)}; // Purple 500
    return {"icon_generic", "[?]", COLA(0x6b7280, 1.0f)}; // Gray
}

void ExplorerPanel::draw() {
    auto& T = NexusTheme::instance();
    
    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Explorer", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar);

    float width = ImGui::GetWindowWidth();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ─────────────────────────────────────────────────────────────────────────
    // HEADER TABS (h=30)
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.bgPanel);
    ImGui::BeginChild("##ExplHdr", ImVec2(width, 30), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        
        const char* tabs[] = {"Explorer", "History"};
        float cx = p.x;
        
        for (int i=0; i<2; i++) {
            bool active = (i == 0);
            ImVec2 ts = ImGui::CalcTextSize(tabs[i]);
            float tabW = ts.x + 20.0f; // 10px padding
            
            ImVec2 r(cx, p.y);
            ImGui::SetCursorScreenPos(r);
            ImGui::InvisibleButton(tabs[i], ImVec2(tabW, 30));
            
            ImU32 textCol = active ? COL(T.textPrimary) : COL(T.textMuted);
            dl->AddText(ImVec2(r.x + 10, r.y + 8), textCol, tabs[i]);
            
            cx += tabW;
        }

        // Action buttons
        float btnW = 24.0f;
        float rx = p.x + width - btnW*2 - 10.0f;
        
        ImGui::SetCursorScreenPos(ImVec2(rx, p.y+3));
        ImGui::InvisibleButton("##add", ImVec2(btnW, 24));
        if (ImGui::IsItemHovered()) dl->AddRectFilled(ImVec2(rx, p.y+3), ImVec2(rx+btnW, p.y+27), COL(T.panelHover), 4.0f);
        dl->AddText(ImVec2(rx + 8, p.y + 5), COL(T.textMuted), "+");
        if (ImGui::IsItemClicked()) { ImGui::OpenPopup("ExplorerInsertPopup"); }
        
        rx += btnW;
        ImGui::SetCursorScreenPos(ImVec2(rx, p.y+3));
        ImGui::InvisibleButton("##more", ImVec2(btnW, 24));
        if (ImGui::IsItemHovered()) dl->AddRectFilled(ImVec2(rx, p.y+3), ImVec2(rx+btnW, p.y+27), COL(T.panelHover), 4.0f);
        dl->AddText(ImVec2(rx + 6, p.y + 3), COL(T.textMuted), "...");
        
        if (ImGui::BeginPopup("ExplorerInsertPopup")) {
            drawInsertObjectMenu(DataModel::instance());
            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // TopBar bottom border (drawn on top of header)
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddLine(ImVec2(p.x, p.y - 1), ImVec2(p.x + width, p.y - 1), COL(T.border));

    // ─────────────────────────────────────────────────────────────────────────
    // TREE BODY
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.bgDeepest);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); 
    
    ImGui::BeginChild("##ExplBody", ImVec2(width, ImGui::GetContentRegionAvail().y), false, 0);

    for (auto& child : DataModel::instance()->getChildren()) {
        drawInstanceNode(child);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    // Right and bottom panel border
    ImVec2 winP = ImGui::GetWindowPos();
    float winH = ImGui::GetWindowHeight();
    dl->AddLine(ImVec2(winP.x + width - 1.0f, winP.y), ImVec2(winP.x + width - 1.0f, winP.y + winH), COL(T.border));
    dl->AddLine(ImVec2(winP.x, winP.y + winH - 1.0f), ImVec2(winP.x + width, winP.y + winH - 1.0f), COL(T.border));

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
    float width = ImGui::GetWindowWidth();
    float rowHeight = 20.0f; // Spec: Her satır 20px yükseklik

    // Determine depth based on current X vs window pos
    float winX = ImGui::GetWindowPos().x;
    float indentLevel = pos.x - winX; 
    // Indent starts at 0 for root, +18px per depth. Wait, ImGui does weird things. We'll use indentLevel.

    // Row interaction
    ImGui::PushID(inst.get());
    ImGui::InvisibleButton("##row", ImVec2(width - indentLevel, rowHeight));
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
    ImVec2 rowMin = ImVec2(winX, pos.y);
    ImVec2 rowMax = ImVec2(rowMin.x + width, rowMin.y + rowHeight);

    if (isSelected) {
        dl->AddRectFilled(rowMin, rowMax, COLA(0x00d2ff, 0.15f));
    } else if (hovered) {
        dl->AddRectFilled(rowMin, rowMax, COL(T.panelHover));
    }

    float cx = pos.x; 

    // Extra padding for root nodes (Workspace, ServerScriptService)
    if (indentLevel < 1.0f) cx += 10.0f;

    // Arrow
    if (hasChildren) {
        ImTextureID chevron = IconRegistry::instance().get(open ? "icon_chevron_down" : "icon_chevron_right");
        if (chevron) {
            dl->AddImage(chevron, ImVec2(cx, pos.y + 4.0f), ImVec2(cx + 12.0f, pos.y + 16.0f), ImVec2(0,0), ImVec2(1,1), isSelected ? COL(T.accent) : COL(T.textMuted));
        } else {
            const char* arrow = open ? "v" : ">";
            dl->AddText(ImVec2(cx, pos.y + 3.0f), isSelected ? COL(T.accent) : COL(T.textMuted), arrow);
        }
        
        if (hovered && ImGui::IsMouseClicked(0)) {
            if (ImGui::GetMousePos().x < cx + 16.0f) {
                open = !open;
            }
        }
    }
    cx += 18.0f; // Spec: indent 18px

    // [vertical divider 1px] [type badge 3×10px] [ikon 16×16px] [metin]
    dl->AddLine(ImVec2(cx, pos.y), ImVec2(cx, pos.y + rowHeight), COLA(0xFFFFFF, 0.20f));
    cx += 6.0f;

    // Type Badge (3x10px)
    dl->AddRectFilled(ImVec2(cx, pos.y + 5.0f), ImVec2(cx + 3.0f, pos.y + 15.0f), meta.barColor, 2.0f);
    cx += 8.0f;

    // PNG Icon (16x16px)
    ImTextureID iconTex = IconRegistry::instance().get(meta.iconKey);
    if (iconTex) {
        ImU32 iconCol = isSelected ? IM_COL32_WHITE : COL(T.textPrimary);
        dl->AddImage(iconTex, ImVec2(cx, pos.y + 2.0f), ImVec2(cx + 16.0f, pos.y + 18.0f), ImVec2(0,0), ImVec2(1,1), iconCol);
    } else {
        dl->AddText(ImVec2(cx, pos.y + 2.0f), COL(T.textPrimary), meta.emojiFallback);
    }
    cx += 16.0f;
    cx += 10.0f; // Gap: 10px

    // Label
    ImU32 textCol = isSelected ? IM_COL32_WHITE : COL(T.textMuted);
    
    // Check if it's a service folder
    if (cls == "ServerScriptService" || cls == "SoundService" || cls == "Lighting") {
        textCol = meta.barColor;
    }

    dl->AddText(ImVec2(cx, pos.y + 3.0f), textCol, inst->name.c_str());

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
        ImGui::Indent(15.0f); // Spec: (18+15)px
        for (auto& child : inst->getChildren()) {
            drawInstanceNode(child);
        }
        ImGui::Unindent(15.0f);
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
