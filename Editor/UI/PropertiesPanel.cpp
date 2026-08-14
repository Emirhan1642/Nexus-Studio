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

// ─── Toggle Pill ─────────────────────────────────────────────────────────────
static bool DrawTogglePill(const char* id, bool value) {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = 28.0f, h = 14.0f;
    ImVec2 p = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton(id, ImVec2(w, h));
    bool clicked = ImGui::IsItemClicked();

    ImU32 bg = value ? COL(T.accentGreen) : IM_COL32(36, 36, 36, 255);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, h * 0.5f);
    float cx = value ? p.x + w - h * 0.5f : p.x + h * 0.5f;
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

// ─── Interactive Vec3 Row with Consolas Monospace Font & No Background Stroke ─
static bool Vec3InteractiveRow(const char* lbl, float* x, float* y, float* z, bool isScale = false) {
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
    ImGui::SetNextItemWidth(cw);
    ImGui::PushStyleColor(ImGuiCol_Text, xCol);
    char idX[32]; snprintf(idX, sizeof(idX), "##%s_X", lbl);
    if (ImGui::DragFloat(idX, x, 0.05f, 0.0f, 0.0f, "%.3f")) changed = true;
    ImGui::PopStyleColor();

    curX += cw;
    // Y axis
    ImGui::SetCursorScreenPos(ImVec2(curX, p.y + 2.0f));
    ImGui::SetNextItemWidth(cw);
    ImGui::PushStyleColor(ImGuiCol_Text, yCol);
    char idY[32]; snprintf(idY, sizeof(idY), "##%s_Y", lbl);
    if (ImGui::DragFloat(idY, y, 0.05f, 0.0f, 0.0f, "%.3f")) changed = true;
    ImGui::PopStyleColor();

    curX += cw;
    // Z axis
    ImGui::SetCursorScreenPos(ImVec2(curX, p.y + 2.0f));
    ImGui::SetNextItemWidth(cw);
    ImGui::PushStyleColor(ImGuiCol_Text, zCol);
    char idZ[32]; snprintf(idZ, sizeof(idZ), "##%s_Z", lbl);
    if (ImGui::DragFloat(idZ, z, 0.05f, 0.0f, 0.0f, "%.3f")) changed = true;
    ImGui::PopStyleColor();

    if (monoFont) ImGui::PopFont();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    return changed;
}

// ─── Interactive Slider Row (Spacious 32px height, 3px track with thumb) ─────
static bool SliderRow(const char* lbl, float* val, float mn = 0.0f, float mx = 1.0f) {
    auto& T = NexusTheme::instance();
    ImGui::Dummy(ImVec2(0, 32.0f));
    ImVec2 p = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    char valStr[16];
    snprintf(valStr, sizeof(valStr), "%.2f", *val);

    // Label on top
    dl->AddText(ImVec2(p.x + 8.0f, p.y + 2.0f), COL(T.textSecondary), lbl);
    DrawTextRightAligned(dl, ImVec2(p.x, p.y + 2.0f), w - 8.0f, COL(T.accent), valStr);

    // Track on bottom
    float sliderW = w - 16.0f;
    float curX = p.x + 8.0f;
    float trackY = p.y + 23.0f;

    // Background track
    dl->AddRectFilled(ImVec2(curX, trackY - 1.5f), ImVec2(curX + sliderW, trackY + 1.5f), COLA(0xFFFFFF, 0.20f), 1.5f);
    
    // Fill track
    float fillW = sliderW * std::clamp((*val - mn) / (mx - mn), 0.0f, 1.0f);
    dl->AddRectFilled(ImVec2(curX, trackY - 1.5f), ImVec2(curX + fillW, trackY + 1.5f), COL(T.accent), 1.5f);

    // Thumb 11x11px
    ImVec2 thumbCenter(curX + fillW, trackY);
    dl->AddCircleFilled(thumbCenter, 5.5f, IM_COL32_WHITE);
    dl->AddCircle(thumbCenter, 5.5f, COL(T.accent), 0, 1.0f);
    
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
    
    auto selected = SelectionManager::instance().getSelected();

    // ─────────────────────────────────────────────────────────────────────────
    // EMPTY STATE
    // ─────────────────────────────────────────────────────────────────────────
    if (!selected) {
        ImGui::SetCursorPos(ImVec2(18.0f, 48.0f));
        ImGui::TextColored(T.textMuted, "No object selected.");
        ImGui::SetCursorPos(ImVec2(18.0f, 70.0f));
        ImGui::TextColored(T.textSecondary, "Select an item in Explorer or Viewport.");
        
        // Borders
        dl->AddLine(ImVec2(winPos.x + width - 1.0f, winPos.y), ImVec2(winPos.x + width - 1.0f, winPos.y + height), COL(T.border));
        dl->AddLine(ImVec2(winPos.x, winPos.y + height - 1.0f), ImVec2(winPos.x + width, winPos.y + height - 1.0f), COL(T.border));
        
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    std::string className = selected->getClassName();
    std::string displayName = selected->name;

    if (className == "Instance" && (displayName == "Workspace" || displayName == "Lighting" || 
                                   displayName == "ServerScriptService" || displayName == "ReplicatedStorage" ||
                                   displayName == "SoundService")) {
        className = displayName;
    }

    std::string parentName = "None";
    if (auto parent = selected->getParent()) {
        parentName = parent->name.empty() ? parent->getClassName() : parent->name;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // SCROLLABLE CONTENT (Fresh & Spacious bgDeepest)
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.bgDeepest); 
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
    
    ImGui::BeginChild("##PropScroll", ImVec2(0, 0), false, 0);

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
            strcpy_s(nameBuf, selected->name.c_str());
            float inputW = std::max(w - 110.0f, 100.0f);

            ImGui::SetCursorScreenPos(ImVec2(p.x + w - inputW - 8.0f, p.y + 2.0f));
            ImGui::SetNextItemWidth(inputW);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, T.bgPanel);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, T.bgCard);
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, T.bgCard);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));

            if (ImGui::InputText("##PropDataName", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                selected->name = nameBuf;
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                selected->name = nameBuf;
            }

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
        }

        // Class Row
        PropRow("Class", className.c_str(), COL(T.accent));

        // Parent Row
        PropRow("Parent", parentName.c_str(), COL(T.textPrimary));
    }

    // ─────────────────────────────────────────────────────────────────────────
    // IF PART / 3D SPATIAL OBJECT:
    // ─────────────────────────────────────────────────────────────────────────
    auto part = std::dynamic_pointer_cast<Part>(selected);
    if (part) {
        // ─── 2. TRANSFORM ────────────────────────────────────────────────────
        if (SectionHeader("Transform", "icon_transform", nullptr)) {
            // Position
            Engine::Math::Vector3 pos = part->getPosition();
            if (Vec3InteractiveRow("Position", &pos.x, &pos.y, &pos.z, false)) {
                part->setPosition(pos);
            }

            // Rotation
            static float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
            Vec3InteractiveRow("Rotation", &rotX, &rotY, &rotZ, false);

            // Scale / Size
            Engine::Math::Vector3 sz = part->getSize();
            if (Vec3InteractiveRow("Scale", &sz.x, &sz.y, &sz.z, true)) {
                part->setSize(sz);
            }

            // Pivot Offset
            static float pvtX = 0.0f, pvtY = 0.0f, pvtZ = 0.0f;
            Vec3InteractiveRow("Pivot Offset", &pvtX, &pvtY, &pvtZ, true);
        }

        // ─── 3. TAGS & LAYERS ────────────────────────────────────────────────
        if (SectionHeader("Tags & Layers", "icon_tags", nullptr)) {
            static std::map<const Instance*, std::vector<std::string>> s_instanceTags;
            auto& tags = s_instanceTags[selected.get()];

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
                    tags.push_back(newTagBuf);
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
            bool isAnchored = part->getAnchored();
            ImGui::Dummy(ImVec2(0, 24.0f));
            ImVec2 p3 = ImGui::GetItemRectMin();
            ImGui::GetWindowDrawList()->AddText(ImVec2(p3.x + 8.0f, p3.y + 4.0f), COL(T.textSecondary), "Anchored");
            ImGui::SetCursorScreenPos(ImVec2(p3.x + ImGui::GetContentRegionAvail().x - 36.0f, p3.y + 4.0f));
            if (DrawTogglePill("##anchored", isAnchored)) {
                part->setAnchored(!isAnchored);
            }
        }

        // ─── 5. SHADER / APPEARANCE ──────────────────────────────────────────
        if (SectionHeader("Shader", "icon_shader", nullptr)) {
            // Albedo Tint
            Engine::Math::Vector3 albCol = part->getAlbedoColor();
            float colArr[3] = { albCol.x, albCol.y, albCol.z };

            ImGui::Dummy(ImVec2(0, 26.0f));
            ImVec2 rowMin = ImGui::GetItemRectMin();
            float rw = ImGui::GetContentRegionAvail().x;
            ImDrawList* sDl = ImGui::GetWindowDrawList();

            sDl->AddText(ImVec2(rowMin.x + 8.0f, rowMin.y + 4.0f), COL(T.textSecondary), "Albedo Tint");
            ImGui::SetCursorScreenPos(ImVec2(rowMin.x + rw - 36.0f, rowMin.y + 3.0f));
            if (ImGui::ColorEdit3("##AlbedoEdit", colArr, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                part->setAlbedoColor(Engine::Math::Vector3(colArr[0], colArr[1], colArr[2]));
            }

            // Alpha / Transparency
            float alpha = 1.0f - part->transparency;
            if (SliderRow("Alpha", &alpha, 0.0f, 1.0f)) {
                part->transparency = 1.0f - alpha;
                part->markRenderDirty();
            }

            // Metallic
            float met = part->metallic;
            if (SliderRow("Metallic", &met, 0.0f, 1.0f)) {
                part->setMetallic(met);
            }

            // Roughness
            float rough = part->roughness;
            if (SliderRow("Roughness", &rough, 0.0f, 1.0f)) {
                part->setRoughness(rough);
            }

            // Normal Strength
            static float normalStr = 0.80f;
            SliderRow("Normal Strength", &normalStr, 0.0f, 2.0f);
        }
    }
    // ─────────────────────────────────────────────────────────────────────────
    // IF SCRIPT:
    // ─────────────────────────────────────────────────────────────────────────
    else if (selected->getClassName() == "Script") {
        if (SectionHeader("Scripting", "icon_script", nullptr)) {
            PropRow("Script Type", "Server Script", COL(T.accentGreen));
            PropRow("Execution", "Luau VM Runtime", COL(T.accent));
            PropRow("Status", "Active", COL(T.textPrimary));
        }
    }
    // ─────────────────────────────────────────────────────────────────────────
    // IF SERVICE / FOLDER / OTHER INSTANCE (ServerScriptService, ReplicatedStorage etc.)
    // ─────────────────────────────────────────────────────────────────────────
    else {
        if (SectionHeader("Service Details", "icon_service", nullptr)) {
            char childBuf[32];
            snprintf(childBuf, sizeof(childBuf), "%zu items", selected->getChildren().size());
            PropRow("Children Count", childBuf, COL(T.textPrimary));

            if (className == "ServerScriptService") {
                PropRow("Context", "Server Only (Protected)", COL(T.accentGold));
                PropRow("Replication", "Not Replicated to Clients", COL(T.textMuted));
            } else if (className == "ReplicatedStorage") {
                PropRow("Context", "Shared (Server & Client)", COL(T.accent));
                PropRow("Replication", "Auto Replicated", COL(T.accentGreen));
            } else if (className == "Workspace") {
                PropRow("Context", "World Root", COL(T.accent));
                PropRow("Physics Mode", "Jolt Physics Active", COL(T.accentGreen));
            } else if (className == "Lighting") {
                PropRow("Environment", "PBR / Directional Sunlight", COL(T.accentGold));
            } else if (className == "SoundService") {
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
