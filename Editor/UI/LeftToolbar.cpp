#include "LeftToolbar.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "IconRegistry.h"
#include "NexusTheme.h"
#include "EditorLayout.h"

// ─── Araç enum ──────────────────────────────────────────────────────────────
enum class Tool { Select, Move, Rotate, Scale };
static Tool  s_currentTool = Tool::Move; // HTML'de Move aktif geliyor
static bool  s_gridSnap    = true;       // HTML'de snap aktif

static ImU32 COL(const ImVec4& v)            { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float alpha)  {
    return IM_COL32((hex>>16)&0xFF,(hex>>8)&0xFF,hex&0xFF,(uint8_t)(alpha*255));
}

// ─── Inline SVG fallback ikonları (DrawList primitifleri) ──────────────────
// HTML'deki SVG path verilerini yakın geometrilerle temsil ediyoruz.

static void DrawIcon_Select(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    // Cursor arrow: ▶ benzeri ok (path: M7 2l12 11.2...)
    ImVec2 pts[4] = {
        {c.x - s*0.3f, c.y - s*0.5f},
        {c.x + s*0.5f, c.y},
        {c.x - s*0.1f, c.y + 0.1f},
        {c.x - s*0.3f, c.y + s*0.5f}
    };
    dl->AddPolyline(pts, 4, col, ImDrawFlags_None, 1.5f);
    dl->AddTriangleFilled(pts[0], pts[1], {c.x - s*0.1f, c.y + 0.1f}, col);
}

static void DrawIcon_Move(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    // 4 yönlü ok
    float h = s * 0.45f, t = s * 0.2f;
    // Yukarı ok
    dl->AddTriangleFilled({c.x,c.y-h},{c.x-t,c.y-h+t*1.2f},{c.x+t,c.y-h+t*1.2f},col);
    dl->AddRectFilled({c.x-t*0.5f,c.y-h+t*1.2f},{c.x+t*0.5f,c.y},col);
    // Aşağı ok
    dl->AddTriangleFilled({c.x,c.y+h},{c.x-t,c.y+h-t*1.2f},{c.x+t,c.y+h-t*1.2f},col);
    dl->AddRectFilled({c.x-t*0.5f,c.y},{c.x+t*0.5f,c.y+h-t*1.2f},col);
    // Sol ok
    dl->AddTriangleFilled({c.x-h,c.y},{c.x-h+t*1.2f,c.y-t},{c.x-h+t*1.2f,c.y+t},col);
    dl->AddRectFilled({c.x-h+t*1.2f,c.y-t*0.5f},{c.x,c.y+t*0.5f},col);
    // Sağ ok
    dl->AddTriangleFilled({c.x+h,c.y},{c.x+h-t*1.2f,c.y-t},{c.x+h-t*1.2f,c.y+t},col);
    dl->AddRectFilled({c.x,c.y-t*0.5f},{c.x+h-t*1.2f,c.y+t*0.5f},col);
}

static void DrawIcon_Rotate(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    // Dairesel ok (sadece arc + ok ucu yaklaşımı)
    dl->AddCircle(c, s*0.42f, col, 24, 1.5f);
    // Ok ucu (sağ alt)
    ImVec2 tip = {c.x + s*0.42f, c.y};
    dl->AddTriangleFilled({tip.x, tip.y},{tip.x-s*0.18f,tip.y-s*0.18f},{tip.x+s*0.02f,tip.y-s*0.22f},col);
}

static void DrawIcon_Scale(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    // Sağ üst ok (scale out)
    float h = s * 0.4f;
    dl->AddLine({c.x-h,c.y+h},{c.x+h,c.y-h},col,1.5f);
    dl->AddTriangleFilled({c.x+h,c.y-h},{c.x+h-s*0.2f,c.y-h},{c.x+h,c.y-h+s*0.2f},col);
    // İç kare
    dl->AddRect({c.x-h,c.y-h*0.3f},{c.x+h*0.3f,c.y+h},col,0,0,1.2f);
}

static void DrawIcon_Snap(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    // Grid benzeri 9 nokta
    for (int row = 0; row < 3; row++)
        for (int col2 = 0; col2 < 3; col2++)
            dl->AddCircleFilled({c.x + (col2-1)*s*0.3f, c.y + (row-1)*s*0.3f}, 2.0f, col);
    // Alt çizgi (snap aktif sembolü)
    dl->AddLine({c.x-s*0.4f, c.y+s*0.48f},{c.x+s*0.4f,c.y+s*0.48f},col,1.5f);
}

static void DrawIcon_Folder(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    float w = s*0.8f, h = s*0.55f;
    dl->AddRectFilled({c.x-w*0.5f,c.y-h*0.2f},{c.x+w*0.5f,c.y+h*0.8f},col,2.0f);
    dl->AddRectFilled({c.x-w*0.5f,c.y-h*0.6f},{c.x-w*0.1f,c.y-h*0.2f},col,2.0f);
}

static void DrawIcon_Material(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    // Nota simgesi (material = müzik/shader grafik)
    dl->AddCircleFilled({c.x-s*0.15f, c.y+s*0.35f}, s*0.2f, col);
    dl->AddRectFilled({c.x-s*0.05f,c.y-s*0.5f},{c.x+s*0.05f,c.y+s*0.35f},col);
    dl->AddRectFilled({c.x-s*0.05f,c.y-s*0.5f},{c.x+s*0.45f,c.y-s*0.3f},col);
}

static void DrawIcon_Light(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    // Ampul
    dl->AddCircle(c, s*0.33f, col, 16, 1.5f);
    dl->AddRectFilled({c.x-s*0.15f,c.y+s*0.33f},{c.x+s*0.15f,c.y+s*0.5f},col,1.0f);
    dl->AddLine({c.x-s*0.15f,c.y+s*0.5f},{c.x+s*0.15f,c.y+s*0.5f},col,1.5f);
}

static void DrawIcon_Settings(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    // Dişli çark yaklaşımı
    dl->AddCircle(c, s*0.22f, col, 12, 1.5f);
    int teeth = 6;
    for (int i = 0; i < teeth; i++) {
        float angle = i * (3.14159265f * 2.0f / teeth);
        float ca = cosf(angle), sa = sinf(angle);
        dl->AddLine({c.x + ca*s*0.28f, c.y + sa*s*0.28f},
                    {c.x + ca*s*0.5f,  c.y + sa*s*0.5f}, col, 2.0f);
    }
}

// ─── Ana çizim ──────────────────────────────────────────────────────────────

void LeftToolbar::draw() {
    if (!EditorLayout::instance().showLeftToolbar) return;

    auto& T = NexusTheme::instance();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.panel);

    ImGui::Begin("##LeftToolbar", nullptr,
        ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoNav       | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    float       winW = ImGui::GetWindowWidth();
    float       btnS = 32.0f;   // buton boyutu (HTML: p-1.5 = ~28px, biz 32 aldık)
    float       offX = (winW - btnS) * 0.5f;

    // ── Araç butonu çizici ────────────────────────────────────────────────
    auto drawToolBtn = [&](
        Tool toolType,
        const char* iconKey,
        void(*drawFallback)(ImDrawList*, ImVec2, float, ImU32),
        const char* tooltip
    ) {
        bool isActive = (s_currentTool == toolType);
        ImGui::SetCursorPosX(offX);

        // Arka plan rengi
        ImU32 bgCol = isActive
            ? COLA(0x00d2ff, 0.15f)        // accent/15 (HTML: bg-studio-accent/15)
            : IM_COL32(0, 0, 0, 0);

        // InvisibleButton + DrawList yaklaşımı — hover/click kontrolü
        ImVec2 bMin = ImGui::GetCursorScreenPos();
        ImVec2 bMax = ImVec2(bMin.x + btnS, bMin.y + btnS);

        ImGui::InvisibleButton(tooltip, ImVec2(btnS, btnS));
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();

        if (clk) s_currentTool = toolType;

        // Arka plan
        if (hov && !isActive) bgCol = COL(T.panelHover);
        if (bgCol) dl->AddRectFilled(bMin, bMax, bgCol, 6.0f);

        // Aktif kenarlık
        if (isActive) {
            dl->AddRect(bMin, bMax, COLA(0x00d2ff, 0.5f), 6.0f, 0, 1.0f);
        }

        // İkon: önce registry, sonra fallback DrawList
        ImVec2 center = {(bMin.x + bMax.x)*0.5f, (bMin.y + bMax.y)*0.5f};
        ImTextureID tex = IconRegistry::instance().get(iconKey);
        ImU32 iconCol = isActive
            ? COL(T.accent)
            : (hov ? COL(T.textPrimary) : COL(T.textMuted));

        if (tex) {
            float is = btnS * 0.5f;
            dl->AddImage(tex,
                {center.x - is*0.5f, center.y - is*0.5f},
                {center.x + is*0.5f, center.y + is*0.5f},
                {0,0},{1,1}, iconCol);
        } else {
            drawFallback(dl, center, btnS * 0.35f, iconCol);
        }

        if (hov) ImGui::SetTooltip("%s", tooltip);
    };

    // ── Dönüşüm araçları ─────────────────────────────────────────────────
    drawToolBtn(Tool::Select, "icon_cursor", DrawIcon_Select, "Select Tool (Q)");
    drawToolBtn(Tool::Move,   "icon_move",   DrawIcon_Move,   "Move Tool (W)");
    drawToolBtn(Tool::Rotate, "icon_rotate", DrawIcon_Rotate, "Rotate Tool (E)");
    drawToolBtn(Tool::Scale,  "icon_scale",  DrawIcon_Scale,  "Scale Tool (R)");

    // ── Snap toggle ───────────────────────────────────────────────────────
    {
        ImGui::SetCursorPosX(offX);
        ImVec2 bMin = ImGui::GetCursorScreenPos();
        ImVec2 bMax = ImVec2(bMin.x + btnS, bMin.y + btnS);

        ImGui::InvisibleButton("##snap", ImVec2(btnS, btnS));
        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) s_gridSnap = !s_gridSnap;

        ImU32 snapCol = s_gridSnap ? COL(T.toggleOn) : COL(T.textMuted);
        if (hov) dl->AddRectFilled(bMin, bMax, COL(T.panelHover), 6.0f);

        ImVec2 c = {(bMin.x+bMax.x)*0.5f,(bMin.y+bMax.y)*0.5f};
        ImTextureID tex = IconRegistry::instance().get("icon_snap");
        if (tex) {
            float is = btnS * 0.5f;
            dl->AddImage(tex,{c.x-is*0.5f,c.y-is*0.5f},{c.x+is*0.5f,c.y+is*0.5f},{0,0},{1,1},snapCol);
        } else {
            DrawIcon_Snap(dl, c, btnS * 0.35f, snapCol);
        }
        if (hov) ImGui::SetTooltip("Grid Snap • 1m / 15°  [%s]", s_gridSnap ? "Active" : "Off");
    }

    // ── Ayırıcı çizgi ─────────────────────────────────────────────────────
    ImGui::Dummy(ImVec2(winW, 6));
    {
        ImVec2 sep = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(sep.x + offX*0.5f,      sep.y),
                    ImVec2(sep.x + winW - offX*0.5f, sep.y),
                    COL(T.border));
    }
    ImGui::Dummy(ImVec2(winW, 6));

    // ── Editör kısayolları ────────────────────────────────────────────────

    // Asset Manager (HTML: bg-studio-accent/10, border accent/40)
    {
        ImGui::SetCursorPosX(offX);
        ImVec2 bMin = ImGui::GetCursorScreenPos();
        ImVec2 bMax = ImVec2(bMin.x + btnS, bMin.y + btnS);
        ImGui::InvisibleButton("##ast", ImVec2(btnS, btnS));
        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) EditorLayout::instance().showAssetBrowser = !EditorLayout::instance().showAssetBrowser;

        dl->AddRectFilled(bMin, bMax, COLA(0x00d2ff, 0.10f), 6.0f);
        dl->AddRect(bMin, bMax, COLA(0x00d2ff, 0.40f), 6.0f, 0, 1.0f);
        ImVec2 c = {(bMin.x+bMax.x)*0.5f,(bMin.y+bMax.y)*0.5f};
        ImTextureID tex = IconRegistry::instance().get("icon_folder");
        ImU32 ic = COL(T.accent);
        if (tex) {
            float is = btnS*0.5f;
            dl->AddImage(tex,{c.x-is*0.5f,c.y-is*0.5f},{c.x+is*0.5f,c.y+is*0.5f},{0,0},{1,1},ic);
        } else {
            DrawIcon_Folder(dl,c,btnS*0.35f,ic);
        }
        if (hov) ImGui::SetTooltip("Asset Manager / Content Drawer");
        ImGui::Dummy(ImVec2(winW, 2));
    }

    // Material Editor (HTML: bg-studio-panelHover, border border-studio-border)
    {
        ImGui::SetCursorPosX(offX);
        ImVec2 bMin = ImGui::GetCursorScreenPos();
        ImVec2 bMax = ImVec2(bMin.x + btnS, bMin.y + btnS);
        ImGui::InvisibleButton("##mat", ImVec2(btnS, btnS));
        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) EditorLayout::instance().showMaterialEditor = !EditorLayout::instance().showMaterialEditor;

        dl->AddRectFilled(bMin, bMax, hov ? COL(T.panelHover) : COLA(0x171717, 1.0f), 6.0f);
        dl->AddRect(bMin, bMax, COL(T.border), 6.0f, 0, 1.0f);
        ImVec2 c = {(bMin.x+bMax.x)*0.5f,(bMin.y+bMax.y)*0.5f};
        ImTextureID tex = IconRegistry::instance().get("icon_material");
        ImU32 ic = COL(T.textPrimary);
        if (tex) {
            float is = btnS*0.5f;
            dl->AddImage(tex,{c.x-is*0.5f,c.y-is*0.5f},{c.x+is*0.5f,c.y+is*0.5f},{0,0},{1,1},ic);
        } else {
            DrawIcon_Material(dl,c,btnS*0.35f,ic);
        }
        if (hov) ImGui::SetTooltip("Material Editor [Node Graph]");
        ImGui::Dummy(ImVec2(winW, 2));
    }

    // Lighting
    {
        ImGui::SetCursorPosX(offX);
        ImVec2 bMin = ImGui::GetCursorScreenPos();
        ImVec2 bMax = ImVec2(bMin.x + btnS, bMin.y + btnS);
        ImGui::InvisibleButton("##light", ImVec2(btnS, btnS));
        bool hov = ImGui::IsItemHovered();

        if (hov) dl->AddRectFilled(bMin, bMax, COL(T.panelHover), 6.0f);
        ImVec2 c = {(bMin.x+bMax.x)*0.5f,(bMin.y+bMax.y)*0.5f};
        ImTextureID tex = IconRegistry::instance().get("icon_light");
        ImU32 ic = hov ? COL(T.textPrimary) : COL(T.textMuted);
        if (tex) {
            float is = btnS*0.5f;
            dl->AddImage(tex,{c.x-is*0.5f,c.y-is*0.5f},{c.x+is*0.5f,c.y+is*0.5f},{0,0},{1,1},ic);
        } else {
            DrawIcon_Light(dl,c,btnS*0.35f,ic);
        }
        if (hov) ImGui::SetTooltip("Lighting & Lightmaps");
    }

    // ── Project Settings (alt) ────────────────────────────────────────────
    {
        float posY = ImGui::GetWindowHeight() - btnS - 10.0f;
        if (posY > ImGui::GetCursorPosY())
            ImGui::SetCursorPosY(posY);
        ImGui::SetCursorPosX(offX);

        ImVec2 bMin = ImGui::GetCursorScreenPos();
        ImVec2 bMax = ImVec2(bMin.x + btnS, bMin.y + btnS);
        ImGui::InvisibleButton("##settings", ImVec2(btnS, btnS));
        bool hov = ImGui::IsItemHovered();
        if (hov) dl->AddRectFilled(bMin, bMax, COL(T.panelHover), 6.0f);

        ImVec2 c = {(bMin.x+bMax.x)*0.5f,(bMin.y+bMax.y)*0.5f};
        ImTextureID tex = IconRegistry::instance().get("icon_settings");
        ImU32 ic = hov ? COL(T.textPrimary) : COL(T.textMuted);
        if (tex) {
            float is = btnS*0.5f;
            dl->AddImage(tex,{c.x-is*0.5f,c.y-is*0.5f},{c.x+is*0.5f,c.y+is*0.5f},{0,0},{1,1},ic);
        } else {
            DrawIcon_Settings(dl,c,btnS*0.35f,ic);
        }
        if (hov) ImGui::SetTooltip("Project Settings");
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}
