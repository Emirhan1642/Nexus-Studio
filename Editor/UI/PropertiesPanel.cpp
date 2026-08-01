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

// ─── Renk kısayolları ───────────────────────────────────────────────────────
static ImU32 COL(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a) {
    return IM_COL32((hex>>16)&0xFF,(hex>>8)&0xFF,hex&0xFF,(uint8_t)(a*255));
}

// ─── Toggle Pill ─────────────────────────────────────────────────────────────
// HTML: w-6 h-3.5 = 24×14px, bg toggleOn/panel
static bool DrawTogglePill(const char* id, bool value) {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = 24.0f, h = 14.0f;
    ImVec2 p = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton(id, ImVec2(w, h));
    bool clicked = ImGui::IsItemClicked();

    ImU32 bg = value ? COL(T.toggleOn) : IM_COL32(36,36,36,255);
    dl->AddRectFilled(p, ImVec2(p.x+w, p.y+h), bg, h*0.5f);
    float cx = value ? p.x + w - 7.0f : p.x + 7.0f;
    dl->AddCircleFilled(ImVec2(cx, p.y + h*0.5f), 5.0f, IM_COL32(255,255,255,255));

    // Glow (açık)
    if (value)
        dl->AddCircle(ImVec2(cx, p.y+h*0.5f), 6.0f, COLA(0x22c55e, 0.35f));

    return clicked;
}

// ─── Section header ───────────────────────────────────────────────────────────
// HTML: border-b border-studio-border, font-semibold text-white
static bool SectionHeader(const char* label, bool* open) {
    auto& T = NexusTheme::instance();
    ImGui::PushStyleColor(ImGuiCol_Header,        COL(T.panel));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, COL(T.panelHover));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  COL(T.panelHover));
    ImGui::PushStyleColor(ImGuiCol_Text,          COL(T.textPrimary));

    ImGuiTreeNodeFlags f = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    bool ret = ImGui::CollapsingHeader(label, f);
    if (open) *open = ret;

    ImGui::PopStyleColor(4);

    // Bottom border (HTML: border-b border-studio-border)
    ImVec2 rMin = ImGui::GetItemRectMin();
    ImVec2 rMax = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(rMin.x, rMax.y), ImVec2(rMax.x, rMax.y), COL(T.border));

    return ret;
}

// ─── 2-col row (label + değer) ───────────────────────────────────────────────
static void PropRow(const char* lbl, const char* val,
                    ImVec4 valCol = ImVec4(-1,-1,-1,-1)) {
    auto& T = NexusTheme::instance();
    ImGui::TextColored(T.textMuted, "%s", lbl);
    ImGui::SameLine(120);
    if (valCol.x < 0) valCol = T.textPrimary;
    ImGui::TextColored(valCol, "%s", val);
}

// ─── XYZ satırı (kırmızı/yeşil/mavi renklendirilmiş) ────────────────────────
static void Vec3Row(const char* lbl, const char* x, const char* y, const char* z) {
    auto& T = NexusTheme::instance();
    ImGui::TextColored(T.textMuted, "%s", lbl);
    ImGui::SameLine(90); ImGui::TextColored(ImVec4(0.9f,0.3f,0.3f,1), "%s", x);
    ImGui::SameLine(0,8);ImGui::TextColored(ImVec4(0.3f,0.9f,0.3f,1), "%s", y);
    ImGui::SameLine(0,8);ImGui::TextColored(ImVec4(0.3f,0.5f,1.0f,1), "%s", z);
}

// ─── Slider satırı ───────────────────────────────────────────────────────────
static void SliderRow(const char* lbl, const char* valStr, float* val, float mn, float mx) {
    auto& T = NexusTheme::instance();
    ImGui::TextColored(T.textMuted, "%s", lbl);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80);
    ImGui::TextColored(T.accent, "%s", valStr);
    ImGui::SetNextItemWidth(-1);
    char id[64]; snprintf(id, sizeof(id), "##sl_%s", lbl);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, COL(T.accent));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(36,36,36,255));
    ImGui::SliderFloat(id, val, mn, mx, "");
    ImGui::PopStyleColor(2);
}

// ═══════════════════════════════════════════════════════════════════════════════
void PropertiesPanel::draw() {
    auto& T = NexusTheme::instance();

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Properties");

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    float       width = ImGui::GetWindowWidth();
    ImVec2      wPos  = ImGui::GetCursorScreenPos();

    // ─────────────────────────────────────────────────────────────────────────
    // HEADER (h=28)
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.panel);
    ImGui::BeginChild("##PropHdr", ImVec2(width, 28), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 base = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(base.x, base.y + 27),
                    ImVec2(base.x + width, base.y + 27), COL(T.border));

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(0,0));
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, T.bg);
        ImGui::PushStyleColor(ImGuiCol_Text,   T.textPrimary);
        ImGui::Button("##propTab", ImVec2(width, 28));
        ImGui::PopStyleColor(2);
        dl->AddLine(ImVec2(base.x, base.y + 1),
                    ImVec2(base.x + 90, base.y + 1), COL(T.accent), 2.0f);

        // Chevron + label + ID badge
        dl->AddText(ImVec2(base.x + 8, base.y + 7), COL(T.accent), "▼");
        dl->AddText(ImVec2(base.x + 22, base.y + 7), COL(T.textPrimary), "PROPERTIES");

        ImGui::PopStyleVar(2);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ─────────────────────────────────────────────────────────────────────────
    // Seçili nesne yoksa
    // ─────────────────────────────────────────────────────────────────────────
    auto selected = SelectionManager::instance().getSelected();
    if (!selected) {
        ImGui::BeginChild("##PropEmpty");
        ImGui::SetCursorPos(ImVec2(12, 16));
        ImGui::TextDisabled("Nothing selected");
        ImGui::EndChild();
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // BAŞLIK BANDİ (nesne adı + class badge + ID)
    // HTML: px-2.5, justify-between
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.panel);
    ImGui::BeginChild("##PropTitle", ImVec2(width, 32), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 base = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(base.x, base.y + 31),
                    ImVec2(base.x + width, base.y + 31), COL(T.border));

        // Accent arrow + name
        dl->AddText(ImVec2(base.x + 8, base.y + 8), COL(T.accent), "▼");
        std::string headerLabel = "PROPERTIES - " + selected->name;
        dl->AddText(ImVec2(base.x + 22, base.y + 8), COL(T.textPrimary), headerLabel.c_str());

        // ID badge sağda
        char idBuf[32]; snprintf(idBuf, sizeof(idBuf), "ID: 0x%04X", (unsigned)selected->getInstanceId() & 0xFFFF);
        float idW = ImGui::CalcTextSize(idBuf).x + 12;
        float clsW= ImGui::CalcTextSize(selected->getClassName().c_str()).x + 12;
        ImVec2 idMin  = {base.x + width - idW - clsW - 10, base.y + 6};
        ImVec2 clsMin = {base.x + width - clsW - 4, base.y + 6};

        // ID badge
        dl->AddRectFilled(idMin, {idMin.x+idW, idMin.y+18}, COL(T.panel), 4.0f);
        dl->AddRect(idMin, {idMin.x+idW, idMin.y+18}, COL(T.border), 4.0f);
        dl->AddText({idMin.x+6, idMin.y+2}, COL(T.textMuted), idBuf);

        // Class badge (accent)
        dl->AddRectFilled(clsMin, {clsMin.x+clsW, clsMin.y+18}, COLA(0x00d2ff,0.10f), 4.0f);
        dl->AddRect(clsMin, {clsMin.x+clsW, clsMin.y+18}, COLA(0x00d2ff,0.30f), 4.0f);
        dl->AddText({clsMin.x+6, clsMin.y+2}, COL(T.accent), selected->getClassName().c_str());
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ─────────────────────────────────────────────────────────────────────────
    // SCROLLABLE CONTENT
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, NexusTheme::HexColorAlpha(0x050505, 0.6f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 5));
    ImGui::BeginChild("##PropScroll", ImVec2(0,0), false, 0);
    ImGui::Dummy(ImVec2(0, 4));

    // ─── 1. TRANSFORM ────────────────────────────────────────────────────────
    if (SectionHeader("▼  Transform", nullptr)) {
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(T.textMuted, "World Space  ▾");
        ImGui::Spacing();

        auto* cls = Engine::Reflection::TypeRegistry::instance().find(selected->getClassName());
        bool hasPos = false, hasRot = false, hasSz = false;
        if (cls) {
            for (auto& prop : cls->properties) {
                if (prop.name == "Position" && prop.getter) {
                    auto v = std::any_cast<Engine::Math::Vector3>(prop.getter(selected.get()));
                    char xs[24],ys[24],zs[24];
                    snprintf(xs,sizeof(xs),"%.3f",v.x);
                    snprintf(ys,sizeof(ys),"%.3f",v.y);
                    snprintf(zs,sizeof(zs),"%.3f",v.z);
                    ImGui::SetCursorPosX(10); Vec3Row("Position", xs, ys, zs); hasPos=true;
                }
                if (prop.name == "Size" && prop.getter) {
                    auto v = std::any_cast<Engine::Math::Vector3>(prop.getter(selected.get()));
                    char xs[24],ys[24],zs[24];
                    snprintf(xs,sizeof(xs),"%.3f",v.x);
                    snprintf(ys,sizeof(ys),"%.3f",v.y);
                    snprintf(zs,sizeof(zs),"%.3f",v.z);
                    ImGui::SetCursorPosX(10); Vec3Row("Scale", xs, ys, zs); hasSz=true;
                }
            }
        }
        // Fallback sabit değerler (gerçek veri yoksa)
        if (!hasPos) { ImGui::SetCursorPosX(10); Vec3Row("Position","0.000","24.000","-12.000"); }
        ImGui::SetCursorPosX(10); Vec3Row("Rotation", "0.0°","23.0°","14.3°");
        if (!hasSz)  { ImGui::SetCursorPosX(10); Vec3Row("Scale","1.000","1.000","1.000"); }
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(T.textMuted, "Pivot Offset");
        ImGui::SameLine(90);
        ImGui::TextColored(T.textMuted, "0.000  -0.500  0.000");
        ImGui::Spacing();
    }

    // ─── 2. TAGS & LAYERS ────────────────────────────────────────────────────
    if (SectionHeader("▼  Tags & Layers", nullptr)) {
        ImGui::SetCursorPosX(10);
        // Chip-style tag listesi
        const char* tags[] = {"Interactable","LootBox","Metallic_Heavy"};
        for (auto t : tags) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            float tw = ImGui::CalcTextSize(t).x + 16;
            ImGui::GetWindowDrawList()->AddRectFilled(p, {p.x+tw,p.y+18},
                COL(T.panel), 12.0f);
            ImGui::GetWindowDrawList()->AddRect(p, {p.x+tw,p.y+18},
                COL(T.border), 12.0f);
            ImGui::GetWindowDrawList()->AddText({p.x+8,p.y+2},
                COL(T.textPrimary), t);
            ImGui::Dummy(ImVec2(tw, 20));
            ImGui::SameLine(0, 4);
        }
        // + butonu
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(p,{p.x+22,p.y+18},COL(T.panel),10.0f);
            ImGui::GetWindowDrawList()->AddRect(p,{p.x+22,p.y+18},COL(T.border),10.0f);
            ImGui::GetWindowDrawList()->AddText({p.x+6,p.y+2},COL(T.textMuted),"+");
            ImGui::Dummy(ImVec2(22,20));
        }
        ImGui::NewLine();
        ImGui::SetCursorPosX(10);
        PropRow("Collision Layer","Layer 3 (Props)  ▾");
        ImGui::SetCursorPosX(10);
        PropRow("Collision Mask","Player, Ground, Raycast", T.accent);
        ImGui::Spacing();
    }

    // ─── 3. MESH & GEOMETRY ──────────────────────────────────────────────────
    if (SectionHeader("▼  Mesh & Geometry", nullptr)) {
        // LOD badge sağda - header içinde değil, onun hemen altında
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            const char* lodLbl = "LOD 0 (12,400 Tris)";
            float lw = ImGui::CalcTextSize(lodLbl).x + 10;
            dl->AddText({p.x + width - lw - 12, p.y - 20}, COL(T.accent), lodLbl);
        }
        ImGui::SetCursorPosX(10); PropRow("Mesh Asset",       "Gold_Box.mesh  ▾");
        ImGui::SetCursorPosX(10); PropRow("Collision Fidelity","Precise Convex  ▾");

        // Toggle switches
        static bool castShadow = true, recvShadow = true;
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(T.textMuted, "Cast Shadows");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 26);
        if (DrawTogglePill("##cs", castShadow)) castShadow = !castShadow;

        ImGui::SetCursorPosX(10);
        ImGui::TextColored(T.textMuted, "Receive Shadows");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 26);
        if (DrawTogglePill("##rs", recvShadow)) recvShadow = !recvShadow;
        ImGui::Spacing();
    }

    // ─── 4. MATERIAL / PBR SHADER ────────────────────────────────────────────
    if (SectionHeader("▼  Gold (Shader)", nullptr)) {
        // PBR Standard + Opaque sağda
        {
            float rx = width - 130;
            ImVec2 base = ImGui::GetCursorScreenPos();
            dl->AddText({base.x - ImGui::GetScrollX() + rx,  base.y - 22},
                        COL(T.accent), "PBR Standard  ▾");
            dl->AddText({base.x - ImGui::GetScrollX() + rx + 90, base.y - 22},
                        COL(T.textMuted), "Opaque  ▾");
        }

        // Texture thumbnails (4 kutucuk: BASE, Normal, ARM, Emissive)
        ImGui::SetCursorPosX(10);
        struct TexSlot { const char* lbl; bool hasData; ImVec4 tint; };
        TexSlot slots[] = {
            {"BASE",   true,  ImVec4(0.7f,0.4f,0,0.5f)},
            {"Normal", false, ImVec4(0,0,0,0)},
            {"ARM",    false, ImVec4(0,0,0,0)},
            {"Emissive",false,ImVec4(0,0,0,0)},
        };
        for (int i = 0; i < 4; i++) {
            auto& sl = slots[i];
            ImVec2 p = ImGui::GetCursorScreenPos();
            float bw = (width - 20) / 4.0f - 4;
            float bh = 40.0f;
            if (sl.hasData)
                dl->AddRectFilled(p,{p.x+bw,p.y+bh}, COLA(0xB45309,0.3f), 4.0f);
            dl->AddRect(p,{p.x+bw,p.y+bh},
                sl.hasData ? COLA(0xB45309,0.5f) : COL(T.border), 4.0f);
            // Label ortada
            const char* slLbl = sl.lbl;
            float tw = ImGui::CalcTextSize(slLbl).x;
            dl->AddText({p.x+(bw-tw)*0.5f, p.y+13},
                sl.hasData ? COLA(0xFCD34D,1.0f) : COL(T.textMuted), slLbl);
            ImGui::Dummy(ImVec2(bw, bh));
            if (i < 3) ImGui::SameLine(0, 4);
        }
        ImGui::Spacing();
        ImGui::SetCursorPosX(10);
        PropRow("Albedo Tint", ""); // renk kare sonra
        {
            ImVec2 p = ImGui::GetItemRectMax();
            p.x -= 28; p.y -= 18;
            dl->AddRectFilled(p, {p.x+20,p.y+10}, COLA(0xFBBF24,1.0f), 2.0f);
            dl->AddRect(p, {p.x+20,p.y+10}, COL(T.border), 2.0f);
        }
        // Sliderlar
        static float alpha=1.0f, normal=0.2f, rough=0.1f, metal=0.85f, emiss=0.0f;
        ImGui::SetCursorPosX(10); SliderRow("Alpha (Opacity)",   "100%", &alpha,  0,1);
        ImGui::SetCursorPosX(10); SliderRow("Normal Strength",   "20%",  &normal, 0,1);
        ImGui::SetCursorPosX(10); SliderRow("Roughness",         "10%",  &rough,  0,1);
        ImGui::SetCursorPosX(10); SliderRow("Metalness",         "85%",  &metal,  0,1);
        ImGui::SetCursorPosX(10); SliderRow("Emissive Intensity","0.0 eV",&emiss, 0,5);

        ImGui::SetCursorPosX(10);
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y),
            ImVec2(ImGui::GetCursorScreenPos().x+width-20, ImGui::GetCursorScreenPos().y),
            COLA(0x242424,0.6f));
        ImGui::Dummy(ImVec2(0,3));
        ImGui::SetCursorPosX(10); PropRow("UV Tiling (X, Y)","1.00   1.00");
        ImGui::Spacing();
    }

    // ─── 5. PHYSICS / RIGIDBODY ──────────────────────────────────────────────
    // HTML: from-orange-500/15 border-orange-500/40
    {
        ImVec2 physStart = ImGui::GetCursorScreenPos();
        // Arkaplan gradient (AddRectFilledMultiColor — öne çizilecek)

        if (SectionHeader("▼  Physics • Rigidbody", nullptr)) {
            // Gradient overlay (after header)
            ImVec2 contentStart = ImGui::GetCursorScreenPos();

            ImGui::SetCursorPosX(10);
            // Physics toggle (yeşil, glow)
            static bool physEnabled = true;
            ImGui::TextColored(ImVec4(0.96f,0.62f,0.07f,1), "Simulation Enabled");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 26);
            if (DrawTogglePill("##phy", physEnabled)) physEnabled = !physEnabled;

            ImGui::SetCursorPosX(10); PropRow("Simulation Mode",   "Dynamic  ▾");
            ImGui::SetCursorPosX(10);
            ImGui::TextColored(T.textMuted, "Mass / Density");
            ImGui::SameLine(120);
            ImGui::TextColored(ImVec4(0.96f,0.7f,0.3f,1), "45.2 kg (Auto)");

            ImGui::SetCursorPosX(10); PropRow("Center of Mass",         "(0.0, 0.0, 0.0)");
            ImGui::SetCursorPosX(10); PropRow("Friction",               "1.00");
            ImGui::SetCursorPosX(10); PropRow("Bounciness (Restitution)","0.50");
            ImGui::SetCursorPosX(10); PropRow("Linear / Angular Damping","0.05 / 0.05");
            ImGui::SetCursorPosX(10); PropRow("Gravity Scale",          "1.0x");

            ImGui::Dummy(ImVec2(0,2));
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(ImGui::GetCursorScreenPos().x+10, ImGui::GetCursorScreenPos().y),
                ImVec2(ImGui::GetCursorScreenPos().x+width-10, ImGui::GetCursorScreenPos().y),
                COLA(0xF97316, 0.20f));
            ImGui::Dummy(ImVec2(0,3));

            static bool ccd = true;
            ImGui::SetCursorPosX(10);
            ImGui::TextColored(T.textMuted, "Continuous Collision (CCD)");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 26);
            if (DrawTogglePill("##ccd", ccd)) ccd = !ccd;

            // Gradient overlay RENDER (hemen kapandı)
            ImVec2 contentEnd = ImGui::GetCursorScreenPos();
            contentEnd.x = physStart.x + width;
            // Dört köşe rengi: sol/sağ turuncu-şeffaf
            ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                physStart, contentEnd,
                COLA(0xF97316, 0.15f), COLA(0xF97316, 0.0f),
                COLA(0xF97316, 0.0f),  COLA(0xF97316, 0.15f));
            // Border
            ImGui::GetWindowDrawList()->AddRect(physStart, contentEnd, COLA(0xF97316, 0.4f), 4.0f);
        }
        ImGui::Spacing();
    }

    // ─── 6. COLLIDER & BOUNDS ────────────────────────────────────────────────
    if (SectionHeader("▼  Collider & Bounds", nullptr)) {
        {
            ImVec2 base = ImGui::GetCursorScreenPos();
            dl->AddText({base.x - ImGui::GetScrollX() + width - 120, base.y - 22},
                        COL(T.accent), "Box Collider  ▾");
        }
        ImGui::SetCursorPosX(10); PropRow("Center Offset",               "0.0   0.0   0.0");
        ImGui::SetCursorPosX(10); PropRow("Extents (W/H/D)",             "1.2   1.2   1.2");

        static bool isTrigger = false;
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(T.textMuted, "Is Trigger (Pass-Through)");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 26);
        if (DrawTogglePill("##trig", isTrigger)) isTrigger = !isTrigger;

        ImGui::SetCursorPosX(10); PropRow("Physics Material","Heavy_Metal.phys  ▾");
        ImGui::Spacing();
    }

    // ─── 7. NETWORK / REPLICATION ────────────────────────────────────────────
    if (SectionHeader("▼  Network • Replication", nullptr)) {
        {
            ImVec2 base = ImGui::GetCursorScreenPos();
            dl->AddText({base.x - ImGui::GetScrollX() + width - 100, base.y - 22},
                        COL(T.toggleOn), "Synced (60Hz)");
        }
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(T.textMuted, "Network Authority");
        ImGui::SameLine(120);
        ImGui::TextColored(T.accent, "Server Authoritative  ▾");

        static bool csp = true;
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(T.textMuted, "Client-Side Prediction");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 26);
        if (DrawTogglePill("##csp", csp)) csp = !csp;

        ImGui::SetCursorPosX(10); PropRow("Replication Priority","High (2.0)");
        ImGui::Spacing();
    }

    // ─── Reflection tabanlı EK ÖZELLİKLER ───────────────────────────────────
    auto* clsDesc = Engine::Reflection::TypeRegistry::instance().find(selected->getClassName());
    if (clsDesc && !clsDesc->properties.empty()) {
        if (SectionHeader("▼  Custom Properties", nullptr)) {
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6,4));
            if (ImGui::BeginTable("##reflTable", 2,
                    ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Name",  ImGuiTableColumnFlags_WidthFixed, 120);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                for (auto& prop : clsDesc->properties)
                    drawPropertyEditor(selected, &prop);
                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
        }
    }

    ImGui::Dummy(ImVec2(0, 12));
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
}

// ─── Reflection property editor ─────────────────────────────────────────────
void PropertiesPanel::drawPropertyEditor(
    const std::shared_ptr<Instance>& inst,
    const Engine::Reflection::PropertyDescriptor* prop)
{
    auto& T = NexusTheme::instance();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(T.textMuted, "%s", prop->name.c_str());
    ImGui::TableNextColumn();
    ImGui::PushID(prop->name.c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::BeginDisabled(prop->readOnly);

    std::any cur = prop->getter(inst.get());

    if (cur.type() == typeid(std::string)) {
        std::string v = std::any_cast<std::string>(cur);
        char buf[256]; strncpy(buf, v.c_str(), sizeof(buf));
        if (ImGui::InputText("##v", buf, sizeof(buf))) {
            UndoStack::instance().pushPropertyChangeCommand(inst, prop->name, cur, std::string(buf));
            prop->setter(inst.get(), std::string(buf));
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_GUID")) {
                Engine::Assets::AssetGuid g = *(Engine::Assets::AssetGuid*)pl->Data;
                const auto* m = Engine::Assets::AssetDatabase::instance().find(g);
                if (m) {
                    UndoStack::instance().pushPropertyChangeCommand(inst,prop->name,cur,m->relativePath);
                    prop->setter(inst.get(), m->relativePath);
                    Engine::Assets::AssetDependencyTracker::instance().registerUsage(g, inst->getInstanceId());
                }
            }
            ImGui::EndDragDropTarget();
        }
    }
    else if (cur.type() == typeid(float)) {
        float v = std::any_cast<float>(cur);
        if (ImGui::DragFloat("##v",&v,0.1f)) {
            UndoStack::instance().pushPropertyChangeCommand(inst,prop->name,cur,v);
            prop->setter(inst.get(),v);
        }
    }
    else if (cur.type() == typeid(Engine::Math::Vector3)) {
        Engine::Math::Vector3 v = std::any_cast<Engine::Math::Vector3>(cur);
        float arr[3] = {v.x,v.y,v.z};
        if (ImGui::DragFloat3("##v",arr,0.1f)) {
            Engine::Math::Vector3 nv{arr[0],arr[1],arr[2]};
            UndoStack::instance().pushPropertyChangeCommand(inst,prop->name,cur,nv);
            prop->setter(inst.get(),nv);
        }
    }
    else if (cur.type() == typeid(bool)) {
        bool v = std::any_cast<bool>(cur);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##tb", ImVec2(24,14));
        if (ImGui::IsItemClicked()) {
            UndoStack::instance().pushPropertyChangeCommand(inst,prop->name,cur,!v);
            prop->setter(inst.get(),!v);
        }
        // Toggle pill
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 bg = v ? COL(T.toggleOn) : IM_COL32(36,36,36,255);
        dl->AddRectFilled(p,{p.x+24,p.y+14},bg,7.0f);
        dl->AddCircleFilled({v?p.x+17:p.x+7, p.y+7},5.0f,IM_COL32(255,255,255,255));
    }
    else if (cur.type() == typeid(int)) {
        int v = std::any_cast<int>(cur);
        if (ImGui::DragInt("##v",&v))
            prop->setter(inst.get(),v);
    }

    ImGui::EndDisabled();
    ImGui::PopID();
}
