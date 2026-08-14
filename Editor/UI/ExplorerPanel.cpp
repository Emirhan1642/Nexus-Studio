#include "ExplorerPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "SelectionManager.h"
#include "../Undo/UndoStack.h"
#include "Engine/Core/DataModel/DataModel.h"
#include "Engine/Core/Reflection/TypeRegistry.h"
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Core/DataModel/SpringConstraint.h"
#include "Engine/Core/DataModel/HingeConstraint.h"
#include "Engine/Core/DataModel/WeldConstraint.h"
#include "Engine/Core/DataModel/IKControl.h"
#include "Engine/Scripting/Script.h"
#include "IconRegistry.h"
#include "NexusTheme.h"
#include "EditorLayout.h"
#include "SharedTabBar.h"
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>

// ─── Color Helpers ───────────────────────────────────────────────────────────
static ImU32 COL(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a) {
    return IM_COL32((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF, (uint8_t)(a * 255));
}

// ─── Tree Expand/Collapse State ──────────────────────────────────────────────
static std::map<void*, bool> s_treeState;

// ─── Inline Renaming State ───────────────────────────────────────────────────
static const Instance* s_renamingInstance = nullptr;
static char s_renameBuf[128] = "";

// ─── Git Status Registry ─────────────────────────────────────────────────────
static std::map<const Instance*, GitStatus> s_gitStatuses;

void ExplorerPanel::setGitStatus(const Instance* inst, GitStatus status) {
    if (inst) s_gitStatuses[inst] = status;
}

GitStatus ExplorerPanel::getGitStatus(const Instance* inst) {
    auto it = s_gitStatuses.find(inst);
    return (it != s_gitStatuses.end()) ? it->second : GitStatus::None;
}

// ─── Class Meta Info ─────────────────────────────────────────────────────────
struct ClassMeta {
    const char* iconKey;
    const char* emojiFallback;
    bool hasBar;
    ImU32 barColor;
};

static ClassMeta getClassMeta(const std::string& cls, const std::string& name) {
    // Only show colored vertical bar for:
    // 1. Folders (GameModels, Materials, Folder)
    // 2. Scene objects: MainCamera, SkyBox, DirectionalLight, ParticleEffect
    // 3. Script / Code files

    if (cls == "Workspace" || name == "Workspace") 
        return { "icon_world_bold", "[W]", false, 0 };
    
    if (cls == "Camera" || name == "Camera" || name == "MainCamera") 
        return { "icon_camera_bold", "[C]", true, COLA(0xFF7700, 1.0f) }; // Orange
    
    if (cls == "Skybox" || name == "Skybox" || name == "SkyBox") 
        return { "icon_sky_bold", "[S]", true, COLA(0x82D9FF, 1.0f) }; // Cyan
    
    if (cls == "DirectionalLight" || name == "Directional Light" || name == "DirectionalLight") 
        return { "icon_light_bold", "[L]", true, COLA(0xBBFF00, 1.0f) }; // LimeYellow
    
    if (cls == "ParticleSystem" || name == "ParticleEffect" || name == "ParticleSystem") 
        return { "icon_fx_bold", "[P]", true, COLA(0xFFDD6C, 1.0f) }; // ParticleYellow
    
    if (cls == "Script" || name.find(".luau") != std::string::npos || name.find(".cpp") != std::string::npos) 
        return { "icon_script_bold", "[S]", true, COLA(0x66FF99, 1.0f) }; // Green
    
    if (name == "Materials" || cls == "Material") 
        return { "icon_folder_bold", "[F]", true, COLA(0xFD71FF, 1.0f) }; // Purple Folder
    
    if (cls == "Folder" || name == "GameModels" || cls == "Model") 
        return { "icon_folder_bold", "[F]", true, COLA(0xA7FF71, 1.0f) }; // LimeGreen Folder

    if (cls == "Part") 
        return { "icon_mesh_bold", "[P]", false, 0 };
    
    if (cls == "MeshPart") 
        return { "icon_mesh2_bold", "[M]", false, 0 };

    if (name.find(".fbx") != std::string::npos || name.find(".mesh") != std::string::npos)
        return { "icon_mesh_bold", "[M]", false, 0 };

    if (name.find(".mat") != std::string::npos)
        return { "icon_material_bold", "[M]", false, 0 };

    if (cls == "ServerScriptService" || name == "ServerScriptService") 
        return { "icon_svr_bold", "[S]", false, 0 };
    
    if (cls == "Lighting" || name == "Lighting" || name == "LightingService") 
        return { "icon_light2_bold", "[L]", false, 0 };
    
    if (cls == "SoundService" || name == "SoundService") 
        return { "icon_snd_bold", "[S]", false, 0 };
    
    if (cls == "ReplicatedStorage" || name == "ReplicatedStorage") 
        return { "icon_box", "[R]", false, 0 };

    if (cls == "SpringConstraint" || cls == "HingeConstraint" || cls == "WeldConstraint" || cls == "IKControl")
        return { "icon_transform_bold", "[T]", false, 0 };

    return { "icon_box", "[?]", false, 0 };
}

// ─── Main Panel Draw ─────────────────────────────────────────────────────────
void ExplorerPanel::draw() {
    auto& T = NexusTheme::instance();
    
    if (!EditorLayout::instance().showExplorer) return;

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_HiddenTabBar;
    ImGui::SetNextWindowClass(&window_class);
    float minW = Editor::UI::CalculateNodeMinTabWidth("Explorer");
    ImGui::SetNextWindowSizeConstraints(ImVec2(minW, 80.0f), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.bgPanel);
    ImGui::Begin("Explorer", &EditorLayout::instance().showExplorer, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
    ImGui::PopStyleColor();

    // Standard Tab Header matching other panels
    Editor::UI::DrawSingleTabHeader("Explorer", "icon_explorer_bold", 150.0f, ImGui::ColorConvertFloat4ToU32(T.accent));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float w = ImGui::GetWindowWidth();
    float h = ImGui::GetWindowHeight();

    // ─────────────────────────────────────────────────────────────────────────
    // TREE BODY
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.bgDeepest);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 4.0f)); 
    
    ImGui::BeginChild("##ExplBody", ImVec2(w, ImGui::GetContentRegionAvail().y), false, 0);

    for (auto& child : DataModel::instance()->getChildren()) {
        drawInstanceNode(child, 0);
    }

    // Right Click on empty space in Explorer
    if (ImGui::BeginPopupContextWindow("##ExplEmptyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("+ Insert Object...")) {
            m_insertTarget = DataModel::instance();
            m_openInsertPopup = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Expand All")) {
            auto expandAll = [](auto& self, const std::shared_ptr<Instance>& node) -> void {
                s_treeState[node.get()] = true;
                for (auto& c : node->getChildren()) self(self, c);
            };
            for (auto& c : DataModel::instance()->getChildren()) expandAll(expandAll, c);
        }
        if (ImGui::MenuItem("Collapse All")) {
            s_treeState.clear();
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    // ─────────────────────────────────────────────────────────────────────────
    // INSERT OBJECT MODAL / POPUP
    // ─────────────────────────────────────────────────────────────────────────
    if (m_openInsertPopup) {
        ImGui::OpenPopup("InsertObjectPopup");
        m_openInsertPopup = false;
        m_insertSearch[0] = '\0';
    }

    drawInsertObjectPopup();

    // Panel Outer Borders
    dl->AddLine(ImVec2(p.x + w - 1.0f, p.y), ImVec2(p.x + w - 1.0f, p.y + h), COL(T.border));
    dl->AddLine(ImVec2(p.x, p.y + h - 1.0f), ImVec2(p.x + w, p.y + h - 1.0f), COL(T.border));

    ImGui::End();
    ImGui::PopStyleVar();
}

// ─── Draw Node (With Precise Guide Lines, Colored Bars, Git Status, Hover +) ───
void ExplorerPanel::drawInstanceNode(const std::shared_ptr<Instance>& inst, int depth) {
    if (!inst) return;

    auto& T = NexusTheme::instance();
    bool isSelected = (inst == SelectionManager::instance().getSelected());
    std::string cls = inst->getClassName();
    std::string name = inst->name;
    
    ClassMeta meta = getClassMeta(cls, name);
    bool hasChildren = !inst->getChildren().empty();

    if ((cls == "Workspace" || cls == "Model" || cls == "Folder" || name == "GameModels" || name == "Materials") && 
        s_treeState.find(inst.get()) == s_treeState.end()) {
        s_treeState[inst.get()] = true; // Auto open primary nodes
    }
    bool& open = s_treeState[inst.get()];

    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetWindowWidth();
    float rowHeight = 22.0f;
    float winX = ImGui::GetWindowPos().x;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 rowMin = ImVec2(winX, pos.y);
    ImVec2 rowMax = ImVec2(rowMin.x + width, rowMin.y + rowHeight);

    // Row Interaction Button
    ImGui::PushID(inst.get());
    ImGui::InvisibleButton("##row", ImVec2(width, rowHeight));
    bool hovered = ImGui::IsItemHovered();

    if (ImGui::IsItemClicked(0)) {
        SelectionManager::instance().select(inst);
    }
    if (ImGui::IsItemClicked(1)) {
        SelectionManager::instance().select(inst);
        ImGui::OpenPopup("Context");
    }
    if (hovered && ImGui::IsMouseDoubleClicked(0) && hasChildren) {
        open = !open;
    }

    // Row Highlight
    if (isSelected) {
        dl->AddRectFilled(rowMin, rowMax, COLA(0x82D9FF, 0.15f));
    } else if (hovered) {
        dl->AddRectFilled(rowMin, rowMax, COL(T.panelHover));
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 1. VERTICAL HIERARCHY GUIDE LINES (Aligned precisely under parent chevrons)
    // ─────────────────────────────────────────────────────────────────────────
    float indentStep = 18.0f;
    float basePad = 10.0f;

    for (int d = 0; d < depth; d++) {
        float lineX = winX + basePad + 8.0f + d * indentStep;
        dl->AddLine(ImVec2(lineX, pos.y), ImVec2(lineX, pos.y + rowHeight), COLA(0xFFFFFF, 0.20f));
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 2. CHEVRON / ARROW AT CURRENT DEPTH
    // ─────────────────────────────────────────────────────────────────────────
    float curX = winX + basePad + depth * indentStep;

    if (hasChildren) {
        ImTextureID chevron = IconRegistry::instance().get(open ? "icon_chevron_down_bold" : "icon_chevron_right_bold");
        if (!chevron) chevron = IconRegistry::instance().get(open ? "icon_chevron_down" : "icon_chevron_right");
        
        ImVec2 chPos(curX, pos.y + 3.0f);
        if (chevron) {
            dl->AddImage(chevron, chPos, ImVec2(chPos.x + 16.0f, chPos.y + 16.0f), ImVec2(0,0), ImVec2(1,1), isSelected ? COL(T.accent) : IM_COL32_WHITE);
        } else {
            const char* arrow = open ? "v" : ">";
            dl->AddText(ImVec2(curX + 2.0f, pos.y + 3.0f), isSelected ? COL(T.accent) : IM_COL32_WHITE, arrow);
        }

        // Toggle on chevron click
        if (hovered && ImGui::IsMouseClicked(0)) {
            if (ImGui::GetMousePos().x >= curX && ImGui::GetMousePos().x <= curX + 16.0f) {
                open = !open;
            }
        }
    }

    curX += 18.0f;

    // ─────────────────────────────────────────────────────────────────────────
    // 3. TYPE BADGE (3x10px Colored Bar - Only for Folders, Camera, Skybox, Light, Scripts)
    // ─────────────────────────────────────────────────────────────────────────
    if (meta.hasBar) {
        dl->AddRectFilled(ImVec2(curX, pos.y + 6.0f), ImVec2(curX + 3.0f, pos.y + 16.0f), meta.barColor, 1.5f);
        curX += 7.0f;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 4. ICON (16x16px)
    // ─────────────────────────────────────────────────────────────────────────
    ImTextureID iconTex = IconRegistry::instance().get(meta.iconKey);
    if (iconTex) {
        ImU32 iconTint = meta.hasBar ? meta.barColor : (isSelected ? IM_COL32_WHITE : COL(T.textPrimary));
        dl->AddImage(iconTex, ImVec2(curX, pos.y + 3.0f), ImVec2(curX + 16.0f, pos.y + 19.0f), ImVec2(0,0), ImVec2(1,1), iconTint);
    } else {
        dl->AddText(ImVec2(curX, pos.y + 3.0f), COL(T.textPrimary), meta.emojiFallback);
    }
    curX += 22.0f;

    // ─────────────────────────────────────────────────────────────────────────
    // 5. LABEL & GIT STATUS COLORING (Object name does NOT change color, badges are bold)
    // ─────────────────────────────────────────────────────────────────────────
    GitStatus git = getGitStatus(inst.get());
    ImU32 textCol = isSelected ? IM_COL32_WHITE : COL(T.textPrimary);

    if (s_renamingInstance == inst.get()) {
        // Inline Rename Input Field
        ImGui::SetCursorScreenPos(ImVec2(curX, pos.y + 1.0f));
        ImGui::SetNextItemWidth(140.0f);
        ImGui::SetKeyboardFocusHere(0);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, T.bgCard);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));

        if (ImGui::InputText("##InlineRename", s_renameBuf, sizeof(s_renameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string oldN = inst->name;
            std::string newN = s_renameBuf;
            if (!newN.empty()) {
                inst->name = newN;
                UndoStack::instance().pushPropertyChangeCommand(inst, "Name", oldN, newN);
            }
            s_renamingInstance = nullptr;
        }
        if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit()) {
            s_renamingInstance = nullptr;
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    } else {
        // Normal Text Draw
        dl->AddText(ImVec2(curX, pos.y + 3.0f), textCol, name.c_str());

        // Bold Colored Git Badge Beside Name
        if (git == GitStatus::Modified) {
            ImVec2 nSz = ImGui::CalcTextSize(name.c_str());
            ImVec2 bPos(curX + nSz.x + 8.0f, pos.y + 3.0f);
            dl->AddText(bPos, COL(T.accentYellow), "M");
            dl->AddText(ImVec2(bPos.x + 0.5f, bPos.y), COL(T.accentYellow), "M"); // Simulated bold
        } else if (git == GitStatus::Added) {
            ImVec2 nSz = ImGui::CalcTextSize(name.c_str());
            ImVec2 bPos(curX + nSz.x + 8.0f, pos.y + 3.0f);
            dl->AddText(bPos, COL(T.accentGreen), "U");
            dl->AddText(ImVec2(bPos.x + 0.5f, bPos.y), COL(T.accentGreen), "U"); // Simulated bold
        } else if (git == GitStatus::Deleted) {
            ImVec2 nSz = ImGui::CalcTextSize(name.c_str());
            ImVec2 bPos(curX + nSz.x + 8.0f, pos.y + 3.0f);
            dl->AddText(bPos, COL(T.accentRed), "D");
            dl->AddText(ImVec2(bPos.x + 0.5f, bPos.y), COL(T.accentRed), "D"); // Simulated bold
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 6. HOVER '+' INSERT BUTTON (14x14px, does not shift layout)
    // ─────────────────────────────────────────────────────────────────────────
    if (hovered) {
        float plusW = 14.0f, plusH = 14.0f;
        float plusX = rowMax.x - 22.0f;
        float plusY = pos.y + 4.0f;
        ImVec2 pMin(plusX, plusY);
        ImVec2 pMax(plusX + plusW, plusY + plusH);

        dl->AddRectFilled(pMin, pMax, IM_COL32(40, 40, 40, 220), 3.0f);
        dl->AddRect(pMin, pMax, COLA(0xFFFFFF, 0.20f), 3.0f);

        ImTextureID pTex = IconRegistry::instance().get("icon_plus");
        if (pTex) {
            dl->AddImage(pTex, ImVec2(plusX + 2.0f, plusY + 2.0f), ImVec2(plusX + 12.0f, plusY + 12.0f));
        } else {
            dl->AddText(ImVec2(plusX + 3.0f, plusY - 1.0f), IM_COL32_WHITE, "+");
        }

        // Direct click detection on the + button without submitting extra ImGui widgets
        if (ImGui::IsMouseClicked(0)) {
            ImVec2 mpos = ImGui::GetMousePos();
            if (mpos.x >= plusX && mpos.x <= plusX + plusW && mpos.y >= plusY && mpos.y <= plusY + plusH) {
                m_insertTarget = inst;
                m_openInsertPopup = true;
            }
        }
    }

    // Context Menu
    if (ImGui::BeginPopup("Context")) {
        if (ImGui::MenuItem("+ Insert Object...")) {
            m_insertTarget = inst;
            m_openInsertPopup = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Rename", "F2")) {
            s_renamingInstance = inst.get();
            strcpy_s(s_renameBuf, inst->name.c_str());
        }
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
            duplicateInstance(inst);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete", "Del")) {
            inst->setParent(nullptr);
            SelectionManager::instance().clear();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Expand All")) {
            auto expandAll = [](auto& self, const std::shared_ptr<Instance>& node) -> void {
                s_treeState[node.get()] = true;
                for (auto& c : node->getChildren()) self(self, c);
            };
            expandAll(expandAll, inst);
        }
        if (ImGui::MenuItem("Collapse All")) {
            auto collapseAll = [](auto& self, const std::shared_ptr<Instance>& node) -> void {
                s_treeState[node.get()] = false;
                for (auto& c : node->getChildren()) self(self, c);
            };
            collapseAll(collapseAll, inst);
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();

    // ─────────────────────────────────────────────────────────────────────────
    // RECURSE CHILDREN
    // ─────────────────────────────────────────────────────────────────────────
    if (open && hasChildren) {
        for (auto& child : inst->getChildren()) {
            drawInstanceNode(child, depth + 1);
        }
    }
}

// ─── Insert Object Popup Panel ────────────────────────────────────────────────
void ExplorerPanel::drawInsertObjectPopup() {
    auto& T = NexusTheme::instance();

    ImGui::SetNextWindowSize(ImVec2(290.0f, 380.0f), ImGuiCond_Appearing);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, T.bgPanel);
    ImGui::PushStyleColor(ImGuiCol_Border, T.border);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);

    if (ImGui::BeginPopup("InsertObjectPopup")) {
        // Header Title
        std::string targetName = m_insertTarget ? m_insertTarget->name : "Workspace";
        ImGui::TextColored(T.textPrimary, "Insert Object");
        ImGui::SameLine();
        ImGui::TextColored(T.accent, "into [%s]", targetName.c_str());

        ImGui::Spacing();

        // Search Bar
        ImGui::PushStyleColor(ImGuiCol_FrameBg, T.bgDeepest);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##InsertSearch", "Search objects...", m_insertSearch, sizeof(m_insertSearch));
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere(-1);
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        ImGui::Separator();
        ImGui::Spacing();

        // Object Definition Structure
        struct InsertableItem {
            std::string className;
            std::string category;
            std::string iconKey;
            std::string description;
        };

        static const std::vector<InsertableItem> s_items = {
            // 3D & Geometry
            { "Part", "3D & Geometry", "icon_mesh_bold", "Standard 3D primitive block part" },
            { "MeshPart", "3D & Geometry", "icon_mesh2_bold", "Mesh geometry loaded from 3D asset" },
            { "Model", "3D & Geometry", "icon_model_bold", "Group container for multiple parts" },
            
            // Scripting
            { "Script", "Scripting", "icon_script_bold", "Server-side Luau execution script" },
            
            // Organization
            { "Folder", "Organization", "icon_folder_bold", "Organizational folder instance" },
            
            // Lighting & Environment
            { "DirectionalLight", "Lighting & Environment", "icon_light_bold", "Sunlight directional source" },
            { "Skybox", "Lighting & Environment", "icon_sky_bold", "Atmospheric environment skybox" },
            { "ParticleSystem", "Lighting & Environment", "icon_fx_bold", "Particle emitter simulation effect" },
            
            // Physics & Constraints
            { "WeldConstraint", "Physics & Constraints", "icon_transform_bold", "Rigid weld joint between parts" },
            { "HingeConstraint", "Physics & Constraints", "icon_transform_bold", "Rotational hinge physics constraint" },
            { "SpringConstraint", "Physics & Constraints", "icon_transform_bold", "Linear spring physics joint" },
            { "IKControl", "Physics & Constraints", "icon_transform_bold", "Inverse kinematics limb controller" },
        };

        std::string searchLower = m_insertSearch;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

        // Group into categories
        std::string lastCat = "";

        ImGui::BeginChild("##InsertList", ImVec2(0, 0), false);

        for (const auto& item : s_items) {
            std::string nameLower = item.className;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

            if (!searchLower.empty() && nameLower.find(searchLower) == std::string::npos && 
                item.category.find(searchLower) == std::string::npos) {
                continue;
            }

            if (item.category != lastCat) {
                lastCat = item.category;
                ImGui::Spacing();
                ImGui::TextColored(T.textSecondary, "%s", lastCat.c_str());
                ImGui::Separator();
            }

            ImGui::PushID(item.className.c_str());
            ImVec2 pMin = ImGui::GetCursorScreenPos();
            float w = ImGui::GetContentRegionAvail().x;
            float rowH = 32.0f;

            if (ImGui::InvisibleButton("##itemBtn", ImVec2(w, rowH))) {
                auto target = m_insertTarget ? m_insertTarget : DataModel::instance();
                createAndInsertObject(item.className, target);
                ImGui::CloseCurrentPopup();
            }
            bool hovered = ImGui::IsItemHovered();

            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (hovered) {
                dl->AddRectFilled(pMin, ImVec2(pMin.x + w, pMin.y + rowH), COL(T.panelHover), 4.0f);
                dl->AddRect(pMin, ImVec2(pMin.x + w, pMin.y + rowH), COL(T.accent), 4.0f);
            }

            // Icon
            ImTextureID iconTex = IconRegistry::instance().get(item.iconKey.c_str());
            if (iconTex) {
                dl->AddImage(iconTex, ImVec2(pMin.x + 6.0f, pMin.y + 6.0f), ImVec2(pMin.x + 24.0f, pMin.y + 24.0f));
            }

            // Class Name
            dl->AddText(ImVec2(pMin.x + 30.0f, pMin.y + 3.0f), IM_COL32_WHITE, item.className.c_str());

            // Description
            dl->AddText(ImVec2(pMin.x + 30.0f, pMin.y + 17.0f), COL(T.textMuted), item.description.c_str());

            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// ─── Create & Insert Instance Factory ─────────────────────────────────────────
void ExplorerPanel::createAndInsertObject(const std::string& className, const std::shared_ptr<Instance>& parent) {
    if (!parent) return;

    std::shared_ptr<Instance> newObj;

    if (className == "Part") {
        auto part = std::make_shared<Part>();
        part->name = "Part";
        part->position = Engine::Math::Vector3(0.0f, 2.0f, 0.0f);
        part->size = Engine::Math::Vector3(4.0f, 1.0f, 2.0f);
        newObj = part;
    } else if (className == "MeshPart") {
        auto part = std::make_shared<Part>();
        part->name = "MeshPart";
        part->customClassName = "MeshPart";
        part->position = Engine::Math::Vector3(0.0f, 2.0f, 0.0f);
        part->size = Engine::Math::Vector3(2.0f, 2.0f, 2.0f);
        newObj = part;
    } else if (className == "Script") {
        auto script = std::make_shared<Script>();
        script->name = "Script";
        script->source = "-- Luau Script\nprint(\"Hello from Nexus Studio!\")\n";
        newObj = script;
    } else if (className == "Folder") {
        newObj = std::make_shared<Instance>();
        newObj->name = "Folder";
        newObj->customClassName = "Folder";
    } else if (className == "Model") {
        newObj = std::make_shared<Instance>();
        newObj->name = "Model";
        newObj->customClassName = "Model";
    } else if (className == "DirectionalLight") {
        newObj = std::make_shared<Instance>();
        newObj->name = "Directional Light";
        newObj->customClassName = "DirectionalLight";
    } else if (className == "Skybox") {
        newObj = std::make_shared<Instance>();
        newObj->name = "SkyBox";
        newObj->customClassName = "Skybox";
    } else if (className == "ParticleSystem") {
        newObj = std::make_shared<Instance>();
        newObj->name = "ParticleEffect";
        newObj->customClassName = "ParticleSystem";
    } else if (className == "SpringConstraint") {
        newObj = std::make_shared<SpringConstraint>();
        newObj->name = "SpringConstraint";
    } else if (className == "HingeConstraint") {
        newObj = std::make_shared<HingeConstraint>();
        newObj->name = "HingeConstraint";
    } else if (className == "WeldConstraint") {
        newObj = std::make_shared<WeldConstraint>();
        newObj->name = "WeldConstraint";
    } else if (className == "IKControl") {
        newObj = std::make_shared<Engine::IKControl>();
        newObj->name = "IKControl";
    } else {
        newObj = std::make_shared<Instance>();
        newObj->name = className;
        newObj->customClassName = className;
    }

    if (newObj) {
        UndoStack::instance().pushCreateCommand(newObj, parent);
        SelectionManager::instance().select(newObj);
        ExplorerPanel::setGitStatus(newObj.get(), GitStatus::Added);
    }
}

// ─── Duplicate Instance Implementation ───────────────────────────────────────
void ExplorerPanel::duplicateInstance(const std::shared_ptr<Instance>& inst) {
    if (!inst) return;
    auto parent = inst->getParent();
    if (!parent) parent = DataModel::instance();

    std::shared_ptr<Instance> dup;
    if (auto part = std::dynamic_pointer_cast<Part>(inst)) {
        auto newPart = std::make_shared<Part>();
        newPart->name = part->name + "_Copy";
        newPart->customClassName = part->customClassName;
        newPart->setPosition(part->getPosition() + Engine::Math::Vector3(2.0f, 0.0f, 0.0f));
        newPart->setSize(part->getSize());
        newPart->setAnchored(part->getAnchored());
        newPart->setAlbedoColor(part->getAlbedoColor());
        newPart->transparency = part->transparency;
        newPart->metallic = part->metallic;
        newPart->roughness = part->roughness;
        newPart->emissiveStrength = part->emissiveStrength;
        dup = newPart;
    } else if (auto sc = std::dynamic_pointer_cast<Script>(inst)) {
        auto newScript = std::make_shared<Script>();
        newScript->name = sc->name + "_Copy";
        newScript->source = sc->source;
        dup = newScript;
    } else {
        dup = std::make_shared<Instance>();
        dup->name = inst->name + "_Copy";
        dup->customClassName = inst->getClassName();
    }

    if (dup) {
        UndoStack::instance().pushCreateCommand(dup, parent);
        SelectionManager::instance().select(dup);
        ExplorerPanel::setGitStatus(dup.get(), GitStatus::Added);
    }
}
