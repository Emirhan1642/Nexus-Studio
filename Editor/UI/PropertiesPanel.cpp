#include "PropertiesPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "SelectionManager.h"
#include "../Undo/UndoStack.h"
#include "Engine/Core/Reflection/TypeRegistry.h"
#include "Engine/Core/DataModel/Instance.h"
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Core/Math/Vector3.h"
#include "IconRegistry.h"
#include "NexusTheme.h"
#include "EditorLayout.h"
#include "SharedTabBar.h"
#include "ImGuiLayer.h"
#include "Asset/MeshAssetExporter.h"
#include "Engine/Core/Geometry/UVUnwrapper.h"
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <stdio.h>

static ImU32 COL(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a) {
    return IM_COL32((hex>>16)&0xFF,(hex>>8)&0xFF,hex&0xFF,(uint8_t)(a*255));
}

// ─── Aligned text helper ────────────────────────────────────────────────────
static void DrawTextRightAligned(ImDrawList* dl, ImVec2 pos, float width, ImU32 col, const char* text) {
    ImVec2 size = ImGui::CalcTextSize(text);
    dl->AddText(ImVec2(pos.x + width - size.x, pos.y), col, text);
}

// ─── Class Icon Resolver ────────────────────────────────────────────────────
static const char* getClassIcon(const std::string& cls, const std::string& name) {
    if (cls == "Workspace" || name == "Workspace") return "icon_world_bold";
    if (cls == "Part") return "icon_mesh";
    if (cls == "MeshPart") return "icon_mesh_bold";
    if (cls == "Script") return "icon_script_bold";
    if (cls == "Camera") return "icon_camera_bold";
    if (cls == "DirectionalLight") return "icon_light_bold";
    if (cls == "Skybox") return "icon_sky_bold";
    if (cls == "Model") return "icon_model_bold";
    if (cls == "Bone") return "icon_transform";
    if (cls == "Folder") return "icon_folder_bold";
    if (cls == "ParticleSystem") return "icon_fx_bold";
    if (cls == "Lighting" || name == "Lighting") return "icon_light2_bold";
    if (cls == "ServerScriptService" || name == "ServerScriptService") return "icon_svr_bold";
    if (cls == "SoundService" || name == "SoundService") return "icon_snd_bold";
    if (cls == "ReplicatedStorage" || name == "ReplicatedStorage") return "icon_box";
    if (cls == "Humanoid") return "icon_model_bold";
    if (cls == "IKControl") return "icon_transform";
    return "icon_box";
}

// ─── Toggle Pill (With Mixed State Support) ──────────────────────────────────
static bool DrawTogglePill(const char* id, bool value, bool isMixed = false) {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = 28.0f, h = 14.0f;
    ImVec2 p = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton(id, ImVec2(w, h));
    bool clicked = ImGui::IsItemClicked();

    ImU32 bg;
    if (isMixed) {
        bg = IM_COL32(65, 65, 65, 255);
    } else {
        bg = value ? COL(T.accentGreen) : IM_COL32(36, 36, 36, 255);
    }
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, h * 0.5f);

    float cx;
    if (isMixed) {
        cx = p.x + w * 0.5f;
    } else {
        cx = value ? p.x + w - h * 0.5f : p.x + h * 0.5f;
    }
    dl->AddCircleFilled(ImVec2(cx, p.y + h * 0.5f), h * 0.5f - 2.0f, IM_COL32_WHITE);

    return clicked;
}

// ─── Section Header (Spacious, elegant with padded divider) ───────────────────
static bool SectionHeader(const char* label, const char* iconKey = nullptr, const char* rightLabel = nullptr, bool defaultOpen = true) {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float width = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float headerH = 28.0f;

    static std::map<std::string, bool> s_openState;
    if (s_openState.find(label) == s_openState.end()) {
        s_openState[label] = defaultOpen;
    }
    bool& open = s_openState[label];

    ImGui::InvisibleButton(label, ImVec2(width, headerH));
    if (ImGui::IsItemClicked()) open = !open;

    // Divider Line with side padding and increased breathing room
    float divMargin = 6.0f;
    float divY = p.y + headerH - 1.0f;
    dl->AddLine(ImVec2(p.x + divMargin, divY), ImVec2(p.x + width - divMargin, divY), COLA(0xFFFFFF, 0.12f));

    // Arrow + Icon + Label
    ImTextureID chevron = IconRegistry::instance().get(open ? "icon_chevron_down_bold" : "icon_chevron_right_bold");
    if (!chevron) chevron = IconRegistry::instance().get(open ? "icon_chevron_down" : "icon_chevron_right");
    if (chevron) {
        dl->AddImage(chevron, ImVec2(p.x + 2.0f, p.y + 5.0f), ImVec2(p.x + 18.0f, p.y + 21.0f), ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE);
    } else {
        const char* arrow = open ? "v" : ">";
        dl->AddText(ImVec2(p.x + 2.0f, p.y + 5.0f), IM_COL32_WHITE, arrow);
    }

    float cx = p.x + 22.0f;
    if (iconKey) {
        ImTextureID iconTex = IconRegistry::instance().get(iconKey);
        if (iconTex) {
            dl->AddImage(iconTex, ImVec2(cx, p.y + 5.0f), ImVec2(cx + 16.0f, p.y + 21.0f));
            cx += 22.0f;
        }
    }

    dl->AddText(ImVec2(cx, p.y + 5.0f), IM_COL32_WHITE, label); 

    if (rightLabel) {
        ImVec2 size = ImGui::CalcTextSize(rightLabel);
        dl->AddText(ImVec2(p.x + width - size.x - 8.0f, p.y + 5.0f), COL(T.textMuted), rightLabel);
    }

    return open;
}

// ─── Simple Property Row (Spacious & Clean) ──────────────────────────────────
static void PropRow(const char* lbl, const char* val, ImU32 valCol = 0) {
    auto& T = NexusTheme::instance();
    if (valCol == 0) valCol = COL(T.textPrimary);
    
    ImGui::Dummy(ImVec2(0, 24.0f));
    ImVec2 p = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    dl->AddText(ImVec2(p.x + 8.0f, p.y + 4.0f), COL(T.textSecondary), lbl);
    DrawTextRightAligned(dl, ImVec2(p.x, p.y + 4.0f), w - 8.0f, valCol, val);
}

// ─── Single Axis Field (Clean Empty Input in Mixed State, Right-Aligned '--') ───
static bool RenderAxisField(const char* id, float* val, bool mixed, ImU32 col, float width, bool* outChanged) {
    bool changed = false;

    ImGui::SetNextItemWidth(width);
    ImGui::PushStyleColor(ImGuiCol_Text, col);

    if (mixed) {
        static std::map<std::string, std::string> s_mixedBuffers;
        std::string key = id;
        std::string& bufStr = s_mixedBuffers[key];

        char cBuf[32];
        strncpy_s(cBuf, bufStr.c_str(), sizeof(cBuf));

        ImVec2 curPos = ImGui::GetCursorScreenPos();
        bool enterPressed = ImGui::InputTextWithHint(id, "", cBuf, sizeof(cBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        bool isActive = ImGui::IsItemActive();

        // When not actively being typed in and buffer is empty, draw right-aligned "--"
        if (!isActive && cBuf[0] == '\0') {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 textSize = ImGui::CalcTextSize("--");
            dl->AddText(ImVec2(curPos.x + width - textSize.x - 4.0f, curPos.y + 2.0f), col, "--");
        }

        bufStr = cBuf;

        bool committed = enterPressed || ImGui::IsItemDeactivatedAfterEdit();
        if (committed && cBuf[0] != '\0') {
            try {
                float parsed = std::stof(cBuf);
                *val = parsed;
                changed = true;
                if (outChanged) *outChanged = true;
                bufStr.clear();
            } catch (...) {}
        } else if (ImGui::IsItemDeactivated() || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            bufStr.clear();
        }
    } else {
        bool drag = ImGui::DragFloat(id, val, 0.05f, 0.0f, 0.0f, "%.3f");
        bool editDone = ImGui::IsItemDeactivatedAfterEdit();
        if (drag || editDone) {
            changed = true;
            if (outChanged) *outChanged = true;
        }
    }

    ImGui::PopStyleColor();
    return changed;
}

// ─── Interactive Vec3 Row with Consolas Monospace Font & Mixed Value Support ──
static bool Vec3InteractiveRow(const char* lbl, float* x, float* y, float* z, 
                               bool mixedX = false, bool mixedY = false, bool mixedZ = false, 
                               bool isScale = false,
                               bool* outChangedX = nullptr, bool* outChangedY = nullptr, bool* outChangedZ = nullptr) {
    auto& T = NexusTheme::instance();
    ImGui::Dummy(ImVec2(0, 25.0f));
    ImVec2 p = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    dl->AddText(ImVec2(p.x + 8.0f, p.y + 4.0f), COL(T.textSecondary), lbl);

    float cw = 58.0f;
    float curX = p.x + w - (cw * 3.0f) - 8.0f;

    ImU32 xCol = isScale ? IM_COL32_WHITE : COL(T.accentRed);
    ImU32 yCol = isScale ? IM_COL32_WHITE : COL(T.accentGreen);
    ImU32 zCol = isScale ? IM_COL32_WHITE : COL(T.accent);

    bool changed = false;

    // Remove all background stroke / borders
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1,1,1,0.05f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1,1,1,0.08f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 2.0f));

    ImFont* monoFont = ImGuiLayer::instance().getMonoFont();
    if (monoFont) ImGui::PushFont(monoFont);

    // X axis
    ImGui::SetCursorScreenPos(ImVec2(curX, p.y + 2.0f));
    char idX[32]; snprintf(idX, sizeof(idX), "##%s_X", lbl);
    if (RenderAxisField(idX, x, mixedX, xCol, cw, outChangedX)) changed = true;

    curX += cw;
    // Y axis
    ImGui::SetCursorScreenPos(ImVec2(curX, p.y + 2.0f));
    char idY[32]; snprintf(idY, sizeof(idY), "##%s_Y", lbl);
    if (RenderAxisField(idY, y, mixedY, yCol, cw, outChangedY)) changed = true;

    curX += cw;
    // Z axis
    ImGui::SetCursorScreenPos(ImVec2(curX, p.y + 2.0f));
    char idZ[32]; snprintf(idZ, sizeof(idZ), "##%s_Z", lbl);
    if (RenderAxisField(idZ, z, mixedZ, zCol, cw, outChangedZ)) changed = true;

    if (monoFont) ImGui::PopFont();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    return changed;
}

// ─── Interactive Slider Row (With Mixed Value Support) ────────────────────────
static bool SliderRow(const char* lbl, float* val, float mn = 0.0f, float mx = 1.0f, bool isMixed = false) {
    auto& T = NexusTheme::instance();
    ImGui::Dummy(ImVec2(0, 32.0f));
    ImVec2 p = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    char valStr[16];
    if (isMixed) {
        snprintf(valStr, sizeof(valStr), "--");
    } else {
        snprintf(valStr, sizeof(valStr), "%.2f", *val);
    }

    // Label on top
    dl->AddText(ImVec2(p.x + 8.0f, p.y + 2.0f), COL(T.textSecondary), lbl);
    DrawTextRightAligned(dl, ImVec2(p.x, p.y + 2.0f), w - 8.0f, isMixed ? COL(T.textMuted) : COL(T.accent), valStr);

    // Track on bottom
    float sliderW = w - 16.0f;
    float curX = p.x + 8.0f;
    float trackY = p.y + 23.0f;

    // Background track
    dl->AddRectFilled(ImVec2(curX, trackY - 1.5f), ImVec2(curX + sliderW, trackY + 1.5f), COLA(0xFFFFFF, 0.20f), 1.5f);
    
    // Fill track
    float fillW = isMixed ? (sliderW * 0.5f) : (sliderW * std::clamp((*val - mn) / (mx - mn), 0.0f, 1.0f));
    dl->AddRectFilled(ImVec2(curX, trackY - 1.5f), ImVec2(curX + fillW, trackY + 1.5f), isMixed ? IM_COL32(80, 80, 80, 255) : COL(T.accent), 1.5f);

    // Thumb 11x11px
    ImVec2 thumbCenter(curX + fillW, trackY);
    dl->AddCircleFilled(thumbCenter, 5.5f, IM_COL32_WHITE);
    dl->AddCircle(thumbCenter, 5.5f, isMixed ? IM_COL32(100, 100, 100, 255) : COL(T.accent), 0, 1.0f);
    
    // Interactive invisible slider
    ImGui::SetCursorScreenPos(ImVec2(curX, p.y + 16.0f));
    ImGui::SetNextItemWidth(sliderW);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0,0,0,0));
    char id[64]; snprintf(id, sizeof(id), "##sl_%s", lbl);
    bool changed = ImGui::SliderFloat(id, val, mn, mx, "");
    if (isMixed && (ImGui::IsItemDeactivatedAfterEdit() || (GImGui->InputTextDeactivatedState.ID == ImGui::GetItemID() && !ImGui::IsKeyPressed(ImGuiKey_Escape)))) {
        changed = true;
    }
    ImGui::PopStyleColor(6);

    return changed;
}

// ═══════════════════════════════════════════════════════════════════════════════
void PropertiesPanel::draw() {
    auto& T = NexusTheme::instance();

    if (!EditorLayout::instance().showProperties) return;

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_HiddenTabBar;
    ImGui::SetNextWindowClass(&window_class);
    float minW = Editor::UI::CalculateNodeMinTabWidth("Properties");
    ImGui::SetNextWindowSizeConstraints(ImVec2(minW, 80.0f), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.bgPanel);
    ImGui::Begin("Properties", &EditorLayout::instance().showProperties, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove); 
    ImGui::PopStyleColor();

    Editor::UI::DrawSingleTabHeader("Properties", "icon_properties_bold", 150.0f, ImGui::ColorConvertFloat4ToU32(T.accent));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    float width = ImGui::GetWindowWidth();
    float height = ImGui::GetWindowHeight();
    
    // Retrieve all selected instances
    auto selectionList = SelectionManager::instance().getSelectionList();
    if (selectionList.empty() && SelectionManager::instance().getSelected()) {
        selectionList.push_back(SelectionManager::instance().getSelected());
    }

    // ─────────────────────────────────────────────────────────────────────────
    // EMPTY STATE
    // ─────────────────────────────────────────────────────────────────────────
    if (selectionList.empty()) {
        ImGui::SetCursorPos(ImVec2(18.0f, 48.0f));
        ImGui::TextColored(T.textMuted, "No object selected.");
        ImGui::SetCursorPos(ImVec2(18.0f, 70.0f));
        ImGui::TextColored(T.textSecondary, "Select one or more items in Explorer or Viewport.");
        
        // Borders
        dl->AddLine(ImVec2(winPos.x + width - 1.0f, winPos.y), ImVec2(winPos.x + width - 1.0f, winPos.y + height), COL(T.border));
        dl->AddLine(ImVec2(winPos.x, winPos.y + height - 1.0f), ImVec2(winPos.x + width, winPos.y + height - 1.0f), COL(T.border));
        
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    // Calculate common meta information
    bool isMulti = (selectionList.size() > 1);

    bool allSameClass = true;
    std::string firstClass = selectionList[0]->getClassName();
    for (size_t i = 1; i < selectionList.size(); ++i) {
        if (selectionList[i]->getClassName() != firstClass) {
            allSameClass = false;
            break;
        }
    }
    std::string displayClass = allSameClass ? firstClass : "Mixed";

    bool allSameName = true;
    std::string firstName = selectionList[0]->name;
    for (size_t i = 1; i < selectionList.size(); ++i) {
        if (selectionList[i]->name != firstName) {
            allSameName = false;
            break;
        }
    }

    bool allSameParent = true;
    auto firstParent = selectionList[0]->getParent();
    std::string firstParentName = firstParent ? (firstParent->name.empty() ? firstParent->getClassName() : firstParent->name) : "None";
    for (size_t i = 1; i < selectionList.size(); ++i) {
        auto p = selectionList[i]->getParent();
        std::string pName = p ? (p->name.empty() ? p->getClassName() : p->name) : "None";
        if (pName != firstParentName) {
            allSameParent = false;
            break;
        }
    }
    std::string displayParent = allSameParent ? firstParentName : "(Multiple Parents)";

    // Filter Part instances
    std::vector<std::shared_ptr<Part>> selectedParts;
    for (auto& inst : selectionList) {
        if (auto p = std::dynamic_pointer_cast<Part>(inst)) {
            selectedParts.push_back(p);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // SCROLLABLE CONTENT (Fresh & Spacious bgDeepest)
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.bgDeepest); 
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
    
    ImGui::BeginChild("##PropScroll", ImVec2(0, 0), false, 0);

    // Multi-Selection Header Banner
    if (isMulti) {
        ImGui::Dummy(ImVec2(0, 28.0f));
        ImVec2 bMin = ImGui::GetItemRectMin();
        float bw = ImGui::GetContentRegionAvail().x;
        dl->AddRectFilled(bMin, ImVec2(bMin.x + bw, bMin.y + 26.0f), COLA(0x82D9FF, 0.10f), 4.0f);
        dl->AddRect(bMin, ImVec2(bMin.x + bw, bMin.y + 26.0f), COLA(0x82D9FF, 0.30f), 4.0f);

        char bannerText[128];
        snprintf(bannerText, sizeof(bannerText), "%zu Objects Selected (%s)", selectionList.size(), displayClass.c_str());
        dl->AddText(ImVec2(bMin.x + 8.0f, bMin.y + 5.0f), COL(T.accent), bannerText);
    }

    // ─── 1. DATA CATEGORY (Name, Class, Parent Trio) ──────────────────────────
    if (SectionHeader("Data", "icon_data", nullptr)) {
        // Name Row (Editable Input)
        {
            ImGui::Dummy(ImVec2(0, 26.0f));
            ImVec2 p = ImGui::GetItemRectMin();
            ImDrawList* cDl = ImGui::GetWindowDrawList();
            float w = ImGui::GetContentRegionAvail().x;

            cDl->AddText(ImVec2(p.x + 8.0f, p.y + 4.0f), COL(T.textSecondary), "Name");

            char nameBuf[128];
            if (allSameName) {
                strcpy_s(nameBuf, firstName.c_str());
            } else {
                nameBuf[0] = '\0';
            }
            float inputW = std::max(w - 110.0f, 100.0f);

            ImGui::SetCursorScreenPos(ImVec2(p.x + w - inputW - 8.0f, p.y + 2.0f));
            ImGui::SetNextItemWidth(inputW);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, T.bgPanel);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, T.bgCard);
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, T.bgCard);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));

            bool nameSubmitted = ImGui::InputTextWithHint("##PropDataName", allSameName ? "" : "Multiple Values", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue);
            if (nameSubmitted || ImGui::IsItemDeactivatedAfterEdit()) {
                if (strlen(nameBuf) > 0) {
                    for (auto& inst : selectionList) {
                        if (inst) inst->name = nameBuf;
                    }
                }
            }

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
        }

        // Class Row
        PropRow("Class", displayClass.c_str(), COL(T.accent));

        // Parent Row
        PropRow("Parent", displayParent.c_str(), COL(T.textPrimary));
    }

    // ─────────────────────────────────────────────────────────────────────────
    // IF PART / 3D SPATIAL OBJECTS IN SELECTION:
    // ─────────────────────────────────────────────────────────────────────────
    if (!selectedParts.empty()) {
        // ─── 2. TRANSFORM ────────────────────────────────────────────────────
        if (SectionHeader("Transform", "icon_transform", nullptr)) {
            // Position
            bool samePosX = true, samePosY = true, samePosZ = true;
            Engine::Math::Vector3 firstPos = selectedParts[0]->getPosition();
            for (size_t i = 1; i < selectedParts.size(); ++i) {
                Engine::Math::Vector3 p = selectedParts[i]->getPosition();
                if (std::abs(p.x - firstPos.x) > 0.001f) samePosX = false;
                if (std::abs(p.y - firstPos.y) > 0.001f) samePosY = false;
                if (std::abs(p.z - firstPos.z) > 0.001f) samePosZ = false;
            }
            Engine::Math::Vector3 pos = firstPos;
            bool chgX = false, chgY = false, chgZ = false;
            if (Vec3InteractiveRow("Position", &pos.x, &pos.y, &pos.z, !samePosX, !samePosY, !samePosZ, false, &chgX, &chgY, &chgZ)) {
                for (auto& p : selectedParts) {
                    Engine::Math::Vector3 cur = p->getPosition();
                    if (chgX) cur.x = pos.x;
                    if (chgY) cur.y = pos.y;
                    if (chgZ) cur.z = pos.z;
                    p->setPosition(cur);
                }
            }

            // Rotation
            static float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
            bool chgRotX = false, chgRotY = false, chgRotZ = false;
            Vec3InteractiveRow("Rotation", &rotX, &rotY, &rotZ, false, false, false, false, &chgRotX, &chgRotY, &chgRotZ);

            // Scale / Size
            bool sameSzX = true, sameSzY = true, sameSzZ = true;
            Engine::Math::Vector3 firstSz = selectedParts[0]->getSize();
            for (size_t i = 1; i < selectedParts.size(); ++i) {
                Engine::Math::Vector3 s = selectedParts[i]->getSize();
                if (std::abs(s.x - firstSz.x) > 0.001f) sameSzX = false;
                if (std::abs(s.y - firstSz.y) > 0.001f) sameSzY = false;
                if (std::abs(s.z - firstSz.z) > 0.001f) sameSzZ = false;
            }
            Engine::Math::Vector3 sz = firstSz;
            bool chgSzX = false, chgSzY = false, chgSzZ = false;
            if (Vec3InteractiveRow("Scale", &sz.x, &sz.y, &sz.z, !sameSzX, !sameSzY, !sameSzZ, true, &chgSzX, &chgSzY, &chgSzZ)) {
                for (auto& p : selectedParts) {
                    Engine::Math::Vector3 cur = p->getSize();
                    if (chgSzX) cur.x = sz.x;
                    if (chgSzY) cur.y = sz.y;
                    if (chgSzZ) cur.z = sz.z;
                    p->setSize(cur);
                }
            }

            // Pivot Offset
            static float pvtX = 0.0f, pvtY = 0.0f, pvtZ = 0.0f;
            Vec3InteractiveRow("Pivot Offset", &pvtX, &pvtY, &pvtZ, false, false, false, true);
        }

        // ─── 3. TAGS & LAYERS ────────────────────────────────────────────────
        if (SectionHeader("Tags & Layers", "icon_tags", nullptr)) {
            static std::map<const Instance*, std::vector<std::string>> s_instanceTags;
            auto& tags = s_instanceTags[selectionList[0].get()];

            ImGui::Dummy(ImVec2(0, 24.0f));
            ImVec2 pR = ImGui::GetItemRectMin();
            ImDrawList* l = ImGui::GetWindowDrawList();

            float cx = pR.x + 8.0f;

            for (size_t i = 0; i < tags.size(); i++) {
                const auto& tag = tags[i];
                ImVec2 ts = ImGui::CalcTextSize(tag.c_str());
                float bw = ts.x + 16.0f;

                ImVec2 pMin(cx, pR.y + 2.0f);
                ImVec2 pMax(cx + bw, pR.y + 22.0f);

                l->AddRectFilled(pMin, pMax, IM_COL32(30, 30, 30, 255), 6.0f);
                l->AddRect(pMin, pMax, COLA(0xFFFFFF, 0.18f), 6.0f);
                l->AddText(ImVec2(cx + 8.0f, pR.y + 4.0f), IM_COL32_WHITE, tag.c_str());

                cx += bw + 6.0f;
            }

            // "+" Button Pill
            float plusW = 24.0f, plusH = 20.0f;
            ImVec2 plusMin(cx, pR.y + 2.0f);

            ImGui::SetCursorScreenPos(plusMin);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.08f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0.20f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

            if (ImGui::Button("+##AddTagPill", ImVec2(plusW, plusH))) {
                ImGui::OpenPopup("AddTagPopup");
            }

            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(4);

            if (ImGui::BeginPopup("AddTagPopup")) {
                static char newTagBuf[64] = "";
                ImGui::TextColored(T.textSecondary, "New Tag Name:");
                ImGui::SetNextItemWidth(130.0f);
                bool submitted = ImGui::InputText("##NewTagInput", newTagBuf, sizeof(newTagBuf), ImGuiInputTextFlags_EnterReturnsTrue);
                if (ImGui::IsWindowAppearing()) {
                    ImGui::SetKeyboardFocusHere(-1);
                }
                if (submitted && strlen(newTagBuf) > 0) {
                    for (auto& inst : selectionList) {
                        s_instanceTags[inst.get()].push_back(newTagBuf);
                    }
                    newTagBuf[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            PropRow("Collision Layer", "Layer 1 (Default)", IM_COL32_WHITE);
            PropRow("Collision Mask", "All", COL(T.accent));
        }

        // ─── 4. MESH & GEOMETRY ──────────────────────────────────────────────
        if (SectionHeader("Mesh & Geometry", "icon_mesh2", nullptr)) {
            PropRow("Mesh Asset", "Block.mesh", IM_COL32_WHITE);
            PropRow("Collision Fidelity", "Precise Convex", IM_COL32_WHITE);
            
            static bool castShadow = true, recvShadow = true;
            ImGui::Dummy(ImVec2(0, 24.0f));
            ImVec2 p1 = ImGui::GetItemRectMin();
            ImGui::GetWindowDrawList()->AddText(ImVec2(p1.x + 8.0f, p1.y + 4.0f), COL(T.textSecondary), "Cast Shadows");
            ImGui::SetCursorScreenPos(ImVec2(p1.x + ImGui::GetContentRegionAvail().x - 36.0f, p1.y + 4.0f));
            if (DrawTogglePill("##cs", castShadow)) castShadow = !castShadow;

            ImGui::Dummy(ImVec2(0, 24.0f));
            ImVec2 p2 = ImGui::GetItemRectMin();
            ImGui::GetWindowDrawList()->AddText(ImVec2(p2.x + 8.0f, p2.y + 4.0f), COL(T.textSecondary), "Receive Shadows");
            ImGui::SetCursorScreenPos(ImVec2(p2.x + ImGui::GetContentRegionAvail().x - 36.0f, p2.y + 4.0f));
            if (DrawTogglePill("##rs", recvShadow)) recvShadow = !recvShadow;

            // Anchored
            bool allAnchored = true, noneAnchored = true;
            for (auto& p : selectedParts) {
                if (p->getAnchored()) noneAnchored = false;
                else allAnchored = false;
            }
            bool isMixedAnchored = (!allAnchored && !noneAnchored);
            bool currentAnchoredVal = allAnchored;

            ImGui::Dummy(ImVec2(0, 24.0f));
            ImVec2 p3 = ImGui::GetItemRectMin();
            ImGui::GetWindowDrawList()->AddText(ImVec2(p3.x + 8.0f, p3.y + 4.0f), COL(T.textSecondary), "Anchored");
            if (isMixedAnchored) {
                ImGui::GetWindowDrawList()->AddText(ImVec2(p3.x + 80.0f, p3.y + 4.0f), COL(T.textMuted), "(Mixed)");
            }
            ImGui::SetCursorScreenPos(ImVec2(p3.x + ImGui::GetContentRegionAvail().x - 36.0f, p3.y + 4.0f));
            if (DrawTogglePill("##anchored", currentAnchoredVal, isMixedAnchored)) {
                bool targetState = isMixedAnchored ? true : !allAnchored;
                for (auto& p : selectedParts) {
                    p->setAnchored(targetState);
                }
            }
        }

        // ─── 5. SHADER / APPEARANCE ──────────────────────────────────────────
        if (SectionHeader("Shader", "icon_shader", nullptr)) {
            // Albedo Tint
            bool allSameColor = true;
            Engine::Math::Vector3 firstCol = selectedParts[0]->getAlbedoColor();
            for (size_t i = 1; i < selectedParts.size(); ++i) {
                Engine::Math::Vector3 c = selectedParts[i]->getAlbedoColor();
                if (std::abs(c.x - firstCol.x) > 0.01f || std::abs(c.y - firstCol.y) > 0.01f || std::abs(c.z - firstCol.z) > 0.01f) {
                    allSameColor = false;
                    break;
                }
            }
            float colArr[3] = { firstCol.x, firstCol.y, firstCol.z };

            ImGui::Dummy(ImVec2(0, 26.0f));
            ImVec2 rowMin = ImGui::GetItemRectMin();
            float rw = ImGui::GetContentRegionAvail().x;
            ImDrawList* sDl = ImGui::GetWindowDrawList();

            sDl->AddText(ImVec2(rowMin.x + 8.0f, rowMin.y + 4.0f), COL(T.textSecondary), "Albedo Tint");
            if (!allSameColor) {
                sDl->AddText(ImVec2(rowMin.x + 85.0f, rowMin.y + 4.0f), COL(T.textMuted), "(Mixed)");
            }
            ImGui::SetCursorScreenPos(ImVec2(rowMin.x + rw - 36.0f, rowMin.y + 3.0f));
            if (ImGui::ColorEdit3("##AlbedoEdit", colArr, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                for (auto& p : selectedParts) {
                    p->setAlbedoColor(Engine::Math::Vector3(colArr[0], colArr[1], colArr[2]));
                }
            }

            // Alpha / Transparency
            bool allSameAlpha = true;
            float firstAlpha = 1.0f - selectedParts[0]->transparency;
            for (size_t i = 1; i < selectedParts.size(); ++i) {
                float a = 1.0f - selectedParts[i]->transparency;
                if (std::abs(a - firstAlpha) > 0.01f) { allSameAlpha = false; break; }
            }
            float alpha = firstAlpha;
            if (SliderRow("Alpha", &alpha, 0.0f, 1.0f, !allSameAlpha)) {
                for (auto& p : selectedParts) {
                    p->transparency = 1.0f - alpha;
                    p->markRenderDirty();
                }
            }

            // Metallic
            bool allSameMet = true;
            float firstMet = selectedParts[0]->metallic;
            for (size_t i = 1; i < selectedParts.size(); ++i) {
                if (std::abs(selectedParts[i]->metallic - firstMet) > 0.01f) { allSameMet = false; break; }
            }
            float met = firstMet;
            if (SliderRow("Metallic", &met, 0.0f, 1.0f, !allSameMet)) {
                for (auto& p : selectedParts) {
                    p->setMetallic(met);
                }
            }

            // Roughness
            bool allSameRough = true;
            float firstRough = selectedParts[0]->roughness;
            for (size_t i = 1; i < selectedParts.size(); ++i) {
                if (std::abs(selectedParts[i]->roughness - firstRough) > 0.01f) { allSameRough = false; break; }
            }
            float rough = firstRough;
            if (SliderRow("Roughness", &rough, 0.0f, 1.0f, !allSameRough)) {
                for (auto& p : selectedParts) {
                    p->setRoughness(rough);
                }
            }

            // Normal Strength
            static float normalStr = 0.80f;
            SliderRow("Normal Strength", &normalStr, 0.0f, 2.0f);

            // ─────────────────────────────────────────────────────────────────
            // MESH & MODELING (EditableMesh inspection, Shading, Normals, Export)
            // ─────────────────────────────────────────────────────────────────
            if (SectionHeader("Mesh & Topology", "icon_mesh_bold", nullptr)) {
                auto singlePart = selectedParts[0];
                singlePart->ensureEditableMesh();
                auto mesh = singlePart->getEditableMesh();
                if (mesh) {
                    char vInfo[32], fInfo[32], eInfo[32];
                    snprintf(vInfo, sizeof(vInfo), "%zu", mesh->getVertices().size());
                    snprintf(fInfo, sizeof(fInfo), "%zu", mesh->getFaces().size());
                    snprintf(eInfo, sizeof(eInfo), "%zu", mesh->getEdges().size());

                    PropRow("Vertices", vInfo, COL(T.textPrimary));
                    PropRow("Faces (Polygons)", fInfo, COL(T.textPrimary));
                    PropRow("Edges", eInfo, COL(T.textPrimary));

                    ImGui::Spacing();
                    float btnW = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;

                    if (ImGui::Button("Recalculate Normals", ImVec2(btnW, 24.0f))) {
                        mesh->recalculateAllNormals(false);
                        singlePart->rebuildProceduralMesh();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Shade Smooth", ImVec2(btnW, 24.0f))) {
                        mesh->recalculateAllNormals(true, 30.0f);
                        singlePart->rebuildProceduralMesh();
                    }

                    if (ImGui::Button("Box UV Project", ImVec2(btnW, 24.0f))) {
                        Engine::Geometry::UVUnwrapper::boxProject(*mesh, 1.0f);
                        singlePart->rebuildProceduralMesh();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Smart UV Unwrap", ImVec2(btnW, 24.0f))) {
                        Engine::Geometry::UVUnwrapper::smartUVProject(*mesh);
                        singlePart->rebuildProceduralMesh();
                    }

                    if (ImGui::Button("Export to OBJ...", ImVec2(-1.0f, 26.0f))) {
                        std::string exportPath = "Assets/" + singlePart->name + ".obj";
                        Engine::Asset::MeshAssetExporter::exportToObj(*mesh, exportPath);
                    }
                }
            }
        }
    }
    // ─────────────────────────────────────────────────────────────────────────
    // IF SCRIPT:
    // ─────────────────────────────────────────────────────────────────────────
    else if (displayClass == "Script") {
        if (SectionHeader("Scripting", "icon_script", nullptr)) {
            PropRow("Script Type", "Server Script", COL(T.accentGreen));
            PropRow("Execution", "Luau VM Runtime", COL(T.accent));
            PropRow("Status", "Active", COL(T.textPrimary));
        }
    }
    // ─────────────────────────────────────────────────────────────────────────
    // IF SERVICE / FOLDER / OTHER INSTANCE
    // ─────────────────────────────────────────────────────────────────────────
    else {
        if (SectionHeader("Service Details", "icon_service", nullptr)) {
            char childBuf[32];
            snprintf(childBuf, sizeof(childBuf), "%zu items", selectionList[0]->getChildren().size());
            PropRow("Children Count", childBuf, COL(T.textPrimary));

            if (displayClass == "ServerScriptService") {
                PropRow("Context", "Server Only (Protected)", COL(T.accentGold));
                PropRow("Replication", "Not Replicated to Clients", COL(T.textMuted));
            } else if (displayClass == "ReplicatedStorage") {
                PropRow("Context", "Shared (Server & Client)", COL(T.accent));
                PropRow("Replication", "Auto Replicated", COL(T.accentGreen));
            } else if (displayClass == "Workspace") {
                PropRow("Context", "World Root", COL(T.accent));
                PropRow("Physics Mode", "Jolt Physics Active", COL(T.accentGreen));
            } else if (displayClass == "Lighting") {
                PropRow("Environment", "PBR / Directional Sunlight", COL(T.accentGold));
            } else if (displayClass == "SoundService") {
                PropRow("Audio Engine", "Spatial 3D Sound", COL(T.accent));
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    // Borders
    dl->AddLine(ImVec2(winPos.x + width - 1.0f, winPos.y), ImVec2(winPos.x + width - 1.0f, winPos.y + height), COL(T.border));
    dl->AddLine(ImVec2(winPos.x, winPos.y + height - 1.0f), ImVec2(winPos.x + width, winPos.y + height - 1.0f), COL(T.border));

    ImGui::End();
    ImGui::PopStyleVar();
}

void PropertiesPanel::drawPropertyEditor(const std::shared_ptr<Instance>& inst, const Engine::Reflection::PropertyDescriptor* prop) {
}
