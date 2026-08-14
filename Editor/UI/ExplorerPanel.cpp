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

// ─── Drag & Drop State (Multi-instance deferred to prevent iterator invalidation) ─
static std::vector<std::shared_ptr<Instance>> s_draggedInstances;
static std::vector<std::shared_ptr<Instance>> s_pendingReparentChildren;
static std::shared_ptr<Instance> s_pendingReparentParent = nullptr;
static std::vector<std::shared_ptr<Instance>> s_insertTargets;

static bool isServiceInstance(const std::shared_ptr<Instance>& inst) {
    if (!inst) return false;
    std::string sCls = inst->getClassName();
    return (sCls == "Workspace" || sCls == "ServerScriptService" || 
            sCls == "ReplicatedStorage" || sCls == "Lighting" || 
            sCls == "SoundService");
}

static bool isAncestorSelected(const std::shared_ptr<Instance>& inst, const std::vector<std::shared_ptr<Instance>>& selection) {
    if (!inst) return false;
    auto curr = inst->getParent();
    while (curr) {
        for (const auto& sel : selection) {
            if (sel && sel == curr) return true;
        }
        curr = curr->getParent();
    }
    return false;
}

static std::shared_ptr<Instance> cloneInstanceInternal(const std::shared_ptr<Instance>& inst) {
    if (!inst) return nullptr;
    std::shared_ptr<Instance> dup;
    if (auto part = std::dynamic_pointer_cast<Part>(inst)) {
        auto newPart = std::make_shared<Part>();
        newPart->name = part->name;
        newPart->customClassName = part->customClassName;
        newPart->setPosition(part->getPosition());
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
        newScript->name = sc->name;
        newScript->source = sc->source;
        dup = newScript;
    } else {
        dup = std::make_shared<Instance>();
        dup->name = inst->name;
        dup->customClassName = inst->getClassName();
    }
    for (const auto& child : inst->getChildren()) {
        auto childDup = cloneInstanceInternal(child);
        if (childDup) {
            childDup->setParent(dup);
        }
    }
    return dup;
}

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
    // 2. Scene objects: MainCamera, SkyBox, DirectionalLight

    if (cls == "Workspace" || name == "Workspace") 
        return { "icon_world_bold", "[W]", false, 0 };
    
    if (cls == "Camera" || name == "Camera" || name == "MainCamera") 
        return { "icon_camera_bold", "[C]", true, COLA(0xFF7700, 1.0f) }; // Orange
    
    if (cls == "Skybox" || name == "Skybox" || name == "SkyBox") 
        return { "icon_sky_bold", "[S]", true, COLA(0x82D9FF, 1.0f) }; // Cyan
    
    if (cls == "DirectionalLight" || name == "Directional Light" || name == "DirectionalLight") 
        return { "icon_light_bold", "[L]", true, COLA(0xBBFF00, 1.0f) }; // LimeYellow
    
    if (cls == "ParticleSystem" || name == "ParticleEffect" || name == "ParticleSystem") 
        return { "icon_fx_bold", "[P]", false, 0 }; // No bar
    
    if (cls == "Script" || name.find(".luau") != std::string::npos || name.find(".cpp") != std::string::npos) 
        return { "icon_script_bold", "[S]", false, 0 }; // No bar
    
    if (name == "Materials" || cls == "Material") 
        return { "icon_folder_bold", "[F]", true, COLA(0xFD71FF, 1.0f) }; // Purple Folder
    
    if (cls == "Folder" || name == "GameModels") 
        return { "icon_folder_bold", "[F]", true, COLA(0xA7FF71, 1.0f) }; // LimeGreen Folder

    if (cls == "Model") 
        return { "icon_model_bold", "[M]", false, 0 }; // Model has its own distinct icon!

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

// ─── Duplicate Incremental Naming Helper ──────────────────────────────────────
static std::string generateDuplicateName(const std::string& baseName, const std::shared_ptr<Instance>& parent) {
    if (!parent) return baseName + "_001";

    // Extract prefix without trailing number (e.g. "Part_001" -> "Part", "Part.002" -> "Part", "Part 001" -> "Part")
    std::string prefix = baseName;
    size_t sep = prefix.find_last_of("._ ");
    if (sep != std::string::npos && sep + 1 < prefix.length()) {
        bool allDigits = true;
        for (size_t i = sep + 1; i < prefix.length(); ++i) {
            if (!isdigit(prefix[i])) { allDigits = false; break; }
        }
        if (allDigits) {
            prefix = prefix.substr(0, sep);
        }
    }

    int maxIndex = 0;
    for (const auto& sibling : parent->getChildren()) {
        if (!sibling) continue;
        const std::string& sName = sibling->name;
        if (sName.rfind(prefix, 0) == 0) {
            size_t sSep = sName.find_last_of("._ ");
            if (sSep != std::string::npos && sSep + 1 < sName.length()) {
                bool allDigits = true;
                for (size_t i = sSep + 1; i < sName.length(); ++i) {
                    if (!isdigit(sName[i])) { allDigits = false; break; }
                }
                if (allDigits) {
                    try {
                        int num = std::stoi(sName.substr(sSep + 1));
                        if (num > maxIndex) maxIndex = num;
                    } catch (...) {}
                }
            }
        }
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%03d", maxIndex + 1);
    return prefix + "_" + buf; // e.g. "Part_001", "Part_002"
}

// ─── Selection Order & Anchor Tracking for Shift+Click Range Selection ────────
static std::vector<std::shared_ptr<Instance>> s_currentVisibleTreeOrder;
static std::vector<std::shared_ptr<Instance>> s_completedVisibleTreeOrder;
static std::shared_ptr<Instance> s_selectionAnchor = nullptr;

// ─── Shift + Drag Box Selection State ─────────────────────────────────────────
static bool s_isBoxSelecting = false;
static ImVec2 s_boxStartPos(0, 0);
static ImVec2 s_boxCurrentPos(0, 0);

struct NodeScreenBounds {
    std::shared_ptr<Instance> inst;
    ImVec2 min;
    ImVec2 max;
};
static std::vector<NodeScreenBounds> s_renderedNodeBounds;

// ─── Main Panel Draw ─────────────────────────────────────────────────────────
void ExplorerPanel::draw() {
    auto& T = NexusTheme::instance();
    
    if (!EditorLayout::instance().showExplorer) return;

    // Reset visible nodes and bounds for this frame
    s_currentVisibleTreeOrder.clear();
    s_renderedNodeBounds.clear();

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_HiddenTabBar;
    ImGui::SetNextWindowClass(&window_class);
    float minW = Editor::UI::CalculateNodeMinTabWidth("Explorer");
    ImGui::SetNextWindowSizeConstraints(ImVec2(minW, 80.0f), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.bgPanel);
    ImGui::Begin("Explorer", &EditorLayout::instance().showExplorer, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
    ImGui::PopStyleColor();

    // Standard Single Tab Header matching other panels
    Editor::UI::DrawSingleTabHeader("Explorer", "icon_explorer_bold", 150.0f, ImGui::ColorConvertFloat4ToU32(T.accent));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float w = ImGui::GetWindowWidth();
    float h = ImGui::GetWindowHeight();

    // ─────────────────────────────────────────────────────────────────────────
    // KEYBOARD SHORTCUTS (F2 Rename, Ctrl+D Duplicate, Del Delete)
    // ─────────────────────────────────────────────────────────────────────────
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        auto selectionList = SelectionManager::instance().getSelectionList();
        if (selectionList.empty() && SelectionManager::instance().getSelected()) {
            selectionList.push_back(SelectionManager::instance().getSelected());
        }

        if (!selectionList.empty()) {
            // F2 -> Inline Rename (for single object) or Batch Rename Modal (for multiple objects)
            if (ImGui::IsKeyPressed(ImGuiKey_F2) && s_renamingInstance == nullptr) {
                std::vector<std::shared_ptr<Instance>> validTargets;
                for (auto& item : selectionList) {
                    if (item && !isServiceInstance(item)) {
                        validTargets.push_back(item);
                    }
                }
                if (validTargets.size() == 1) {
                    s_renamingInstance = validTargets[0].get();
                    strcpy_s(s_renameBuf, sizeof(s_renameBuf), validTargets[0]->name.c_str());
                } else if (validTargets.size() > 1) {
                    m_openBatchRenameModal = true;
                    m_renameMatchBuf[0] = '\0';
                    strcpy_s(m_renameToBuf, sizeof(m_renameToBuf), "$name");
                }
            }
            // Ctrl+D -> Duplicate all selected non-service items (excluding selected children of selected parents)
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
                std::vector<std::shared_ptr<Instance>> topLevelSelection;
                for (auto& item : selectionList) {
                    if (item && !isServiceInstance(item) && !isAncestorSelected(item, selectionList)) {
                        topLevelSelection.push_back(item);
                    }
                }
                std::vector<std::shared_ptr<Instance>> newDups;
                for (auto& item : topLevelSelection) {
                    auto dup = duplicateInstance(item);
                    if (dup) newDups.push_back(dup);
                }
                if (!newDups.empty()) {
                    SelectionManager::instance().clear();
                    for (auto& d : newDups) {
                        SelectionManager::instance().addToSelection(d);
                    }
                }
            }
            // Delete -> Delete all selected non-service items (excluding selected children of selected parents)
            if (ImGui::IsKeyPressed(ImGuiKey_Delete) && s_renamingInstance == nullptr) {
                std::vector<std::shared_ptr<Instance>> topLevelSelection;
                for (auto& item : selectionList) {
                    if (item && !isServiceInstance(item) && !isAncestorSelected(item, selectionList)) {
                        topLevelSelection.push_back(item);
                    }
                }
                for (auto& item : topLevelSelection) {
                    item->destroy();
                }
                SelectionManager::instance().clear();
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // TREE BODY
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.bgDeepest);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 4.0f)); 
    
    ImVec2 bodyMin = ImGui::GetCursorScreenPos();
    ImGui::BeginChild("##ExplBody", ImVec2(w, ImGui::GetContentRegionAvail().y), false, 0);

    for (auto& child : DataModel::instance()->getChildren()) {
        drawInstanceNode(child, 0);
    }

    // Save completed visible tree order
    s_completedVisibleTreeOrder = s_currentVisibleTreeOrder;

    // Safely perform deferred multi-reparenting AFTER tree traversal loop!
    if (!s_pendingReparentChildren.empty() && s_pendingReparentParent) {
        for (auto& child : s_pendingReparentChildren) {
            if (child) child->setParent(s_pendingReparentParent);
        }
        s_treeState[s_pendingReparentParent.get()] = true;
        s_pendingReparentChildren.clear();
        s_pendingReparentParent = nullptr;
    }

    ImVec2 bodyMax = ImVec2(bodyMin.x + w, bodyMin.y + ImGui::GetWindowHeight());

    // ─────────────────────────────────────────────────────────────────────────
    // SHIFT + DRAG BOX (MARQUEE) SELECTION
    // ─────────────────────────────────────────────────────────────────────────
    bool isShiftHeld = ImGui::GetIO().KeyShift;
    ImVec2 mousePos = ImGui::GetMousePos();
    bool mouseInBody = (mousePos.x >= bodyMin.x && mousePos.x <= bodyMax.x && mousePos.y >= bodyMin.y && mousePos.y <= bodyMax.y);

    if (isShiftHeld && ImGui::IsMouseClicked(0) && mouseInBody) {
        s_isBoxSelecting = true;
        s_boxStartPos = mousePos;
        s_boxCurrentPos = mousePos;
    }

    if (s_isBoxSelecting) {
        if (ImGui::IsMouseDown(0) && isShiftHeld) {
            s_boxCurrentPos = mousePos;
            ImVec2 bMin(std::min(s_boxStartPos.x, s_boxCurrentPos.x), std::min(s_boxStartPos.y, s_boxCurrentPos.y));
            ImVec2 bMax(std::max(s_boxStartPos.x, s_boxCurrentPos.x), std::max(s_boxStartPos.y, s_boxCurrentPos.y));

            float dragDist = std::sqrt((s_boxCurrentPos.x - s_boxStartPos.x) * (s_boxCurrentPos.x - s_boxStartPos.x) + 
                                       (s_boxCurrentPos.y - s_boxStartPos.y) * (s_boxCurrentPos.y - s_boxStartPos.y));

            if (dragDist > 4.0f) {
                SelectionManager::instance().clear();
                for (const auto& nb : s_renderedNodeBounds) {
                    if (nb.min.y <= bMax.y && nb.max.y >= bMin.y) {
                        SelectionManager::instance().addToSelection(nb.inst);
                    }
                }

                // Draw Marquee Box Overlay
                dl->AddRectFilled(bMin, bMax, COLA(0x82D9FF, 0.15f), 2.0f);
                dl->AddRect(bMin, bMax, COL(T.accent), 2.0f, 0, 1.5f);
            }
        } else {
            s_isBoxSelecting = false;
        }
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
        m_insertSelectedIndex = 0;
    }

    drawInsertObjectPopup();
    drawBatchRenameModal();

    // Panel Outer Borders
    dl->AddLine(ImVec2(p.x + w - 1.0f, p.y), ImVec2(p.x + w - 1.0f, p.y + h), COL(T.border));
    dl->AddLine(ImVec2(p.x, p.y + h - 1.0f), ImVec2(p.x + w, p.y + h - 1.0f), COL(T.border));

    ImGui::End();
    ImGui::PopStyleVar();
}

// ─── Draw Node (With Guide Lines, Multi-Selection, Drag & Drop, Inline Rename) ─
void ExplorerPanel::drawInstanceNode(const std::shared_ptr<Instance>& inst, int depth) {
    if (!inst) return;

    // Track visible hierarchy order
    s_currentVisibleTreeOrder.push_back(inst);

    auto& T = NexusTheme::instance();
    bool isSelected = SelectionManager::instance().isSelected(inst);
    std::string cls = inst->getClassName();
    std::string name = inst->name;
    bool isService = (cls == "Workspace" || cls == "ServerScriptService" || cls == "ReplicatedStorage" || cls == "Lighting" || cls == "SoundService");
    
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

    // ─── Selection Interaction (Single / Ctrl / Shift Range Selection) ────────
    if (ImGui::IsItemClicked(0)) {
        if (ImGui::GetIO().KeyCtrl) {
            SelectionManager::instance().toggleSelect(inst);
            s_selectionAnchor = inst;
        } else if (ImGui::GetIO().KeyShift) {
            if (!s_selectionAnchor) {
                s_selectionAnchor = SelectionManager::instance().getSelected();
                if (!s_selectionAnchor) s_selectionAnchor = inst;
            }

            const auto& treeOrder = s_completedVisibleTreeOrder.empty() ? s_currentVisibleTreeOrder : s_completedVisibleTreeOrder;

            int idxA = -1, idxB = -1;
            for (int i = 0; i < (int)treeOrder.size(); ++i) {
                if (treeOrder[i] == s_selectionAnchor) idxA = i;
                if (treeOrder[i] == inst) idxB = i;
            }

            if (idxA != -1 && idxB != -1) {
                int start = std::min(idxA, idxB);
                int end = std::max(idxA, idxB);
                SelectionManager::instance().clear();
                for (int i = start; i <= end; ++i) {
                    SelectionManager::instance().addToSelection(treeOrder[i]);
                }
            } else {
                SelectionManager::instance().select(inst);
                s_selectionAnchor = inst;
            }
        } else {
            // Normal click: if not already selected, select immediately
            if (!SelectionManager::instance().isSelected(inst)) {
                SelectionManager::instance().select(inst);
                s_selectionAnchor = inst;
            }
            // If already part of multi-selection, preserve selection for drag-and-drop!
        }
    }

    // If mouse was released on an already-selected item without dragging, reduce selection to single item
    if (hovered && ImGui::IsMouseReleased(0) && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift) {
        if (SelectionManager::instance().getSelectionList().size() > 1 && SelectionManager::instance().isSelected(inst)) {
            ImVec2 dragDelta = ImGui::GetMouseDragDelta(0);
            if (std::abs(dragDelta.x) < 3.0f && std::abs(dragDelta.y) < 3.0f) {
                SelectionManager::instance().select(inst);
                s_selectionAnchor = inst;
            }
        }
    }

    // ─── Right Click (Context Menu on node) ──────────────────────────────────
    if (ImGui::BeginPopupContextItem("NodeContextMenu")) {
        if (!SelectionManager::instance().isSelected(inst)) {
            SelectionManager::instance().select(inst);
        }

        auto selectionList = SelectionManager::instance().getSelectionList();
        if (selectionList.empty()) selectionList.push_back(inst);

        if (ImGui::MenuItem("+ Insert Object...")) {
            s_insertTargets = selectionList;
            if (s_insertTargets.empty()) s_insertTargets.push_back(inst);
            m_insertTarget = inst;
            m_openInsertPopup = true;
            m_insertSelectedIndex = 0;
        }
        
        bool hasNonService = false;
        for (auto& item : selectionList) {
            if (!isServiceInstance(item)) { hasNonService = true; break; }
        }

        if (hasNonService) {
            ImGui::Separator();
            if (ImGui::MenuItem("Rename", "F2")) {
                std::vector<std::shared_ptr<Instance>> validTargets;
                for (auto& item : selectionList) {
                    if (item && !isServiceInstance(item)) {
                        validTargets.push_back(item);
                    }
                }
                if (validTargets.size() == 1) {
                    s_renamingInstance = validTargets[0].get();
                    strcpy_s(s_renameBuf, sizeof(s_renameBuf), validTargets[0]->name.c_str());
                } else if (validTargets.size() > 1) {
                    m_openBatchRenameModal = true;
                    m_renameMatchBuf[0] = '\0';
                    strcpy_s(m_renameToBuf, sizeof(m_renameToBuf), "$name");
                }
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
                std::vector<std::shared_ptr<Instance>> topLevelSelection;
                for (auto& item : selectionList) {
                    if (item && !isServiceInstance(item) && !isAncestorSelected(item, selectionList)) {
                        topLevelSelection.push_back(item);
                    }
                }
                std::vector<std::shared_ptr<Instance>> newDups;
                for (auto& item : topLevelSelection) {
                    auto dup = duplicateInstance(item);
                    if (dup) newDups.push_back(dup);
                }
                if (!newDups.empty()) {
                    SelectionManager::instance().clear();
                    for (auto& d : newDups) {
                        SelectionManager::instance().addToSelection(d);
                    }
                }
            }
            if (ImGui::MenuItem("Delete", "Del")) {
                std::vector<std::shared_ptr<Instance>> topLevelSelection;
                for (auto& item : selectionList) {
                    if (item && !isServiceInstance(item) && !isAncestorSelected(item, selectionList)) {
                        topLevelSelection.push_back(item);
                    }
                }
                for (auto& item : topLevelSelection) {
                    item->destroy();
                }
                SelectionManager::instance().clear();
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Expand All")) {
            auto expandAll = [](auto& self, const std::shared_ptr<Instance>& node) -> void {
                s_treeState[node.get()] = true;
                for (auto& c : node->getChildren()) self(self, c);
            };
            for (auto& item : selectionList) {
                if (item) expandAll(expandAll, item);
            }
        }
        if (ImGui::MenuItem("Collapse All")) {
            auto collapseAll = [](auto& self, const std::shared_ptr<Instance>& node) -> void {
                s_treeState[node.get()] = false;
                for (auto& c : node->getChildren()) self(self, c);
            };
            for (auto& item : selectionList) {
                if (item) collapseAll(collapseAll, item);
            }
        }
        ImGui::EndPopup();
    }

    // ─── Double Click (Rename or Expand) ──────────────────────────────────────
    if (hovered && ImGui::IsMouseDoubleClicked(0)) {
        float textStartX = winX + 10.0f + depth * 18.0f + 38.0f;
        if (ImGui::GetMousePos().x >= textStartX && !isService) {
            // Inline Rename (Services are protected)
            s_renamingInstance = inst.get();
            strcpy_s(s_renameBuf, inst->name.c_str());
        } else if (hasChildren) {
            open = !open;
        }
    }

    // Record screen bounds for marquee box selection
    s_renderedNodeBounds.push_back({ inst, rowMin, rowMax });

    // ─── Drag & Drop Source (Services cannot be moved, and disabled during Shift+Drag box select) ─
    if (!isService && !s_isBoxSelecting && !ImGui::GetIO().KeyShift && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        s_draggedInstances.clear();
        if (SelectionManager::instance().isSelected(inst)) {
            for (const auto& sel : SelectionManager::instance().getSelectionList()) {
                if (!sel) continue;
                std::string sCls = sel->getClassName();
                bool isServ = (sCls == "Workspace" || sCls == "ServerScriptService" || 
                               sCls == "ReplicatedStorage" || sCls == "Lighting" || 
                               sCls == "SoundService");
                if (!isServ) {
                    s_draggedInstances.push_back(sel);
                }
            }
        }
        if (s_draggedInstances.empty()) {
            s_draggedInstances.push_back(inst);
        }

        Instance* rawPtr = inst.get();
        ImGui::SetDragDropPayload("EXPLORER_INSTANCE", &rawPtr, sizeof(Instance*));
        if (s_draggedInstances.size() == 1) {
            ImGui::Text("Moving: %s", s_draggedInstances[0]->name.c_str());
        } else {
            ImGui::Text("Moving %zu items", s_draggedInstances.size());
        }
        ImGui::EndDragDropSource();
    }

    // ─── Drag & Drop Target (Re-parenting with deferral for all selected instances) ─
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EXPLORER_INSTANCE")) {
            auto checkDescendant = [](auto& self, const std::shared_ptr<Instance>& parent, const std::shared_ptr<Instance>& target) -> bool {
                for (auto& c : parent->getChildren()) {
                    if (c == target || self(self, c, target)) return true;
                }
                return false;
            };

            for (const auto& dragged : s_draggedInstances) {
                if (dragged && dragged != inst && !checkDescendant(checkDescendant, dragged, inst)) {
                    s_pendingReparentChildren.push_back(dragged);
                }
            }
            s_pendingReparentParent = inst;
        }
        ImGui::EndDragDropTarget();
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
    // 3. TYPE BADGE (3x10px Colored Bar - Only for Folders, Camera, Skybox, Light)
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
    // 5. LABEL & GIT STATUS (Name color does NOT change, badges are bold)
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
            std::string newN = s_renameBuf;
            if (!newN.empty()) {
                auto selectionList = SelectionManager::instance().getSelectionList();
                if (selectionList.empty()) selectionList.push_back(inst);
                for (auto& item : selectionList) {
                    if (!isServiceInstance(item)) {
                        std::string oldN = item->name;
                        item->name = newN;
                        UndoStack::instance().pushPropertyChangeCommand(item, "Name", oldN, newN);
                    }
                }
            }
            s_renamingInstance = nullptr;
        }
        if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit()) {
            s_renamingInstance = nullptr;
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    } else {
        // Render name (Bold for root services)
        dl->AddText(ImVec2(curX, pos.y + 3.0f), textCol, name.c_str());
        if (isService) {
            dl->AddText(ImVec2(curX + 0.5f, pos.y + 3.0f), textCol, name.c_str()); // Simulated bold for services
        }

        // Bold Colored Git Badge Beside Name
        if (git == GitStatus::Modified) {
            ImVec2 nSz = ImGui::CalcTextSize(name.c_str());
            ImVec2 bPos(curX + nSz.x + 8.0f, pos.y + 3.0f);
            dl->AddText(bPos, COL(T.accentYellow), "M");
            dl->AddText(ImVec2(bPos.x + 0.5f, bPos.y), COL(T.accentYellow), "M");
        } else if (git == GitStatus::Added) {
            ImVec2 nSz = ImGui::CalcTextSize(name.c_str());
            ImVec2 bPos(curX + nSz.x + 8.0f, pos.y + 3.0f);
            dl->AddText(bPos, COL(T.accentGreen), "U");
            dl->AddText(ImVec2(bPos.x + 0.5f, bPos.y), COL(T.accentGreen), "U");
        } else if (git == GitStatus::Deleted) {
            ImVec2 nSz = ImGui::CalcTextSize(name.c_str());
            ImVec2 bPos(curX + nSz.x + 8.0f, pos.y + 3.0f);
            dl->AddText(bPos, COL(T.accentRed), "D");
            dl->AddText(ImVec2(bPos.x + 0.5f, bPos.y), COL(T.accentRed), "D");
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

        // Direct click detection without submitting extra ImGui layout widgets
        if (ImGui::IsMouseClicked(0)) {
            ImVec2 mpos = ImGui::GetMousePos();
            if (mpos.x >= plusX && mpos.x <= plusX + plusW && mpos.y >= plusY && mpos.y <= plusY + plusH) {
                s_insertTargets.clear();
                s_insertTargets.push_back(inst);
                m_insertTarget = inst;
                m_openInsertPopup = true;
                m_insertSelectedIndex = 0;
            }
        }
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

// ─── Insert Object Popup Panel (Direct Search Bar & Keyboard Navigation + Auto Scroll) ─
static int* s_pInsertIndex = nullptr;
static int s_insertItemCount = 0;
static bool s_insertNavScrolled = false;

static int InsertSearchCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventKey == ImGuiKey_DownArrow && s_pInsertIndex && s_insertItemCount > 0) {
        *s_pInsertIndex = (*s_pInsertIndex + 1) % s_insertItemCount;
        s_insertNavScrolled = true;
    } else if (data->EventKey == ImGuiKey_UpArrow && s_pInsertIndex && s_insertItemCount > 0) {
        *s_pInsertIndex = (*s_pInsertIndex - 1 + s_insertItemCount) % s_insertItemCount;
        s_insertNavScrolled = true;
    }
    return 0;
}

void ExplorerPanel::drawInsertObjectPopup() {
    auto& T = NexusTheme::instance();

    ImGui::SetNextWindowSize(ImVec2(290.0f, 360.0f), ImGuiCond_Appearing);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, T.bgPanel);
    ImGui::PushStyleColor(ImGuiCol_Border, T.border);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);

    if (ImGui::BeginPopup("InsertObjectPopup", ImGuiWindowFlags_NoNavInputs)) {
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

        // Filter items
        std::string searchLower = m_insertSearch;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

        std::vector<const InsertableItem*> matchingItems;
        for (const auto& item : s_items) {
            std::string nameLower = item.className;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

            if (searchLower.empty() || nameLower.find(searchLower) != std::string::npos || 
                item.category.find(searchLower) != std::string::npos) {
                matchingItems.push_back(&item);
            }
        }

        // Clamp selection index
        if (matchingItems.empty()) {
            m_insertSelectedIndex = 0;
        } else {
            if (m_insertSelectedIndex >= (int)matchingItems.size()) m_insertSelectedIndex = (int)matchingItems.size() - 1;
            if (m_insertSelectedIndex < 0) m_insertSelectedIndex = 0;
        }

        s_pInsertIndex = &m_insertSelectedIndex;
        s_insertItemCount = (int)matchingItems.size();
        s_insertNavScrolled = false;

        // Enter key inserts selected object
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
            if (!matchingItems.empty() && m_insertSelectedIndex >= 0 && m_insertSelectedIndex < (int)matchingItems.size()) {
                std::vector<std::shared_ptr<Instance>> targets = s_insertTargets;
                if (targets.empty()) {
                    if (m_insertTarget) {
                        targets.push_back(m_insertTarget);
                    } else if (auto sel = SelectionManager::instance().getSelected()) {
                        targets.push_back(sel);
                    } else {
                        auto ws = DataModel::instance()->findFirstChild("Workspace");
                        if (ws) targets.push_back(ws);
                    }
                }
                for (auto& target : targets) {
                    createAndInsertObject(matchingItems[m_insertSelectedIndex]->className, target);
                }
                s_insertTargets.clear();
                m_insertTarget = nullptr;
                ImGui::CloseCurrentPopup();
            }
        }

        // Search Bar (Directly at top without extra "Insert Object into" bar)
        ImGui::PushStyleColor(ImGuiCol_FrameBg, T.bgDeepest);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        
        ImGuiInputTextFlags searchFlags = ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_NoHorizontalScroll;
        ImGui::InputTextWithHint("##InsertSearch", "Search objects...", m_insertSearch, sizeof(m_insertSearch), searchFlags, InsertSearchCallback);
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere(-1);
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Categorized List with Auto Scroll & NoNav Item Flag
        std::string lastCat = "";
        bool mouseMoved = (std::abs(ImGui::GetIO().MouseDelta.x) > 0.5f || std::abs(ImGui::GetIO().MouseDelta.y) > 0.5f);

        ImGui::BeginChild("##InsertList", ImVec2(0, 0), false, ImGuiWindowFlags_NoNav);
        ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true); // Keep keyboard navigation focus locked in Search Box!

        for (size_t idx = 0; idx < matchingItems.size(); idx++) {
            const auto* item = matchingItems[idx];
            bool isCurrentSelected = ((int)idx == m_insertSelectedIndex);

            if (item->category != lastCat) {
                lastCat = item->category;
                ImGui::Spacing();
                ImGui::TextColored(T.textSecondary, "%s", lastCat.c_str());
                ImGui::Separator();
            }

            ImGui::PushID(item->className.c_str());
            ImVec2 pMin = ImGui::GetCursorScreenPos();
            float w = ImGui::GetContentRegionAvail().x;
            float rowH = 32.0f;

            if (ImGui::InvisibleButton("##itemBtn", ImVec2(w, rowH))) {
                std::vector<std::shared_ptr<Instance>> targets = s_insertTargets;
                if (targets.empty()) {
                    if (m_insertTarget) {
                        targets.push_back(m_insertTarget);
                    } else if (auto sel = SelectionManager::instance().getSelected()) {
                        targets.push_back(sel);
                    } else {
                        auto ws = DataModel::instance()->findFirstChild("Workspace");
                        if (ws) targets.push_back(ws);
                    }
                }
                for (auto& target : targets) {
                    createAndInsertObject(item->className, target);
                }
                s_insertTargets.clear();
                m_insertTarget = nullptr;
                ImGui::CloseCurrentPopup();
            }
            bool hovered = ImGui::IsItemHovered();
            if (hovered && mouseMoved) {
                m_insertSelectedIndex = (int)idx;
            }

            // Auto-scroll when navigated via keyboard
            if (isCurrentSelected && s_insertNavScrolled) {
                ImGui::SetScrollHereY(0.5f);
            }

            ImDrawList* cDl = ImGui::GetWindowDrawList();
            if (isCurrentSelected || hovered) {
                cDl->AddRectFilled(pMin, ImVec2(pMin.x + w, pMin.y + rowH), COL(T.panelHover), 4.0f);
                cDl->AddRect(pMin, ImVec2(pMin.x + w, pMin.y + rowH), COL(T.accent), 4.0f);
            }

            // Icon
            ImTextureID iconTex = IconRegistry::instance().get(item->iconKey.c_str());
            if (iconTex) {
                cDl->AddImage(iconTex, ImVec2(pMin.x + 6.0f, pMin.y + 6.0f), ImVec2(pMin.x + 24.0f, pMin.y + 24.0f));
            }

            // Class Name
            cDl->AddText(ImVec2(pMin.x + 30.0f, pMin.y + 3.0f), IM_COL32_WHITE, item->className.c_str());

            // Description
            cDl->AddText(ImVec2(pMin.x + 30.0f, pMin.y + 17.0f), COL(T.textMuted), item->description.c_str());

            ImGui::PopID();
        }

        ImGui::PopItemFlag();
        ImGui::EndChild();
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// ─── Create & Insert Instance Factory ─────────────────────────────────────────
void ExplorerPanel::createAndInsertObject(const std::string& className, const std::shared_ptr<Instance>& parent) {
    auto target = parent;
    if (!target || target == DataModel::instance()) {
        target = DataModel::instance()->findFirstChild("Workspace");
    }
    if (!target) return;

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
        UndoStack::instance().pushCreateCommand(newObj, target);
        SelectionManager::instance().select(newObj);
        ExplorerPanel::setGitStatus(newObj.get(), GitStatus::Added);
        s_treeState[target.get()] = true; // Auto-expand parent node so new child is visible
    }
}

// ─── Duplicate Instance Implementation ───────────────────────────────────────
std::shared_ptr<Instance> ExplorerPanel::duplicateInstance(const std::shared_ptr<Instance>& inst) {
    if (!inst) return nullptr;
    auto parent = inst->getParent();
    if (!parent) parent = DataModel::instance()->findFirstChild("Workspace");
    if (!parent) return nullptr;

    // Services cannot be duplicated
    if (isServiceInstance(inst)) {
        return nullptr;
    }

    std::string newName = generateDuplicateName(inst->name, parent);
    std::shared_ptr<Instance> dup = cloneInstanceInternal(inst);
    if (!dup) return nullptr;

    dup->name = newName;
    if (auto part = std::dynamic_pointer_cast<Part>(dup)) {
        part->setPosition(part->getPosition() + Engine::Math::Vector3(2.0f, 0.0f, 0.0f));
    }

    UndoStack::instance().pushCreateCommand(dup, parent);
    ExplorerPanel::setGitStatus(dup.get(), GitStatus::Added);
    return dup;
}

// ─── Batch Rename Implementation (Figma-Style) ────────────────────────────────
std::string ExplorerPanel::computeRenamedString(const std::string& origName, int index, int totalCount, 
                                                const std::string& matchPattern, const std::string& renameTemplate,
                                                int startNum) {
    if (renameTemplate.empty() && matchPattern.empty()) return origName;

    std::string result = renameTemplate.empty() ? origName : renameTemplate;

    // If matchPattern is provided and non-empty:
    if (!matchPattern.empty()) {
        std::string base = origName;
        size_t pos = 0;
        while ((pos = base.find(matchPattern, pos)) != std::string::npos) {
            base.replace(pos, matchPattern.length(), renameTemplate);
            pos += renameTemplate.length();
        }
        result = base;
    }

    // Replace token $name with origName
    size_t namePos = 0;
    while ((namePos = result.find("$name", namePos)) != std::string::npos) {
        result.replace(namePos, 5, origName);
        namePos += origName.length();
    }

    int currentAscending = startNum + index;
    int currentDescending = (totalCount - 1 - index) + startNum;

    // Number tokens:
    // $nnn -> 001, 002...
    size_t posN3 = 0;
    while ((posN3 = result.find("$nnn", posN3)) != std::string::npos) {
        char nBuf[32];
        snprintf(nBuf, sizeof(nBuf), "%03d", currentAscending);
        result.replace(posN3, 4, nBuf);
        posN3 += strlen(nBuf);
    }

    // $nn -> 01, 02...
    size_t posN2 = 0;
    while ((posN2 = result.find("$nn", posN2)) != std::string::npos) {
        char nBuf[32];
        snprintf(nBuf, sizeof(nBuf), "%02d", currentAscending);
        result.replace(posN2, 3, nBuf);
        posN2 += strlen(nBuf);
    }

    // $n -> 1, 2...
    size_t posN = 0;
    while ((posN = result.find("$n", posN)) != std::string::npos) {
        char nBuf[32];
        snprintf(nBuf, sizeof(nBuf), "%d", currentAscending);
        result.replace(posN, 2, nBuf);
        posN += strlen(nBuf);
    }

    // $d or $N -> Descending number
    size_t posD = 0;
    while ((posD = result.find("$d", posD)) != std::string::npos) {
        char nBuf[32];
        snprintf(nBuf, sizeof(nBuf), "%d", currentDescending);
        result.replace(posD, 2, nBuf);
        posD += strlen(nBuf);
    }

    size_t posCapN = 0;
    while ((posCapN = result.find("$N", posCapN)) != std::string::npos) {
        char nBuf[32];
        snprintf(nBuf, sizeof(nBuf), "%d", currentDescending);
        result.replace(posCapN, 2, nBuf);
        posCapN += strlen(nBuf);
    }

    return result;
}

void ExplorerPanel::drawBatchRenameModal() {
    auto& T = NexusTheme::instance();

    if (m_openBatchRenameModal) {
        ImGui::OpenPopup("Rename Objects");
        m_openBatchRenameModal = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.0f, 260.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, T.bgPanel);
    ImGui::PushStyleColor(ImGuiCol_Border, T.border);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    if (ImGui::BeginPopupModal("Rename Objects", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        auto selectionList = SelectionManager::instance().getSelectionList();
        if (selectionList.empty() && SelectionManager::instance().getSelected()) {
            selectionList.push_back(SelectionManager::instance().getSelected());
        }

        std::vector<std::shared_ptr<Instance>> validTargets;
        for (auto& item : selectionList) {
            if (item && !isServiceInstance(item)) {
                validTargets.push_back(item);
            }
        }

        if (validTargets.size() <= 1) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(3);
            return;
        }

        float fullW = ImGui::GetContentRegionAvail().x;

        // ─── 1. HEADER BAR ───────────────────────────────────────────────────
        char titleText[64];
        if (validTargets.size() == 1) {
            snprintf(titleText, sizeof(titleText), "Rename 1 layer");
        } else {
            snprintf(titleText, sizeof(titleText), "Rename %zu layers", validTargets.size());
        }
        ImGui::TextColored(T.textPrimary, "%s", titleText);

        // Top-right close 'x'
        ImGui::SameLine(fullW - 14.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, T.textPrimary);
        if (ImGui::Selectable("X##CloseRename", false, 0, ImVec2(18.0f, 18.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, T.border);
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        float leftW = 120.0f;
        float gap = 12.0f;
        float rightW = fullW - leftW - gap;

        // ─── 2. LEFT COLUMN: PREVIEW LIST ────────────────────────────────────
        ImGui::BeginGroup();
        ImGui::TextColored(T.textPrimary, "Preview");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, T.border);
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::BeginChild("##RenamePreviewList", ImVec2(leftW, 125.0f), false, ImGuiWindowFlags_None);

        for (size_t i = 0; i < validTargets.size(); ++i) {
            auto& inst = validTargets[i];
            std::string newName = computeRenamedString(inst->name, (int)i, (int)validTargets.size(), m_renameMatchBuf, m_renameToBuf, m_startAscendingFrom);

            ImGui::TextColored(T.textPrimary, "%s", newName.c_str());
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::EndGroup();

        ImGui::SameLine(0, gap);

        // ─── 3. RIGHT COLUMN: INPUTS & CONTROLS ──────────────────────────────
        ImGui::BeginGroup();

        bool submit = false;

        ImGui::PushStyleColor(ImGuiCol_FrameBg, T.bgDeepest);
        ImGui::PushStyleColor(ImGuiCol_Border, T.border);
        ImGui::PushStyleColor(ImGuiCol_Text, T.textPrimary);
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.70f, 0.70f, 0.70f, 1.0f)); // Bright crisp hint
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));

        // Match Input
        ImGui::SetNextItemWidth(rightW);
        if (ImGui::InputTextWithHint("##MatchInput", "Match (optional)", m_renameMatchBuf, sizeof(m_renameMatchBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            submit = true;
        }

        ImGui::Spacing();

        // Rename To Input
        ImGui::SetNextItemWidth(rightW);
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere(0);
        }
        if (ImGui::InputTextWithHint("##RenameToInput", "Rename to", m_renameToBuf, sizeof(m_renameToBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            submit = true;
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(4);

        ImGui::Spacing();

        // Token Pills (Current name, Number ↑, Number ↓)
        ImGui::PushStyleColor(ImGuiCol_Button, T.bgCard);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, T.panelHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, T.bgDeepest);
        ImGui::PushStyleColor(ImGuiCol_Border, T.border);
        ImGui::PushStyleColor(ImGuiCol_Text, T.textPrimary);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 4.0f));

        if (ImGui::Button("Current name")) {
            strcat_s(m_renameToBuf, sizeof(m_renameToBuf), "$name");
        }
        ImGui::SameLine(0, 4.0f);
        if (ImGui::Button("Number \xe2\x86\x91")) { // Number ↑
            strcat_s(m_renameToBuf, sizeof(m_renameToBuf), " $n");
        }
        ImGui::SameLine(0, 4.0f);
        if (ImGui::Button("Number \xe2\x86\x93")) { // Number ↓
            strcat_s(m_renameToBuf, sizeof(m_renameToBuf), " $d");
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);

        ImGui::Spacing();

        // Start ascending from
        ImGui::TextColored(T.textPrimary, "Start ascending from");
        ImGui::SameLine(0, 6.0f);
        
        ImGui::PushStyleColor(ImGuiCol_FrameBg, T.bgDeepest);
        ImGui::PushStyleColor(ImGuiCol_Border, T.border);
        ImGui::PushStyleColor(ImGuiCol_Text, T.textPrimary);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 2.0f));

        ImGui::SetNextItemWidth(40.0f);
        ImGui::InputInt("##StartAscending", &m_startAscendingFrom, 0, 0);
        if (m_startAscendingFrom < 0) m_startAscendingFrom = 0;

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(3);

        ImGui::EndGroup();

        // ─── 4. FOOTER BAR ───────────────────────────────────────────────────
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, T.border);
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // "Learn more" link on left
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1.0f);
        ImGui::TextColored(T.accent, "Learn more");

        // Action buttons on right
        float btnH = 26.0f;
        ImGui::SameLine(fullW - 138.0f);

        // Cancel button
        ImGui::PushStyleColor(ImGuiCol_Button, T.bgCard);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, T.panelHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, T.bgDeepest);
        ImGui::PushStyleColor(ImGuiCol_Border, T.border);
        ImGui::PushStyleColor(ImGuiCol_Text, T.textPrimary);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        if (ImGui::Button("Cancel", ImVec2(62.0f, btnH)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);

        ImGui::SameLine(0, 6.0f);

        // Rename button (NexusTheme accent)
        ImGui::PushStyleColor(ImGuiCol_Button, T.accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, NexusTheme::HexColor(0x9BE0FF));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, NexusTheme::HexColor(0x6CC6FA));
        ImGui::PushStyleColor(ImGuiCol_Text, NexusTheme::HexColor(0x000000));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        if (ImGui::Button("Rename", ImVec2(70.0f, btnH)) || submit) {
            for (size_t i = 0; i < validTargets.size(); ++i) {
                auto& inst = validTargets[i];
                std::string oldN = inst->name;
                std::string newN = computeRenamedString(oldN, (int)i, (int)validTargets.size(), m_renameMatchBuf, m_renameToBuf, m_startAscendingFrom);
                if (!newN.empty()) {
                    inst->name = newN;
                    UndoStack::instance().pushPropertyChangeCommand(inst, "Name", oldN, newN);
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(4);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(3);
}
