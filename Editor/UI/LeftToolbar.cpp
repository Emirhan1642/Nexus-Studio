#include "LeftToolbar.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "IconRegistry.h"
#include "NexusTheme.h"
#include "EditorLayout.h"

enum class Tool { Select, Move, Rotate, Scale };
static Tool  s_currentTool = Tool::Move;
static bool  s_gridSnap    = true;

static ImU32 COL(const ImVec4& v)            { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float alpha)  {
    return IM_COL32((hex>>16)&0xFF,(hex>>8)&0xFF,hex&0xFF,(uint8_t)(alpha*255));
}

// ─── Inline SVG fallback ikonları ─────────────────────────────────────────

static void DrawIcon_Select(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
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
    float h = s * 0.45f, t = s * 0.2f;
    dl->AddTriangleFilled({c.x,c.y-h},{c.x-t,c.y-h+t*1.2f},{c.x+t,c.y-h+t*1.2f},col);
    dl->AddRectFilled({c.x-t*0.5f,c.y-h+t*1.2f},{c.x+t*0.5f,c.y},col);
    dl->AddTriangleFilled({c.x,c.y+h},{c.x-t,c.y+h-t*1.2f},{c.x+t,c.y+h-t*1.2f},col);
    dl->AddRectFilled({c.x-t*0.5f,c.y},{c.x+t*0.5f,c.y+h-t*1.2f},col);
    dl->AddTriangleFilled({c.x-h,c.y},{c.x-h+t*1.2f,c.y-t},{c.x-h+t*1.2f,c.y+t},col);
    dl->AddRectFilled({c.x-h+t*1.2f,c.y-t*0.5f},{c.x,c.y+t*0.5f},col);
    dl->AddTriangleFilled({c.x+h,c.y},{c.x+h-t*1.2f,c.y-t},{c.x+h-t*1.2f,c.y+t},col);
    dl->AddRectFilled({c.x,c.y-t*0.5f},{c.x+h-t*1.2f,c.y+t*0.5f},col);
}

static void DrawIcon_Rotate(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddCircle(c, s*0.42f, col, 24, 1.5f);
    ImVec2 tip = {c.x + s*0.42f, c.y};
    dl->AddTriangleFilled({tip.x, tip.y},{tip.x-s*0.18f,tip.y-s*0.18f},{tip.x+s*0.02f,tip.y-s*0.22f},col);
}

static void DrawIcon_Scale(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    float h = s * 0.4f;
    dl->AddLine({c.x-h,c.y+h},{c.x+h,c.y-h},col,1.5f);
    dl->AddTriangleFilled({c.x+h,c.y-h},{c.x+h-s*0.2f,c.y-h},{c.x+h,c.y-h+s*0.2f},col);
    dl->AddRect({c.x-h,c.y-h*0.3f},{c.x+h*0.3f,c.y+h},col,0,0,1.2f);
}

static void DrawIcon_Snap(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    for (int row = 0; row < 3; row++)
        for (int col2 = 0; col2 < 3; col2++)
            dl->AddCircleFilled({c.x + (col2-1)*s*0.3f, c.y + (row-1)*s*0.3f}, 2.0f, col);
    dl->AddLine({c.x-s*0.4f, c.y+s*0.48f},{c.x+s*0.4f,c.y+s*0.48f},col,1.5f);
}

static void DrawIcon_Folder(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    float w = s*0.8f, h = s*0.55f;
    dl->AddRectFilled({c.x-w*0.5f,c.y-h*0.2f},{c.x+w*0.5f,c.y+h*0.8f},col,2.0f);
    dl->AddRectFilled({c.x-w*0.5f,c.y-h*0.6f},{c.x-w*0.1f,c.y-h*0.2f},col,2.0f);
}

static void DrawIcon_Material(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddCircleFilled({c.x-s*0.15f, c.y+s*0.35f}, s*0.2f, col);
    dl->AddRectFilled({c.x-s*0.05f,c.y-s*0.5f},{c.x+s*0.05f,c.y+s*0.35f},col);
    dl->AddRectFilled({c.x-s*0.05f,c.y-s*0.5f},{c.x+s*0.45f,c.y-s*0.3f},col);
}

static void DrawIcon_Light(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddCircle(c, s*0.33f, col, 16, 1.5f);
    dl->AddRectFilled({c.x-s*0.15f,c.y+s*0.33f},{c.x+s*0.15f,c.y+s*0.5f},col,1.0f);
    dl->AddLine({c.x-s*0.15f,c.y+s*0.5f},{c.x+s*0.15f,c.y+s*0.5f},col,1.5f);
}

static void DrawIcon_Settings(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
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

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0, 8)); // gap-2
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.panel);

    ImGui::Begin("##LeftToolbar", nullptr,
        ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoNav       | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    float       winW = ImGui::GetWindowWidth();
    float       winH = ImGui::GetWindowHeight();
    float       btnS = 32.0f;   // w-8 h-8
    float       offX = (winW - btnS) * 0.5f;

    // Right border (border-r border-studio-border)
    ImVec2 winP = ImGui::GetWindowPos();
    dl->AddLine(ImVec2(winP.x + winW - 1.0f, winP.y), ImVec2(winP.x + winW - 1.0f, winP.y + winH), COL(T.border));

    // initial padding (p-2 y-axis)
    ImGui::Dummy(ImVec2(winW, 4));

    auto drawToolBtn = [&](
        Tool toolType,
        const char* iconKey,
        void(*drawFallback)(ImDrawList*, ImVec2, float, ImU32),
        const char* tooltip
    ) {
        bool isActive = (s_currentTool == toolType);
        ImGui::SetCursorPosX(offX);

        ImU32 bgCol = isActive ? COLA(0x00d2ff, 0.15f) : IM_COL32(0, 0, 0, 0);

        ImVec2 bMin = ImGui::GetCursorScreenPos();
        ImVec2 bMax = ImVec2(bMin.x + btnS, bMin.y + btnS);

        ImGui::InvisibleButton(tooltip, ImVec2(btnS, btnS));
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();

        if (clk) s_currentTool = toolType;

        if (hov && !isActive) bgCol = COL(T.panelHover);
        if (bgCol) dl->AddRectFilled(bMin, bMax, bgCol, 4.0f); // rounded

        // Inset left border for active (shadow-[inset_2px_0_0_0_#00d2ff])
        if (isActive) {
            dl->AddRectFilled(ImVec2(bMin.x, bMin.y), ImVec2(bMin.x + 2.0f, bMax.y), COL(T.accent), 4.0f, ImDrawFlags_RoundCornersLeft);
        }

        ImVec2 center = {(bMin.x + bMax.x)*0.5f, (bMin.y + bMax.y)*0.5f};
        ImTextureID tex = IconRegistry::instance().get(iconKey);
        ImU32 iconCol = isActive ? COL(T.accent) : (hov ? COL(T.textPrimary) : COL(T.textMuted));

        if (tex) {
            float is = btnS * 0.5f;
            dl->AddImage(tex,
                {center.x - is*0.5f, center.y - is*0.5f},
                {center.x + is*0.5f, center.y + is*0.5f},
                {0,0},{1,1}, iconCol);
        } else {
            drawFallback(dl, center, btnS * 0.45f, iconCol);
        }

        if (hov) ImGui::SetTooltip("%s", tooltip);
    };

    drawToolBtn(Tool::Select, "icon_cursor", DrawIcon_Select, "Select Tool (Q)");
    drawToolBtn(Tool::Move,   "icon_move",   DrawIcon_Move,   "Move Tool (W)");
    drawToolBtn(Tool::Rotate, "icon_rotate", DrawIcon_Rotate, "Rotate Tool (E)");
    drawToolBtn(Tool::Scale,  "icon_scale",  DrawIcon_Scale,  "Scale Tool (R)");

    // Snap toggle
    {
        ImGui::SetCursorPosX(offX);
        ImVec2 bMin = ImGui::GetCursorScreenPos();
        ImVec2 bMax = ImVec2(bMin.x + btnS, bMin.y + btnS);

        ImGui::InvisibleButton("##snap", ImVec2(btnS, btnS));
        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) s_gridSnap = !s_gridSnap;

        ImU32 snapCol = s_gridSnap ? COL(T.toggleOn) : COL(T.textMuted);
        if (hov) dl->AddRectFilled(bMin, bMax, COL(T.panelHover), 4.0f);

        ImVec2 c = {(bMin.x+bMax.x)*0.5f,(bMin.y+bMax.y)*0.5f};
        ImTextureID tex = IconRegistry::instance().get("icon_snap");
        if (tex) {
            float is = btnS * 0.5f;
            dl->AddImage(tex,{c.x-is*0.5f,c.y-is*0.5f},{c.x+is*0.5f,c.y+is*0.5f},{0,0},{1,1},snapCol);
        } else {
            DrawIcon_Snap(dl, c, btnS * 0.45f, snapCol);
        }
        if (hov) ImGui::SetTooltip("Grid Snap • 1m / 15°  [%s]", s_gridSnap ? "Active" : "Off");
    }

    ImGui::Dummy(ImVec2(winW, 4));
    {
        ImVec2 sep = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(sep.x + 4.0f, sep.y), ImVec2(sep.x + winW - 4.0f, sep.y), COL(T.border)); // mx-1
    }
    ImGui::Dummy(ImVec2(winW, 4));

    // Editor shortcuts
    auto drawShortcut = [&](const char* id, const char* iconKey, void(*fallback)(ImDrawList*,ImVec2,float,ImU32), ImU32 bgNormal, ImU32 bgHover, ImU32 borderNormal, ImU32 iconCol, bool& toggleState, const char* tip) {
        ImGui::SetCursorPosX(offX);
        ImVec2 bMin = ImGui::GetCursorScreenPos();
        ImVec2 bMax = ImVec2(bMin.x + btnS, bMin.y + btnS);
        ImGui::InvisibleButton(id, ImVec2(btnS, btnS));
        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) toggleState = !toggleState;

        dl->AddRectFilled(bMin, bMax, hov ? bgHover : bgNormal, 4.0f);
        if (borderNormal) dl->AddRect(bMin, bMax, borderNormal, 4.0f, 0, 1.0f);

        ImVec2 c = {(bMin.x+bMax.x)*0.5f,(bMin.y+bMax.y)*0.5f};
        ImTextureID tex = IconRegistry::instance().get(iconKey);
        if (tex) {
            float is = btnS*0.5f;
            dl->AddImage(tex,{c.x-is*0.5f,c.y-is*0.5f},{c.x+is*0.5f,c.y+is*0.5f},{0,0},{1,1},iconCol);
        } else {
            fallback(dl,c,btnS*0.45f,iconCol);
        }
        if (hov) ImGui::SetTooltip("%s", tip);
    };

    drawShortcut("##ast", "icon_folder", DrawIcon_Folder, COLA(0x00d2ff, 0.10f), COLA(0x00d2ff, 0.15f), COLA(0x00d2ff, 0.40f), COL(T.accent), EditorLayout::instance().showAssetBrowser, "Asset Manager");
    drawShortcut("##mat", "icon_material", DrawIcon_Material, COLA(0x171717, 1.0f), COL(T.panelHover), COL(T.border), COL(T.textPrimary), EditorLayout::instance().showMaterialEditor, "Material Editor");

    {
        ImGui::SetCursorPosX(offX);
        ImVec2 bMin = ImGui::GetCursorScreenPos();
        ImVec2 bMax = ImVec2(bMin.x + btnS, bMin.y + btnS);
        ImGui::InvisibleButton("##light", ImVec2(btnS, btnS));
        bool hov = ImGui::IsItemHovered();
        if (hov) dl->AddRectFilled(bMin, bMax, COL(T.panelHover), 4.0f);
        ImVec2 c = {(bMin.x+bMax.x)*0.5f,(bMin.y+bMax.y)*0.5f};
        DrawIcon_Light(dl, c, btnS*0.45f, hov ? COL(T.textPrimary) : COL(T.textMuted));
    }

    {
        float posY = ImGui::GetWindowHeight() - btnS - 8.0f;
        if (posY > ImGui::GetCursorPosY()) ImGui::SetCursorPosY(posY);
        ImGui::SetCursorPosX(offX);

        ImVec2 bMin = ImGui::GetCursorScreenPos();
        ImVec2 bMax = ImVec2(bMin.x + btnS, bMin.y + btnS);
        ImGui::InvisibleButton("##settings", ImVec2(btnS, btnS));
        bool hov = ImGui::IsItemHovered();
        if (hov) dl->AddRectFilled(bMin, bMax, COL(T.panelHover), 4.0f);

        ImVec2 c = {(bMin.x+bMax.x)*0.5f,(bMin.y+bMax.y)*0.5f};
        DrawIcon_Settings(dl, c, btnS*0.45f, hov ? COL(T.textPrimary) : COL(T.textMuted));
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}
