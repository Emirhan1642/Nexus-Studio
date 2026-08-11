#include "PropertiesPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "SelectionManager.h"
#include "../Undo/UndoStack.h"
#include "Engine/Core/Reflection/TypeRegistry.h"
#include "Engine/Core/Reflection/EnumRegistry.h"
#include "Engine/Core/DataModel/Instance.h"
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Core/Math/Vector3.h"
#include "Engine/Assets/AssetDatabase.h"
#include "Engine/Assets/AssetDependencyTracker.h"
#include "IconRegistry.h"
#include "NexusTheme.h"
#include "EditorLayout.h"
#include "SharedTabBar.h"
#include <string>
#include <any>
#include <map>
#include <cctype>

static ImU32 COL(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a) {
    return IM_COL32((hex>>16)&0xFF,(hex>>8)&0xFF,hex&0xFF,(uint8_t)(a*255));
}

// Draw text aligned to right bound
static void DrawTextRightAligned(ImDrawList* dl, ImVec2 pos, float width, ImU32 col, const char* text) {
    ImVec2 size = ImGui::CalcTextSize(text);
    dl->AddText(ImVec2(pos.x + width - size.x, pos.y), col, text);
}

// ─── Toggle Pill ─────────────────────────────────────────────────────────────
static bool DrawTogglePill(const char* id, bool value) {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = 26.66f, h = 13.33f; // From Spec: Checkbox 13.33 x 13.33 (approx toggle size)
    ImVec2 p = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton(id, ImVec2(w, h));
    bool clicked = ImGui::IsItemClicked();

    ImU32 bg = value ? COL(T.accentGreen) : IM_COL32(36,36,36,255);
    dl->AddRectFilled(p, ImVec2(p.x+w, p.y+h), bg, h*0.5f);
    float cx = value ? p.x + w - h*0.5f : p.x + h*0.5f;
    dl->AddCircleFilled(ImVec2(cx, p.y + h*0.5f), h*0.5f - 2.0f, IM_COL32_WHITE);

    return clicked;
}

static bool SectionHeader(const char* label, const char* iconKey = nullptr, const char* rightLabel = nullptr, bool defaultOpen = true) {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float width = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();

    static std::map<std::string, bool> s_openState;
    if (s_openState.find(label) == s_openState.end()) {
        s_openState[label] = defaultOpen;
    }
    bool& open = s_openState[label];

    ImGui::InvisibleButton(label, ImVec2(width, 20)); // Spec height 20px
    if (ImGui::IsItemClicked()) open = !open;

    // Spec: HorizontalDivider 240x1px
    dl->AddLine(ImVec2(p.x, p.y + 19), ImVec2(p.x + width, p.y + 19), COLA(0xFFFFFF, 0.20f));

    // Arrow + Icon + Label
    ImTextureID chevron = IconRegistry::instance().get(open ? "icon_chevron_down_bold" : "icon_chevron_right_bold");
    if (chevron) {
        dl->AddImage(chevron, ImVec2(p.x, p.y + 2.0f), ImVec2(p.x + 16.0f, p.y + 18.0f), ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE);
    } else {
        const char* arrow = open ? "v" : ">";
        dl->AddText(ImVec2(p.x, p.y + 3), IM_COL32_WHITE, arrow);
    }

    float cx = p.x + 18.0f;
    if (iconKey) {
        ImTextureID iconTex = IconRegistry::instance().get(iconKey);
        if (iconTex) {
            dl->AddImage(iconTex, ImVec2(cx, p.y + 2.0f), ImVec2(cx + 16.0f, p.y + 18.0f));
        }
        cx += 20.0f;
    }

    dl->AddText(ImVec2(cx, p.y + 3), IM_COL32_WHITE, label); 

    if (rightLabel) {
        ImVec2 size = ImGui::CalcTextSize(rightLabel);
        dl->AddText(ImVec2(p.x + width - size.x - 4, p.y + 3), COL(T.textMuted), rightLabel);
    }

    return open;
}

static void PropRow(const char* lbl, const char* val, ImU32 valCol = 0) {
    auto& T = NexusTheme::instance();
    if (valCol == 0) valCol = COL(T.textPrimary);
    
    ImGui::Dummy(ImVec2(0, 20)); // Spec height 20px
    ImVec2 p = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    dl->AddText(ImVec2(p.x + 5, p.y + 3), COL(T.textSecondary), lbl); // padding 5px L
    DrawTextRightAligned(dl, ImVec2(p.x, p.y + 3), w - 5, valCol, val);
}

static void Vec3Row(const char* lbl, const char* x, const char* y, const char* z) {
    auto& T = NexusTheme::instance();
    ImGui::Dummy(ImVec2(0, 20));
    ImVec2 p = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    dl->AddText(ImVec2(p.x + 5, p.y + 3), COL(T.textSecondary), lbl);
    
    // Value sütunları 60px.
    float cw = 60.0f;
    float curX = p.x + w - (cw * 3) - 5; // Right aligned with 5px padding

    // Spec renkler: X->Red, Y->Green, Z->Blue, Scale->White
    ImU32 xCol = COL(T.accentRed);
    ImU32 yCol = COL(T.accentGreen);
    ImU32 zCol = COL(T.accent);
    if (std::string(lbl) == "Scale") {
        xCol = yCol = zCol = IM_COL32_WHITE;
    }

    DrawTextRightAligned(dl, ImVec2(curX, p.y + 3), cw, xCol, x);
    curX += cw;
    DrawTextRightAligned(dl, ImVec2(curX, p.y + 3), cw, yCol, y);
    curX += cw;
    DrawTextRightAligned(dl, ImVec2(curX, p.y + 3), cw, zCol, z);
}

static void SliderRow(const char* lbl, const char* valStr, float* val, float mn, float mx) {
    auto& T = NexusTheme::instance();
    ImGui::Dummy(ImVec2(0, 30)); // Spec: height 30px
    ImVec2 p = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    // Label üst yarıda (üst 15px)
    dl->AddText(ImVec2(p.x + 5, p.y + 2), COL(T.textSecondary), lbl);
    DrawTextRightAligned(dl, ImVec2(p.x, p.y + 2), w - 5, COL(T.accent), valStr);

    // Track alt yarıda (alt 15px)
    float sliderW = w - 10.0f;
    float curX = p.x + 5.0f;
    float trackY = p.y + 22.0f;

    // Background track 3px height
    dl->AddRectFilled(ImVec2(curX, trackY - 1), ImVec2(curX + sliderW, trackY + 2), COLA(0xFFFFFF, 0.20f), 1.5f);
    
    // Fill track
    float fillW = sliderW * (*val - mn) / (mx - mn);
    dl->AddRectFilled(ImVec2(curX, trackY - 1), ImVec2(curX + fillW, trackY + 2), COL(T.accent), 1.5f);

    // Thumb 11x11px
    ImVec2 thumbCenter(curX + fillW, trackY + 0.5f);
    dl->AddCircleFilled(thumbCenter, 5.5f, IM_COL32_WHITE);
    dl->AddCircle(thumbCenter, 5.5f, COL(T.accent), 0, 1.0f);
    
    // Interactive part
    ImGui::SetCursorScreenPos(ImVec2(curX, p.y+15));
    ImGui::SetNextItemWidth(sliderW);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0,0,0,0));
    char id[64]; snprintf(id, sizeof(id), "##sl_%s", lbl);
    ImGui::SliderFloat(id, val, mn, mx, "");
    ImGui::PopStyleColor(6);
}

// ═══════════════════════════════════════════════════════════════════════════════
void PropertiesPanel::draw() {
    auto& T = NexusTheme::instance();

    if (!EditorLayout::instance().showProperties) return;

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_HiddenTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.bgPanel);
    ImGui::Begin("Properties", &EditorLayout::instance().showProperties, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar); 
    ImGui::PopStyleColor(); // Pop WindowBg right after Begin

    Editor::UI::DrawSingleTabHeader("Properties", "icon_properties_bold", 150.0f, ImGui::ColorConvertFloat4ToU32(T.accent));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    float width = ImGui::GetWindowWidth();
    float height = ImGui::GetWindowHeight();
    
    auto selected = SelectionManager::instance().getSelected();



    if (!selected) {
        ImGui::SetCursorPos(ImVec2(12, 40));
        ImGui::TextDisabled("Nothing selected");
        
        // Borders
        ImVec2 winP = ImGui::GetWindowPos();
        float winH = ImGui::GetWindowHeight();
        dl->AddLine(ImVec2(winP.x + width - 1.0f, winP.y), ImVec2(winP.x + width - 1.0f, winP.y + winH), COL(T.border));
        dl->AddLine(ImVec2(winP.x, winP.y + winH - 1.0f), ImVec2(winP.x + width, winP.y + winH - 1.0f), COL(T.border));
        
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // SCROLLABLE CONTENT (bgDeepest)
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.bgDeepest); 
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 10)); // Gap: 10px
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10)); // Padding: 10px
    
    ImGui::BeginChild("##PropScroll", ImVec2(0,0), false, 0);

    // ─── 1. TRANSFORM ────────────────────────────────────────────────────────
    if (SectionHeader("Transform", "icon_transform_bold", nullptr)) {
        auto* cls = Engine::Reflection::TypeRegistry::instance().find(selected->getClassName());
        bool hasPos = false, hasSz = false;
        if (cls) {
            for (auto& prop : cls->properties) {
                if (prop.name == "Position" && prop.getter) {
                    try {
                        auto v = std::any_cast<Engine::Math::Vector3>(prop.getter(selected.get()));
                        char xs[24],ys[24],zs[24]; snprintf(xs,24,"%.3f",v.x); snprintf(ys,24,"%.3f",v.y); snprintf(zs,24,"%.3f",v.z);
                        Vec3Row("Position", xs, ys, zs); hasPos=true;
                    } catch (...) {}
                }
                if (prop.name == "Size" && prop.getter) {
                    try {
                        auto v = std::any_cast<Engine::Math::Vector3>(prop.getter(selected.get()));
                        char xs[24],ys[24],zs[24]; snprintf(xs,24,"%.3f",v.x); snprintf(ys,24,"%.3f",v.y); snprintf(zs,24,"%.3f",v.z);
                        Vec3Row("Scale", xs, ys, zs); hasSz=true;
                    } catch (...) {}
                }
            }
        }
        
        if (!hasPos) Vec3Row("Position","0.000","24.000","-12.000");
        Vec3Row("Rotation", "0.0°","23.0°","14.3°");
        if (!hasSz)  Vec3Row("Scale","1.000","1.000","1.000");
        Vec3Row("Pivot Offset", "0.000","-0.500","0.000");
    }

    // ─── 2. TAGS & LAYERS ────────────────────────────────────────────────────
    if (SectionHeader("Tags & Layers", "icon_tags_bold", nullptr)) {
        ImGui::Dummy(ImVec2(0, 24));
        ImVec2 pR = ImGui::GetItemRectMin();
        ImDrawList* l = ImGui::GetWindowDrawList();
        
        const char* tags[] = {"Interactable", "LootBox", "Metallic_Heavy", "+"};
        float cx = pR.x + 5.0f;
        for (int i=0; i<4; i++) {
            ImVec2 ts = ImGui::CalcTextSize(tags[i]);
            float bw = ts.x + 10.0f; // 5px padding sol/sağ
            
            l->AddRect(ImVec2(cx, pR.y + 4.0f), ImVec2(cx + bw, pR.y + 19.0f), COLA(0xFFFFFF, 0.20f), 6.0f); // border-radius 6px
            l->AddText(ImVec2(cx + 5.0f, pR.y + 5.0f), IM_COL32_WHITE, tags[i]);
            
            cx += bw + 5.0f; // gap 5px
        }

        PropRow("Collision Layer", "Layer 3 (Props) v", IM_COL32_WHITE);
        PropRow("Collision Mask", "Player, Ground, Raycast", COL(T.accent));
    }

    // ─── 3. MESH & GEOMETRY ──────────────────────────────────────────────────
    if (SectionHeader("Mesh & Geometry", "icon_mesh2_bold", nullptr)) {
        PropRow("Mesh Asset", "Gold_Box.mesh v", IM_COL32_WHITE);
        PropRow("Collision Fidelity", "Precise Convex v", IM_COL32_WHITE);
        
        static bool castShadow = true, recvShadow = true;
        ImGui::Dummy(ImVec2(0, 20));
        ImVec2 p1 = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddText(ImVec2(p1.x + 5, p1.y + 3), COL(T.textSecondary), "Cast Shadows");
        ImGui::SetCursorScreenPos(ImVec2(p1.x + ImGui::GetContentRegionAvail().x - 30, p1.y + 3));
        if (DrawTogglePill("##cs", castShadow)) castShadow = !castShadow;

        ImGui::Dummy(ImVec2(0, 20));
        ImVec2 p2 = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddText(ImVec2(p2.x + 5, p2.y + 3), COL(T.textSecondary), "Receive Shadows");
        ImGui::SetCursorScreenPos(ImVec2(p2.x + ImGui::GetContentRegionAvail().x - 30, p2.y + 3));
        if (DrawTogglePill("##rs", recvShadow)) recvShadow = !recvShadow;
    }

    // ─── 4. SHADER ────────────────────────────────────────────────────────────
    if (SectionHeader("Shader", "icon_shader_bold", nullptr)) {
        
        PropRow("Albedo Tint", "      ");
        ImVec2 rowMin = ImGui::GetItemRectMin();
        float rw = ImGui::GetContentRegionAvail().x;
        // Albedo renk karesi sağa hizalı
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(rowMin.x + rw - 30, rowMin.y+4), ImVec2(rowMin.x + rw - 5, rowMin.y+16), COL(T.accentGold), 2.0f);
        
        static float alphaVal = 1.0f;
        static float normalStr = 0.8f;
        SliderRow("Alpha", "1.00", &alphaVal, 0.0f, 1.0f);
        SliderRow("Normal Strength", "0.80", &normalStr, 0.0f, 2.0f);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    // Borders
    ImVec2 winP = ImGui::GetWindowPos();
    float winH = ImGui::GetWindowHeight();
    dl->AddLine(ImVec2(winP.x + width - 1.0f, winP.y), ImVec2(winP.x + width - 1.0f, winP.y + winH), COL(T.border));
    dl->AddLine(ImVec2(winP.x, winP.y + winH - 1.0f), ImVec2(winP.x + width, winP.y + winH - 1.0f), COL(T.border));

    ImGui::End();
    ImGui::PopStyleVar();
}

void PropertiesPanel::drawPropertyEditor(const std::shared_ptr<Instance>& inst, const Engine::Reflection::PropertyDescriptor* prop) {
    // Unused in new custom drawlist mockup layout
}
