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

    ImGui::InvisibleButton(label, ImVec2(width, 24));
    if (ImGui::IsItemClicked()) open = !open;

    // Bottom border
    dl->AddLine(ImVec2(p.x, p.y + 23), ImVec2(p.x + width, p.y + 23), COL(T.border));

    // Arrow + Label
    const char* arrow = open ? "▼" : "▶";
    char buf[128];
    snprintf(buf, sizeof(buf), "%s %s", arrow, label);
    dl->AddText(ImVec2(p.x + 4, p.y + 5), IM_COL32(255,255,255,255), buf); // Headers are white in HTML

    if (rightLabel) {
        ImVec2 size = ImGui::CalcTextSize(rightLabel);
        dl->AddText(ImVec2(p.x + width - size.x - 4, p.y + 5), COL(T.textMuted), rightLabel);
    }

    return open;
}

static void PropRow(const char* lbl, const char* val, ImU32 valCol = 0) {
    auto& T = NexusTheme::instance();
    if (valCol == 0) valCol = COL(T.textPrimary);
    
    ImGui::Dummy(ImVec2(0, 24));
    ImVec2 p = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    dl->AddText(ImVec2(p.x + 8, p.y + 4), COL(T.textMuted), lbl);
    DrawTextRightAligned(dl, ImVec2(p.x, p.y + 4), w - 8, valCol, val);
}

static void Vec3Row(const char* lbl, const char* x, const char* y, const char* z) {
    auto& T = NexusTheme::instance();
    ImGui::Dummy(ImVec2(0, 24));
    ImVec2 p = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    dl->AddText(ImVec2(p.x + 8, p.y + 4), COL(T.textMuted), lbl);
    
    // Exact sizing to match the HTML layout
    float cw = (w - 100) / 3.0f;
    float curX = p.x + 100;

    DrawTextRightAligned(dl, ImVec2(curX, p.y + 4), cw, COLA(0xf87171, 1.0f), x); // text-red-400
    curX += cw;
    DrawTextRightAligned(dl, ImVec2(curX, p.y + 4), cw, COLA(0x4ade80, 1.0f), y); // text-green-400
    curX += cw;
    DrawTextRightAligned(dl, ImVec2(curX, p.y + 4), cw, COLA(0x60a5fa, 1.0f), z); // text-blue-400
}

static void SliderRow(const char* lbl, const char* valStr, float* val, float mn, float mx) {
    auto& T = NexusTheme::instance();
    ImGui::Dummy(ImVec2(0, 24));
    ImVec2 p = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;

    dl->AddText(ImVec2(p.x + 8, p.y + 4), COL(T.textMuted), lbl);

    float sliderW = w - 120.0f;
    float curX = p.x + 100.0f;

    // Background track
    dl->AddRectFilled(ImVec2(curX, p.y + 11), ImVec2(curX + sliderW, p.y + 14), COLA(0x242424, 1.0f), 1.5f);
    
    // Fill track
    float fillW = sliderW * (*val - mn) / (mx - mn);
    dl->AddRectFilled(ImVec2(curX, p.y + 11), ImVec2(curX + fillW, p.y + 14), COL(T.accent), 1.5f);

    // Thumb
    ImVec2 thumbCenter(curX + fillW, p.y + 12.5f);
    dl->AddCircleFilled(thumbCenter, 5.0f, IM_COL32_WHITE);
    dl->AddCircle(thumbCenter, 5.0f, COL(T.accent), 0, 2.0f);
    
    // Interactive part (invisible slider)
    ImGui::SetCursorScreenPos(ImVec2(curX, p.y+2));
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
    DrawTextRightAligned(dl, ImVec2(p.x, p.y + 4), w - 8, COL(T.textPrimary), valStr);
}

// ═══════════════════════════════════════════════════════════════════════════════
void PropertiesPanel::draw() {
    auto& T = NexusTheme::instance();

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoScrollbar); 

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    float       width = ImGui::GetWindowWidth();
    
    auto selected = SelectionManager::instance().getSelected();

    // ─────────────────────────────────────────────────────────────────────────
    // UNIFIED HEADER: ▼ PROPERTIES - BOX (GOLD)
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.panel);
    ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts->Fonts.Size > 1) ImGui::PushFont(io.Fonts->Fonts[1]); // Small Font
    
    ImGui::BeginChild("##PropHdr", ImVec2(width, 28), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 base = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(base.x, base.y + 27), ImVec2(base.x + width, base.y + 27), COL(T.border));

        float cx = base.x + 8.0f;
        dl->AddText(ImVec2(cx, base.y + 6.0f), COL(T.accent), "▼");
        cx += 16.0f;
        
        std::string title = "PROPERTIES - ";
        std::string objName = selected ? selected->name : "NONE";
        for (auto & c: objName) c = (char)toupper((unsigned char)c);
        title += objName;
        dl->AddText(ImVec2(cx, base.y + 6.0f), IM_COL32(255,255,255,255), title.c_str());

        // ID and Class badges on the right
        float rx = base.x + width - 8.0f;

        if (selected) {
            // Class badge (MeshPart)
            const char* cls = selected->getClassName().c_str();
            ImVec2 clsSize = ImGui::CalcTextSize(cls);
            float clsW = clsSize.x + 10.0f;
            rx -= clsW;
            dl->AddRectFilled(ImVec2(rx, base.y + 5.0f), ImVec2(rx + clsW, base.y + 21.0f), COLA(0x00d2ff, 0.1f), 4.0f);
            dl->AddRect(ImVec2(rx, base.y + 5.0f), ImVec2(rx + clsW, base.y + 21.0f), COLA(0x00d2ff, 0.3f), 4.0f);
            dl->AddText(ImVec2(rx + 5.0f, base.y + 6.0f), COL(T.accent), cls);

            rx -= 4.0f;

            // ID Badge
            const char* idStr = "ID: 0x8F3D";
            ImVec2 idSize = ImGui::CalcTextSize(idStr);
            float idW = idSize.x + 10.0f;
            rx -= idW;
            dl->AddRectFilled(ImVec2(rx, base.y + 5.0f), ImVec2(rx + idW, base.y + 21.0f), COL(T.panel), 4.0f);
            dl->AddRect(ImVec2(rx, base.y + 5.0f), ImVec2(rx + idW, base.y + 21.0f), COL(T.border), 4.0f);
            dl->AddText(ImVec2(rx + 5.0f, base.y + 6.0f), COL(T.textMuted), idStr);
        }
    }
    ImGui::EndChild();
    if (io.Fonts->Fonts.Size > 1) ImGui::PopFont();
    ImGui::PopStyleColor();

    if (!selected) {
        ImGui::SetCursorPos(ImVec2(12, 40));
        ImGui::TextDisabled("Nothing selected");
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // SCROLLABLE CONTENT (Darker background bg-studio-bg/60)
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, COLA(0x050505, 1.0f)); 
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8)); // Match the HTML p-3 roughly
    
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
        Vec3Row("Pivot Offset", "0.000","-0.500","0.000"); // It uses the same alignment in HTML
    }
    ImGui::Dummy(ImVec2(0, 8));

    // ─── 2. TAGS & LAYERS ────────────────────────────────────────────────────
    if (SectionHeader("Tags & Layers", nullptr, true)) {
        ImGui::Dummy(ImVec2(0, 34));
        ImVec2 p = ImGui::GetItemRectMin();
        ImDrawList* l = ImGui::GetWindowDrawList();
        
        // Match exact Tags from HTML
        const char* tags[] = {"Interactable", "LootBox", "Metallic_Heavy", "+"};
        float cx = p.x + 8.0f;
        for (int i=0; i<4; i++) {
            ImVec2 ts = ImGui::CalcTextSize(tags[i]);
            float bw = ts.x + 16.0f;
            
            ImU32 borderCol = (i == 2) ? COLA(0xFACC15, 0.6f) : COL(T.border); // Metallic_heavy has yellow border
            l->AddRectFilled(ImVec2(cx, p.y + 6.0f), ImVec2(cx + bw, p.y + 26.0f), COL(T.panel), 10.0f);
            l->AddRect(ImVec2(cx, p.y + 6.0f), ImVec2(cx + bw, p.y + 26.0f), borderCol, 10.0f);
            l->AddText(ImVec2(cx + 8.0f, p.y + 10.0f), (i==3) ? COL(T.textMuted) : (i==2 ? COLA(0xFACC15, 1.0f) : IM_COL32_WHITE), tags[i]);
            
            cx += bw + 8.0f;
        }

        PropRow("Collision Layer", "Layer 3 (Props) ⌄", IM_COL32_WHITE);
        PropRow("Collision Mask", "Player, Ground, Raycast", COL(T.accent));
    }
    ImGui::Dummy(ImVec2(0, 8));

    // ─── 3. MESH & GEOMETRY ──────────────────────────────────────────────────
    if (SectionHeader("Mesh & Geometry", "LOD 0 (12,400 Tris)")) {
        PropRow("Mesh Asset", "Gold_Box.mesh ⌄", IM_COL32_WHITE);
        PropRow("Collision Fidelity", "Precise Convex ⌄", IM_COL32_WHITE);
        
        static bool castShadow = true, recvShadow = true;
        ImGui::Dummy(ImVec2(0, 24));
        ImVec2 p1 = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddText(ImVec2(p1.x + 8, p1.y + 4), COL(T.textMuted), "Cast Shadows");
        ImGui::SetCursorScreenPos(ImVec2(p1.x + ImGui::GetContentRegionAvail().x - 32, p1.y + 4));
        if (DrawTogglePill("##cs", castShadow)) castShadow = !castShadow;

        ImGui::Dummy(ImVec2(0, 24));
        ImVec2 p2 = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddText(ImVec2(p2.x + 8, p2.y + 4), COL(T.textMuted), "Receive Shadows");
        ImGui::SetCursorScreenPos(ImVec2(p2.x + ImGui::GetContentRegionAvail().x - 32, p2.y + 4));
        if (DrawTogglePill("##rs", recvShadow)) recvShadow = !recvShadow;
    }
    ImGui::Dummy(ImVec2(0, 8));

    // ─── 4. MATERIAL / PBR SHADER ────────────────────────────────────────────
    if (SectionHeader("Gold (Shader)", "PBR Standard ⌄   Opaque ⌄")) {
        
        ImGui::Dummy(ImVec2(0, 75));
        ImVec2 p = ImGui::GetItemRectMin();
        ImDrawList* l = ImGui::GetWindowDrawList();
        
        float btnW = (width - 32.0f) / 4.0f;
        float curX = p.x + 8.0f;
        
        auto drawMapBtn = [&](const char* lbl, ImU32 bg, ImU32 textCol, bool hasIcon, bool active) {
            l->AddRectFilled(ImVec2(curX, p.y+4), ImVec2(curX+btnW-4, p.y+54), bg, 6.0f);
            l->AddRect(ImVec2(curX, p.y+4), ImVec2(curX+btnW-4, p.y+54), active ? COLA(0xFACC15, 0.5f) : COL(T.border), 6.0f);
            
            if (hasIcon) {
                // simple picture mockup
                l->AddRectFilled(ImVec2(curX + (btnW-4-12)*0.5f, p.y + 20), ImVec2(curX + (btnW-4-12)*0.5f + 12, p.y + 32), IM_COL32_WHITE, 2.0f);
                l->AddRectFilled(ImVec2(curX + (btnW-4-12)*0.5f + 2, p.y + 22), ImVec2(curX + (btnW-4-12)*0.5f + 10, p.y + 30), IM_COL32(50,200,100,255), 1.0f);
            } else if (active) {
                ImVec2 ts = ImGui::CalcTextSize("BASE");
                l->AddText(ImVec2(curX + (btnW-4-ts.x)*0.5f, p.y + 26 - ts.y*0.5f), textCol, "BASE");
            } else {
                // Emissive block is just dark
                l->AddRectFilled(ImVec2(curX + (btnW-4-12)*0.5f, p.y + 20), ImVec2(curX + (btnW-4-12)*0.5f + 12, p.y + 32), COL(T.bg), 2.0f);
            }

            // Radio button below
            l->AddCircle(ImVec2(curX + 12, p.y + 66), 3.0f, active ? IM_COL32_WHITE : COL(T.textMuted));
            if (active) l->AddCircleFilled(ImVec2(curX + 12, p.y + 66), 1.5f, IM_COL32_WHITE);
            
            ImVec2 ts = ImGui::CalcTextSize(lbl);
            l->AddText(ImVec2(curX + 20, p.y + 66 - ts.y*0.5f), active ? IM_COL32_WHITE : COL(T.textMuted), lbl);
            
            curX += btnW;
        };

        drawMapBtn("Base", COLA(0x854d0e, 0.4f), COLA(0xFACC15, 1.0f), false, true); // Active Base
        drawMapBtn("Normal", COL(T.panelHover), COL(T.textMuted), true, false);
        drawMapBtn("ARM", COL(T.panelHover), COL(T.textMuted), true, false);
        drawMapBtn("Emissive", COL(T.panel), COL(T.textMuted), false, false); // dark bg

        PropRow("Albedo Tint", "      ");
        ImVec2 rowMin = ImGui::GetItemRectMin();
        float rw = ImGui::GetContentRegionAvail().x;
        l->AddRectFilled(ImVec2(rowMin.x + rw - 36, rowMin.y+6), ImVec2(rowMin.x + rw - 8, rowMin.y+18), COLA(0xFACC15, 1.0f), 3.0f);
    }
    ImGui::Dummy(ImVec2(0, 8));

    // ─── 5. PHYSICS / RIGIDBODY ──────────────────────────────────────────────
    if (SectionHeader("Physics • Rigidbody", nullptr)) {
        static bool physEnabled = true;
        ImGui::Dummy(ImVec2(0, 24));
        ImVec2 p1 = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddText(ImVec2(p1.x + 8, p1.y + 4), COLA(0xf59e0b, 1.0f), "Simulation Enabled");
        ImGui::SetCursorScreenPos(ImVec2(p1.x + ImGui::GetContentRegionAvail().x - 32, p1.y + 4));
        if (DrawTogglePill("##phy", physEnabled)) physEnabled = !physEnabled;

        PropRow("Simulation Mode", "Dynamic ⌄", IM_COL32_WHITE);
        PropRow("Mass / Density", "45.2 kg (Auto)", IM_COL32_WHITE);
        PropRow("Center of Mass", "(0.0, 0.0, 0.0)", IM_COL32_WHITE);
        PropRow("Friction", "1.00", IM_COL32_WHITE);
        PropRow("Bounciness", "0.50", IM_COL32_WHITE);
        PropRow("Linear/Angular Damping", "0.05 / 0.05", IM_COL32_WHITE);
    }

    ImGui::Dummy(ImVec2(0, 16));
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
}

void PropertiesPanel::drawPropertyEditor(const std::shared_ptr<Instance>& inst, const Engine::Reflection::PropertyDescriptor* prop) {
    // Unused in new custom drawlist mockup layout
}
