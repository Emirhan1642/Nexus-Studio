#include "LeftToolbar.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "IconRegistry.h"
#include "NexusTheme.h"
#include "EditorLayout.h"
#include "SelectionManager.h"
#include "Editor/Modeling/ModelingContext.h"

// ─── Viewport states (extern from ViewportPanel.cpp) ───
extern bool  s_wireframe;
extern bool  s_collision;
extern bool  s_worldSpace;
extern int   s_viewMode;
extern int   s_litMode;

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

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float topBarH = 30.0f;
    float bottomBarH = 20.0f;
    float leftBarW = 50.0f;

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + topBarH));
    ImGui::SetNextWindowSize(ImVec2(leftBarW, vp->WorkSize.y - (topBarH + bottomBarH)));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0, 15)); // gap: 15px
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.bgPanel);

    ImGui::Begin("##LeftToolbar", nullptr, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
    ImGui::PopStyleColor();

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    float       winW = ImGui::GetWindowWidth();
    float       winH = ImGui::GetWindowHeight();
    float       btnS = 24.0f; // Spec'e uygun buton boyutu (20x20 ikon için 24x24 buton)
    float       offX = (winW - btnS) * 0.5f;

    // Right border
    ImVec2 winP = ImGui::GetWindowPos();
    dl->AddLine(ImVec2(winP.x + winW - 1.0f, winP.y), ImVec2(winP.x + winW - 1.0f, winP.y + winH), COL(T.border));

    // initial padding
    ImGui::Dummy(ImVec2(winW, 10));

    auto drawToolBtn = [&](
        EditorTool toolType,
        const char* iconKey,
        void(*drawFallback)(ImDrawList*, ImVec2, float, ImU32),
        const char* tooltip
    ) {
        bool isActive = (EditorLayout::instance().currentTool == toolType);
        ImGui::SetCursorPosX(offX);

        ImVec2 bMin = ImGui::GetCursorScreenPos();
        ImVec2 bMax = ImVec2(bMin.x + btnS, bMin.y + btnS);

        ImGui::InvisibleButton(tooltip, ImVec2(btnS, btnS));
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();

        if (clk) EditorLayout::instance().currentTool = toolType;

        ImU32 iconCol = isActive ? COL(T.accent) : (hov ? COL(T.textPrimary) : COL(T.textMuted));

        ImVec2 center = {(bMin.x + bMax.x)*0.5f, (bMin.y + bMax.y)*0.5f};
        std::string finalIcon = isActive ? std::string(iconKey) + "_bold" : std::string(iconKey);
        ImTextureID tex = IconRegistry::instance().get(finalIcon.c_str());
        if (!tex && isActive) tex = IconRegistry::instance().get(iconKey);

        if (tex) {
            float is = 20.0f; // ikonlar 20x20px
            dl->AddImage(tex,
                {center.x - is*0.5f, center.y - is*0.5f},
                {center.x + is*0.5f, center.y + is*0.5f},
                {0,0},{1,1}, iconCol);
        } else {
            drawFallback(dl, center, 20.0f * 0.45f, iconCol);
        }

        if (hov) ImGui::SetTooltip("%s", tooltip);
    };

    // Tool ikonları
    drawToolBtn(EditorTool::Select, "icon_cursor", DrawIcon_Select, "Select Tool (Q)");
    drawToolBtn(EditorTool::Move,   "icon_move",   DrawIcon_Move,   "Move Tool (W)");
    drawToolBtn(EditorTool::Rotate, "icon_rotate", DrawIcon_Rotate, "Rotate Tool (E)");
    drawToolBtn(EditorTool::Scale,  "icon_scale",  DrawIcon_Scale,  "Scale Tool (R)");

    // Snap toggle
    {
        bool isSnap = EditorLayout::instance().gridSnap;
        ImGui::SetCursorPosX(offX);
        ImVec2 bMin = ImGui::GetCursorScreenPos();
        ImVec2 bMax = ImVec2(bMin.x + btnS, bMin.y + btnS);

        ImGui::InvisibleButton("##snap", ImVec2(btnS, btnS));
        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) EditorLayout::instance().gridSnap = !EditorLayout::instance().gridSnap;

        ImU32 snapCol = isSnap ? COL(T.accent) : (hov ? COL(T.textPrimary) : COL(T.textMuted));

        ImVec2 c = {(bMin.x+bMax.x)*0.5f,(bMin.y+bMax.y)*0.5f};
        std::string finalIcon = isSnap ? "icon_snap_bold" : "icon_snap";
        ImTextureID tex = IconRegistry::instance().get(finalIcon.c_str());
        if (!tex && isSnap) tex = IconRegistry::instance().get("icon_snap");
        if (tex) {
            float is = 20.0f;
            dl->AddImage(tex,{c.x-is*0.5f,c.y-is*0.5f},{c.x+is*0.5f,c.y+is*0.5f},{0,0},{1,1},snapCol);
        } else {
            DrawIcon_Snap(dl, c, 20.0f * 0.45f, snapCol);
        }
        if (hov) ImGui::SetTooltip("Grid Snap • 1m / 15°  [%s]", isSnap ? "Active" : "Off");
    }

    // Divider
    auto drawDivider = [&]() {
        ImVec2 sep = ImGui::GetCursorScreenPos();
        // Divider: width: 26px, height: 2px, opacity: 0.20
        float divW = 26.0f;
        float divX = winP.x + (winW - divW) * 0.5f;
        dl->AddRectFilled(ImVec2(divX, sep.y), ImVec2(divX + divW, sep.y + 2.0f), COLA(0xFFFFFF, 0.20f));
        ImGui::Dummy(ImVec2(winW, 2.0f)); // account for height
    };

    drawDivider();

    // Editor Panel shortcuts
    auto drawShortcut = [&](const char* id, const char* iconKey, void(*fallback)(ImDrawList*,ImVec2,float,ImU32), bool& toggleState, const char* tip) {
        ImGui::SetCursorPosX(offX);
        ImVec2 bMin = ImGui::GetCursorScreenPos();
        ImVec2 bMax = ImVec2(bMin.x + btnS, bMin.y + btnS);
        ImGui::InvisibleButton(id, ImVec2(btnS, btnS));
        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) toggleState = !toggleState;

        ImU32 iconCol = toggleState ? COL(T.accent) : (hov ? COL(T.textPrimary) : COL(T.textMuted));

        ImVec2 c = {(bMin.x+bMax.x)*0.5f,(bMin.y+bMax.y)*0.5f};
        std::string finalIcon = toggleState ? std::string(iconKey) + "_bold" : std::string(iconKey);
        ImTextureID tex = IconRegistry::instance().get(finalIcon.c_str());
        if (!tex && toggleState) tex = IconRegistry::instance().get(iconKey);
        if (tex) {
            float is = 20.0f;
            dl->AddImage(tex,{c.x-is*0.5f,c.y-is*0.5f},{c.x+is*0.5f,c.y+is*0.5f},{0,0},{1,1},iconCol);
        } else {
            fallback(dl,c, 20.0f*0.45f, iconCol);
        }
        if (hov) ImGui::SetTooltip("%s", tip);
    };

    // [Panel ikonları: Folder / Material / AI / Explorer / Properties]
    drawShortcut("##ast", "icon_folder", DrawIcon_Folder, EditorLayout::instance().showAssetBrowser, "Asset Manager");
    drawShortcut("##mat", "icon_node_editor", DrawIcon_Material, EditorLayout::instance().showMaterialEditor, "Material Editor");
    drawShortcut("##exp", "icon_explorer", DrawIcon_Folder, EditorLayout::instance().showExplorer, "Explorer");
    drawShortcut("##prop", "icon_properties", DrawIcon_Folder, EditorLayout::instance().showProperties, "Properties");
    drawShortcut("##ai", "icon_ai", DrawIcon_Folder, EditorLayout::instance().showAICopilot, "AI Copilot");

    drawDivider();

    // [Camera View İkonları]
    // 1. Free View (Kamera serbestçe gezer)
    bool isFree = (EditorLayout::instance().cameraMode == CameraViewMode::Free && !EditorLayout::instance().isOrthographic);
    bool clickedFree = isFree;
    drawShortcut("##cam_free", "icon_free_cam", DrawIcon_Folder, clickedFree, "Free View (Free Fly & Look)");
    if (clickedFree && !isFree) {
        EditorLayout::instance().cameraMode = CameraViewMode::Free;
        EditorLayout::instance().isOrthographic = false;
    }

    // 2. Perspective / Orthographic Projection Toggle
    bool isOrtho = EditorLayout::instance().isOrthographic;
    bool clickedOrtho = isOrtho;
    drawShortcut("##cam_ortho", "icon_camera", DrawIcon_Folder, clickedOrtho, isOrtho ? "Orthographic View (Active)" : "Perspective View (Click for Orthographic)");
    if (clickedOrtho != isOrtho) EditorLayout::instance().isOrthographic = clickedOrtho;

    // 3. 90 Degree View (Seçili objeye veya odak noktasına 90 derecelik açılarla bakar)
    bool is90 = (EditorLayout::instance().cameraMode == CameraViewMode::Degree90);
    bool clicked90 = is90;
    drawShortcut("##cam_90", "icon_90_degree", DrawIcon_Folder, clicked90, "90 Degree View (Snap 90° Axis)");
    if (clicked90 && !is90) {
        EditorLayout::instance().cameraMode = CameraViewMode::Degree90;
    }
    
    drawDivider();
    
    // [Modes: Object, Face, Edge, Vertex]
    bool isObj = (EditorLayout::instance().shadingMode == EditorShadingMode::Object);
    bool clickedObj = isObj;
    drawShortcut("##shading_obj", "icon_model", DrawIcon_Folder, clickedObj, "Object Mode (Select & Transform Objects)");
    if (clickedObj && !isObj) EditorLayout::instance().shadingMode = EditorShadingMode::Object;

    bool isFace = (EditorLayout::instance().shadingMode == EditorShadingMode::Face);
    bool clickedFace = isFace;
    drawShortcut("##shading_face", "icon_mesh", DrawIcon_Folder, clickedFace, "Face Mode (Select & Move Faces)");
    if (clickedFace && !isFace) EditorLayout::instance().shadingMode = EditorShadingMode::Face;

    bool isEdge = (EditorLayout::instance().shadingMode == EditorShadingMode::Edge);
    bool clickedEdge = isEdge;
    drawShortcut("##shading_edge", "icon_box", DrawIcon_Folder, clickedEdge, "Edge Mode (Select & Move Edges)");
    if (clickedEdge && !isEdge) EditorLayout::instance().shadingMode = EditorShadingMode::Edge;

    bool isVert = (EditorLayout::instance().shadingMode == EditorShadingMode::Vertex);
    bool clickedVert = isVert;
    drawShortcut("##shading_vert", "icon_wireframe", DrawIcon_Folder, clickedVert, "Vertex Mode (Select & Move Vertices)");
    if (clickedVert && !isVert) EditorLayout::instance().shadingMode = EditorShadingMode::Vertex;

    // In Edit Modes: Show Dedicated Modeling Tools (Extrude, Inset, Bevel, LoopCut, Knife, Subdivide, Merge, UV)
    if (EditorLayout::instance().shadingMode != EditorShadingMode::Object) {
        drawDivider();
        auto selPart = std::dynamic_pointer_cast<Part>(SelectionManager::instance().getSelected());
        auto& mCtx = Editor::Modeling::ModelingContext::instance();

        // 1. Extrude (Ctrl+E)
        bool dummyExtrude = (mCtx.activeModal == Editor::Modeling::ModalTool::Extrude);
        drawShortcut("##tool_extrude", "icon_mesh", DrawIcon_Folder, dummyExtrude, "Extrude Region (Ctrl+E)");
        if (ImGui::IsItemClicked() && selPart) mCtx.startExtrude(selPart);

        // 2. Inset (I)
        bool dummyInset = (mCtx.activeModal == Editor::Modeling::ModalTool::Inset);
        drawShortcut("##tool_inset", "icon_box", DrawIcon_Folder, dummyInset, "Inset Faces (I)");
        if (ImGui::IsItemClicked() && selPart) mCtx.startInset(selPart);

        // 3. Bevel (Ctrl+B)
        bool dummyBevel = (mCtx.activeModal == Editor::Modeling::ModalTool::Bevel);
        drawShortcut("##tool_bevel", "icon_3d_cube", DrawIcon_Folder, dummyBevel, "Bevel / Chamfer (Ctrl+B)");
        if (ImGui::IsItemClicked() && selPart) mCtx.startBevel(selPart);

        // 4. Loop Cut (Ctrl+R)
        bool dummyLoop = (mCtx.activeModal == Editor::Modeling::ModalTool::LoopCut);
        drawShortcut("##tool_loopcut", "icon_wireframe", DrawIcon_Folder, dummyLoop, "Loop Cut & Slide (Ctrl+R)");
        if (ImGui::IsItemClicked() && selPart) mCtx.startLoopCut(selPart);

        // 5. Knife Tool (K)
        bool dummyKnife = (mCtx.activeModal == Editor::Modeling::ModalTool::Knife);
        drawShortcut("##tool_knife", "icon_ai", DrawIcon_Folder, dummyKnife, "Knife Cut (K)");
        if (ImGui::IsItemClicked() && selPart) mCtx.startKnife(selPart);

        // 6. Subdivide
        bool dummySubdiv = false;
        drawShortcut("##tool_subdiv", "icon_model", DrawIcon_Folder, dummySubdiv, "Subdivide Faces");
        if (ImGui::IsItemClicked() && selPart) mCtx.executeSubdivide(selPart, 1, 0.0f);

        // 7. Auto UV Mapping
        bool dummyUV = false;
        drawShortcut("##tool_uv", "icon_node_editor", DrawIcon_Folder, dummyUV, "Smart UV Project (Auto-Unwrap)");
        if (ImGui::IsItemClicked() && selPart) mCtx.executeSmartUV(selPart);
    }

    drawDivider();

    drawShortcut("##coll", "icon_collision", DrawIcon_Folder, s_collision, "Collision Bounds");
    drawShortcut("##world", "icon_world", DrawIcon_Folder, s_worldSpace, "World Space Toggle");

    // Settings icon at bottom
    {
        float posY = winH - btnS - 10.0f; // settings ikonu en altta
        if (posY > ImGui::GetCursorPosY()) ImGui::SetCursorPosY(posY);
        ImGui::SetCursorPosX(offX);

        ImVec2 bMin = ImGui::GetCursorScreenPos();
        ImVec2 bMax = ImVec2(bMin.x + btnS, bMin.y + btnS);
        ImGui::InvisibleButton("##settings", ImVec2(btnS, btnS));
        bool hov = ImGui::IsItemHovered();
        
        ImVec2 c = {(bMin.x+bMax.x)*0.5f,(bMin.y+bMax.y)*0.5f};
        ImTextureID tex = IconRegistry::instance().get(hov ? "icon_setting_bold" : "icon_setting");
        if (tex) {
            float is = 20.0f;
            dl->AddImage(tex,{c.x-is*0.5f,c.y-is*0.5f},{c.x+is*0.5f,c.y+is*0.5f},{0,0},{1,1}, hov ? COL(T.textPrimary) : COL(T.textMuted));
        } else {
            DrawIcon_Settings(dl, c, 20.0f*0.45f, hov ? COL(T.textPrimary) : COL(T.textMuted));
        }
        if (hov) ImGui::SetTooltip("Settings");
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}
