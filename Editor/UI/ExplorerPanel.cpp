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
struct ClassMeta { const char* iconKey; ImU32 barColor; const char* emojiFallback; };

static ClassMeta getClassMeta(const std::string& cls) {
    if (cls == "Part"           ) return {"icon_mesh",      COLA(0x00d2ff, 1.0f), "🧊"}; // cyan
    if (cls == "Script"         ) return {"icon_script",    COLA(0x2DD4BF, 1.0f), "📜"}; // teal
    if (cls == "Camera"         ) return {"icon_camera",    COLA(0xF97316, 1.0f), "📹"}; // orange
    if (cls == "DirectionalLight") return {"icon_light_dir", COLA(0xFACC15, 1.0f), "☀️"}; // yellow
    if (cls == "ParticleSystem" ) return {"icon_particle",  COLA(0xFBBF24, 1.0f), "✨"}; // amber
    if (cls == "Workspace"      ) return {"icon_world",     COLA(0x9CA3AF, 1.0f), "🌍"}; // grey
    if (cls == "Folder"         ) return {"icon_folder",    COLA(0x6B7280, 1.0f), "📂"}; // grey
    return                               {"icon_mesh",      COLA(0x6B7280, 1.0f), "📦"};
}

// ─── Küçük ikon çizici ──────────────────────────────────────────────────────
static void DrawInlineIcon(ImDrawList* dl, const char* key, const char* fallback, ImVec2 pos, float size, ImU32 col) {
    ImTextureID tex = IconRegistry::instance().get(key);
    if (tex)
        dl->AddImage(tex, pos, {pos.x+size, pos.y+size}, {0,0},{1,1}, col);
    else
        dl->AddText(pos, IM_COL32(255,255,255,255), fallback);
}

void ExplorerPanel::draw() {
    auto& T = NexusTheme::instance();

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Explorer");

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    float       width = ImGui::GetWindowWidth();

    // ─────────────────────────────────────────────────────────────────────────
    // HEADER (h=28)
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.panel);
    ImGui::BeginChild("##ExplHdr", ImVec2(width, 28), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 base = ImGui::GetCursorScreenPos();
        // Bottom border
        dl->AddLine(ImVec2(base.x, base.y + 27), ImVec2(base.x + width, base.y + 27), COL(T.border));

        // Tabs
        struct TabDef { const char* label; const char* icon; float w; };
        static const TabDef tabs[] = {
            {" Explorer", "▼", 80.0f},
            {" World", "🌍", 70.0f},
            {" History", "🕒", 75.0f}
        };

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));
        
        float cx = 0.0f;
        for (int i = 0; i < 3; i++) {
            bool active = (s_activeTab == i);
            ImGui::SetCursorPos(ImVec2(cx, 0));
            ImGui::PushStyleColor(ImGuiCol_Button, active ? COL(T.bg) : (ImU32)0);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL(T.panelHover));
            
            char id[32]; snprintf(id, sizeof(id), "##exptab%d", i);
            if (ImGui::Button(id, ImVec2(tabs[i].w, 28))) s_activeTab = i;
            
            ImVec2 r = ImGui::GetItemRectMin();
            ImU32 textCol = active ? COL(T.textPrimary) : COL(T.textMuted);
            char labelBuf[64]; snprintf(labelBuf, sizeof(labelBuf), "%s %s", tabs[i].icon, tabs[i].label);
            dl->AddText(ImVec2(r.x + 8, r.y + 6), textCol, labelBuf);
            
            if (active)
                dl->AddLine(ImVec2(r.x, r.y+1), ImVec2(r.x+tabs[i].w, r.y+1), COL(T.accent), 2.0f);
            
            ImGui::PopStyleColor(2);
            cx += tabs[i].w;
            ImGui::SameLine(0,0);
        }
        ImGui::PopStyleVar(3);

        // Action buttons (+ and ...)
        ImGui::SetCursorPos(ImVec2(width - 48, 2));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Text, T.textMuted);
        
        if (ImGui::Button("+", ImVec2(24, 24))) { ImGui::OpenPopup("ExplorerInsertPopup"); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add");
        ImGui::SameLine(0, 0);
        if (ImGui::Button("•••", ImVec2(24, 24))) { }
        
        ImGui::PopStyleColor(2);

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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    
    ImGui::BeginChild("##ExplBody", ImVec2(width, ImGui::GetContentRegionAvail().y), false, 0);

    static bool firstFrame = true;
    if (firstFrame) {
        SelectionManager::instance().select(DataModel::instance());
        firstFrame = false;
    }
    drawInstanceNode(DataModel::instance());

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
}

void ExplorerPanel::drawInstanceNode(const std::shared_ptr<Instance>& inst) {
    auto& T = NexusTheme::instance();
    bool isSelected   = (inst == SelectionManager::instance().getSelected());
    bool isDataModel  = (inst == DataModel::instance());
    const auto& cls   = inst->getClassName();
    ClassMeta meta    = getClassMeta(cls);
    bool hasChildren  = !inst->getChildren().empty();

    static std::map<void*, bool> s_treeState;
    if (isDataModel && s_treeState.find(inst.get()) == s_treeState.end()) {
        s_treeState[inst.get()] = true; // Auto open root
    }
    bool& open = s_treeState[inst.get()];

    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    float rowHeight = 22.0f; // Taller rows to match HTML
    float indent = ImGui::GetCursorPosX() - 4.0f; // Track actual indent level

    // Row interaction
    ImGui::PushID(inst.get());
    ImGui::InvisibleButton("##row", ImVec2(width, rowHeight));
    bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        SelectionManager::instance().select(inst);
    }
    if (ImGui::IsItemClicked(1)) { // Right click
        SelectionManager::instance().select(inst);
        ImGui::OpenPopup("Context");
    }
    
    // Toggle expand on double click or arrow click later
    if (hovered && ImGui::IsMouseDoubleClicked(0) && hasChildren) {
        open = !open;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 rowMin = pos;
    ImVec2 rowMax = ImVec2(pos.x + width, pos.y + rowHeight);

    if (isSelected) {
        dl->AddRectFilled(rowMin, rowMax, COLA(0x00d2ff, 0.15f), 4.0f);
        dl->AddRect(rowMin, rowMax, COLA(0x00d2ff, 0.40f), 4.0f);
    } else if (hovered) {
        dl->AddRectFilled(rowMin, rowMax, COL(T.panelHover), 4.0f);
    }

    float cx = rowMin.x + 4.0f;

    // Arrow
    if (hasChildren) {
        const char* arrow = open ? "▼" : "▶";
        dl->AddText(ImVec2(cx, rowMin.y + 4.0f), isSelected ? COL(T.accent) : COL(T.textMuted), arrow);
        // Toggle on arrow click
        if (hovered && ImGui::IsMouseClicked(0)) {
            if (ImGui::GetMousePos().x < cx + 16.0f) {
                open = !open;
            }
        }
    }
    cx += 16.0f;

    // The little colored bar (w-1 h-3 rounded-full)
    if (!isDataModel) {
        dl->AddRectFilled(ImVec2(cx, rowMin.y + 6.0f), ImVec2(cx + 4.0f, rowMin.y + 16.0f), meta.barColor, 2.0f);
        cx += 8.0f;
    }

    DrawInlineIcon(dl, meta.iconKey, meta.emojiFallback, ImVec2(cx, rowMin.y + 5.0f), 14.0f, isSelected ? IM_COL32(255,255,255,220) : COL(T.textMuted));
    cx += 20.0f;

    // Label
    ImU32 textCol = isSelected ? COL(T.textPrimary) : (isDataModel ? COL(T.textPrimary) : COL(T.textMuted));
    dl->AddText(ImVec2(cx, rowMin.y + 4.0f), textCol, inst->name.c_str());

    // Right side badge if selected
    if (isSelected && !isDataModel) {
        ImVec2 size = ImGui::CalcTextSize(cls.c_str());
        dl->AddRectFilled(ImVec2(rowMax.x - size.x - 16.0f, rowMin.y + 4.0f), 
                          ImVec2(rowMax.x - 4.0f, rowMax.y - 4.0f), 
                          COLA(0x00d2ff, 0.2f), 4.0f);
        dl->AddText(ImVec2(rowMax.x - size.x - 10.0f, rowMin.y + 4.0f), COL(T.accent), cls.c_str());
    }

    if (ImGui::BeginPopup("Context")) {
        drawInsertObjectMenu(inst);
        if (!isDataModel) {
            ImGui::Separator();
            if (ImGui::MenuItem("Rename"))   {}
            if (ImGui::MenuItem("Duplicate")){}
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                inst->setParent(nullptr);
                SelectionManager::instance().clear();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();

    if (open && hasChildren) {
        ImGui::Indent(14.0f);
        
        // Draw vertical guide line for children
        ImVec2 lineP1 = ImVec2(pos.x + 10.0f, pos.y + rowHeight);
        float childrenHeightStart = ImGui::GetCursorScreenPos().y;
        
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
