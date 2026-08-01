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
#include "NexusTheme.h"
#include <string>
#include <any>
#include <map>

static ImU32 COL(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a) {
    return IM_COL32((hex>>16)&0xFF,(hex>>8)&0xFF,hex&0xFF,(uint8_t)(a*255));
}

// Draw text without any widgets
static void DrawTextRightAligned(ImDrawList* dl, ImVec2 pos, float width, ImU32 col, const char* text) {
    ImVec2 size = ImGui::CalcTextSize(text);
    dl->AddText(ImVec2(pos.x + width - size.x, pos.y), col, text);
}

// ─── Toggle Pill ─────────────────────────────────────────────────────────────
static bool DrawTogglePill(const char* id, bool value) {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = 24.0f, h = 12.0f;
    ImVec2 p = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton(id, ImVec2(w, h));
    bool clicked = ImGui::IsItemClicked();

    ImU32 bg = value ? COL(T.toggleOn) : IM_COL32(36,36,36,255);
    dl->AddRectFilled(p, ImVec2(p.x+w, p.y+h), bg, h*0.5f);
    float cx = value ? p.x + w - 6.0f : p.x + 6.0f;
    dl->AddCircleFilled(ImVec2(cx, p.y + h*0.5f), 4.0f, IM_COL32(255,255,255,255));

    if (value)
        dl->AddCircle(ImVec2(cx, p.y+h*0.5f), 5.0f, COLA(0x22c55e, 0.35f));

    return clicked;
}

static bool SectionHeader(const char* label, const char* rightLabel = nullptr, bool defaultOpen = true) {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float width = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();

    static std::map<std::string, bool> s_openState;
    if (s_openState.find(label) == s_openState.end()) {
        s_openState[label] = defaultOpen;
    }
    bool& open = s_openState[label];

    ImGui::InvisibleButton(label, ImVec2(width, 22));
    if (ImGui::IsItemClicked()) open = !open;

    // Bottom border
    dl->AddLine(ImVec2(p.x, p.y + 21), ImVec2(p.x + width, p.y + 21), COL(T.border));

    // Arrow + Label
    const char* arrow = open ? "▼" : "▶";
    char buf[128];
    snprintf(buf, sizeof(buf), "%s %s", arrow, label);
    dl->AddText(ImVec2(p.x + 4, p.y + 4), COL(T.textPrimary), buf);

    if (rightLabel) {
        ImVec2 size = ImGui::CalcTextSize(rightLabel);
        dl->AddText(ImVec2(p.x + width - size.x - 4, p.y + 4), COL(T.textMuted), rightLabel);
    }

    return open;
}

static void PropRow(const char* lbl, const char* val, ImU32 valCol = 0) {
    auto& T = NexusTheme::instance();
    if (valCol == 0) valCol = COL(T.textPrimary);
    
    ImGui::Dummy(ImVec2(0, 20));
    ImVec2 p = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    dl->AddText(ImVec2(p.x + 8, p.y + 3), COL(T.textMuted), lbl);
    DrawTextRightAligned(dl, ImVec2(p.x, p.y + 3), w - 8, valCol, val);
}

static void Vec3Row(const char* lbl, const char* x, const char* y, const char* z) {
    auto& T = NexusTheme::instance();
    ImGui::Dummy(ImVec2(0, 20));
    ImVec2 p = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    dl->AddText(ImVec2(p.x + 8, p.y + 3), COL(T.textMuted), lbl);
    
    float cw = (w - 100) / 3.0f; // split remaining space
    float curX = p.x + 100;

    DrawTextRightAligned(dl, ImVec2(curX, p.y + 3), cw, COLA(0xf87171, 1.0f), x); // text-red-400
    curX += cw;
    DrawTextRightAligned(dl, ImVec2(curX, p.y + 3), cw, COLA(0x4ade80, 1.0f), y); // text-green-400
    curX += cw;
    DrawTextRightAligned(dl, ImVec2(curX, p.y + 3), cw, COLA(0x60a5fa, 1.0f), z); // text-blue-400
}

static void SliderRow(const char* lbl, const char* valStr, float* val, float mn, float mx) {
    auto& T = NexusTheme::instance();
    ImGui::Dummy(ImVec2(0, 20));
    ImVec2 p = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    dl->AddText(ImVec2(p.x + 8, p.y + 3), COL(T.textMuted), lbl);

    // Instead of ImGui::SliderFloat, draw our own HTML slider mockup
    float sliderW = w - 120.0f;
    float curX = p.x + 100.0f;

    // Background track
    dl->AddRectFilled(ImVec2(curX, p.y + 8), ImVec2(curX + sliderW, p.y + 11), COLA(0x242424, 1.0f), 1.5f);
    
    // Fill track
    float fillW = sliderW * (*val - mn) / (mx - mn);
    dl->AddRectFilled(ImVec2(curX, p.y + 8), ImVec2(curX + fillW, p.y + 11), COL(T.accent), 1.5f);

    // Thumb
    ImVec2 thumbCenter(curX + fillW, p.y + 9.5f);
    dl->AddCircleFilled(thumbCenter, 5.0f, IM_COL32_WHITE);
    dl->AddCircle(thumbCenter, 5.0f, COL(T.accent), 0, 2.0f);
    
    // Thumb shadow
    dl->AddCircleFilled(thumbCenter, 7.0f, COLA(0x00d2ff, 0.3f));

    // Interactive part (invisible slider)
    ImGui::SetCursorScreenPos(ImVec2(curX, p.y));
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

    // Draw text value
    DrawTextRightAligned(dl, ImVec2(p.x, p.y + 3), w - 8, COL(T.textPrimary), valStr);
}

// ═══════════════════════════════════════════════════════════════════════════════
void PropertiesPanel::draw() {
    auto& T = NexusTheme::instance();

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoScrollbar); // We manage our own scroll area

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    float       width = ImGui::GetWindowWidth();

    // ─────────────────────────────────────────────────────────────────────────
    // HEADER (h=28)
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.panel);
    ImGui::BeginChild("##PropHdr", ImVec2(width, 28), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 base = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(base.x, base.y + 27),
                    ImVec2(base.x + width, base.y + 27), COL(T.border));

        ImGui::SetCursorPos(ImVec2(12, 6));
        ImGui::TextColored(T.textPrimary, "▼ PROPERTIES");

        // Action buttons
        ImGui::SetCursorPos(ImVec2(width - 48, 2));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Text, T.textMuted);
        
        if (ImGui::Button("+", ImVec2(24, 24))) { }
        ImGui::SameLine(0,0);
        if (ImGui::Button("•••", ImVec2(24, 24))) { }
        
        ImGui::PopStyleColor(2);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    auto selected = SelectionManager::instance().getSelected();
    if (!selected) {
        ImGui::SetCursorPos(ImVec2(12, 40));
        ImGui::TextDisabled("Nothing selected");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // BAŞLIK BANDİ (nesne adı + class badge + ID)
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.panel);
    ImGui::BeginChild("##PropTitle", ImVec2(width, 42), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 base = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(base.x, base.y + 41),
                    ImVec2(base.x + width, base.y + 41), COL(T.border));

        // Nesne Icon + Adı
        ImGui::SetCursorPos(ImVec2(12, 12));
        ImGui::TextColored(ImVec4(0.0f, 0.82f, 1.0f, 1.0f), "🧊"); // mock icon
        ImGui::SameLine();
        ImGui::TextColored(T.textPrimary, selected->name.c_str());

        // Class badge and ID
        float rightOff = 12.0f;
        
        const char* idStr = "ID: 0x8F3D";
        float idW = ImGui::CalcTextSize(idStr).x;
        dl->AddText(ImVec2(base.x + width - rightOff - idW, base.y + 12), COL(T.textMuted), idStr);
        rightOff += idW + 12.0f;

        const char* clsStr = selected->getClassName().c_str();
        float clsW = ImGui::CalcTextSize(clsStr).x;
        dl->AddRectFilled(ImVec2(base.x + width - rightOff - clsW - 8, base.y + 10),
                          ImVec2(base.x + width - rightOff + 4, base.y + 26),
                          COLA(0x242424, 1.0f), 4.0f);
        dl->AddText(ImVec2(base.x + width - rightOff - clsW - 2, base.y + 12), COL(T.textMuted), clsStr);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ─────────────────────────────────────────────────────────────────────────
    // SCROLLABLE CONTENT
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, COLA(0x0a0a0a, 1.0f)); // bg-studio-bg/60
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    
    ImGui::BeginChild("##PropScroll", ImVec2(0,0), false, 0);

    // ─── 1. TRANSFORM ────────────────────────────────────────────────────────
    if (SectionHeader("Transform", "World Space ⌄")) {
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
        PropRow("Pivot Offset", "0.000  -0.500  0.000");
    }

    // ─── 2. TAGS & LAYERS ────────────────────────────────────────────────────
    if (SectionHeader("Tags & Layers", nullptr, false)) {
        ImGui::Dummy(ImVec2(0, 24));
        ImVec2 p = ImGui::GetItemRectMin();
        ImDrawList* l = ImGui::GetWindowDrawList();
        
        // Tags mockup
        const char* tag1 = "Interactable";
        const char* tag2 = "Prop";
        float t1w = ImGui::CalcTextSize(tag1).x + 16;
        float t2w = ImGui::CalcTextSize(tag2).x + 16;
        
        l->AddRectFilled(ImVec2(p.x + 8, p.y), ImVec2(p.x + 8 + t1w, p.y + 18), COL(T.panel), 9.0f);
        l->AddRect(ImVec2(p.x + 8, p.y), ImVec2(p.x + 8 + t1w, p.y + 18), COL(T.border), 9.0f);
        l->AddText(ImVec2(p.x + 16, p.y + 2), COL(T.textPrimary), tag1);

        l->AddRectFilled(ImVec2(p.x + 12 + t1w, p.y), ImVec2(p.x + 12 + t1w + t2w, p.y + 18), COL(T.panel), 9.0f);
        l->AddRect(ImVec2(p.x + 12 + t1w, p.y), ImVec2(p.x + 12 + t1w + t2w, p.y + 18), COL(T.border), 9.0f);
        l->AddText(ImVec2(p.x + 20 + t1w, p.y + 2), COL(T.textPrimary), tag2);
    }

    // ─── 3. MESH & GEOMETRY ──────────────────────────────────────────────────
    if (SectionHeader("Mesh & Geometry", "LOD 0 (12k Tris)")) {
        PropRow("Mesh Asset", "Gold_Box.mesh ⌄");
        PropRow("Collision Fidelity", "Precise Convex ⌄");
        
        static bool castShadow = true, recvShadow = true;
        ImGui::Dummy(ImVec2(0, 20));
        ImVec2 p1 = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddText(ImVec2(p1.x + 8, p1.y + 3), COL(T.textMuted), "Cast Shadows");
        ImGui::SetCursorScreenPos(ImVec2(p1.x + ImGui::GetContentRegionAvail().x - 32, p1.y + 2));
        if (DrawTogglePill("##cs", castShadow)) castShadow = !castShadow;

        ImGui::Dummy(ImVec2(0, 20));
        ImVec2 p2 = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddText(ImVec2(p2.x + 8, p2.y + 3), COL(T.textMuted), "Receive Shadows");
        ImGui::SetCursorScreenPos(ImVec2(p2.x + ImGui::GetContentRegionAvail().x - 32, p2.y + 2));
        if (DrawTogglePill("##rs", recvShadow)) recvShadow = !recvShadow;
    }

    // ─── 4. MATERIAL / PBR SHADER ────────────────────────────────────────────
    if (SectionHeader("Gold (Shader)", "PBR Standard ⌄")) {
        PropRow("Albedo Tint", "RGBA(255, 215, 0)"); 
        static float alpha=1.0f, normal=0.2f, rough=0.1f, metal=0.85f, emiss=0.0f;
        SliderRow("Alpha (Opacity)", "100%", &alpha, 0,1);
        SliderRow("Normal Strength", "20%", &normal, 0,1);
        SliderRow("Roughness", "10%", &rough, 0,1);
        SliderRow("Metalness", "85%", &metal, 0,1);
        SliderRow("Emissive Intensity", "0.0 eV", &emiss, 0,5);
        PropRow("UV Tiling", "1.00   1.00");
    }

    // ─── 5. PHYSICS / RIGIDBODY ──────────────────────────────────────────────
    if (SectionHeader("Physics • Rigidbody", nullptr)) {
        static bool physEnabled = true;
        ImGui::Dummy(ImVec2(0, 20));
        ImVec2 p1 = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddText(ImVec2(p1.x + 8, p1.y + 3), COLA(0xf59e0b, 1.0f), "Simulation Enabled");
        ImGui::SetCursorScreenPos(ImVec2(p1.x + ImGui::GetContentRegionAvail().x - 32, p1.y + 2));
        if (DrawTogglePill("##phy", physEnabled)) physEnabled = !physEnabled;

        PropRow("Simulation Mode", "Dynamic ⌄");
        PropRow("Mass / Density", "45.2 kg (Auto)");
        PropRow("Center of Mass", "(0.0, 0.0, 0.0)");
        PropRow("Friction", "1.00");
        PropRow("Bounciness", "0.50");
        PropRow("Linear/Angular Damping", "0.05 / 0.05");
    }

    ImGui::Dummy(ImVec2(0, 12));
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
}

void PropertiesPanel::drawPropertyEditor(const std::shared_ptr<Instance>& inst, const Engine::Reflection::PropertyDescriptor* prop) {
    // Unused in new custom drawlist mockup layout
}
