#include "FileListBar.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "NexusTheme.h"
#include "IconRegistry.h"
#include <string>
#include <vector>
#include <algorithm>

static ImU32 COL(const ImVec4& v)  { return ImGui::ColorConvertFloat4ToU32(v); }

void FileListBar::draw() {
    auto& T = NexusTheme::instance();
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.bgPanel);

    ImGuiWindowClass window_class;
    window_class.ClassId = ImGui::GetID("FileListBarClass");
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDockingOverMe | ImGuiDockNodeFlags_NoDockingSplit | ImGuiDockNodeFlags_NoResize;
    ImGui::SetNextWindowClass(&window_class);
    
    ImGui::Begin("FileListBar", nullptr, 
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleColor();
                 
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 win = ImGui::GetWindowPos();
    float H = ImGui::GetWindowHeight();
    
    // Bottom border
    dl->AddLine(ImVec2(win.x, win.y + H - 1.0f), ImVec2(win.x + ImGui::GetWindowWidth(), win.y + H - 1.0f), COL(T.border));

    float cx = win.x;
    
    ImGuiWindow* viewportWindow = ImGui::FindWindowByName("Viewport");
    ImGuiDockNode* vpNode = viewportWindow ? viewportWindow->DockNode : nullptr;
    ImGuiWindow* activeWin = vpNode ? vpNode->VisibleWindow : nullptr;

    std::vector<ImGuiWindow*> panels;
    ImGuiWindow* vpWin = nullptr;

    if (vpNode) {
        for (int i = 0; i < vpNode->Windows.Size; i++) {
            ImGuiWindow* w = vpNode->Windows[i];
            if (std::string(w->Name) == "Viewport") {
                vpWin = w;
            } else {
                panels.push_back(w);
            }
        }
    }

    // Sort panels alphabetically
    std::sort(panels.begin(), panels.end(), [](ImGuiWindow* a, ImGuiWindow* b) {
        return std::string(a->Name) < std::string(b->Name);
    });

    struct Tab { std::string label; std::string icon; bool active; float width; ImGuiWindow* targetWindow; };
    std::vector<Tab> tabs;

    // 1. Viewport (Always first)
    if (vpWin) {
        ImVec2 ts = ImGui::CalcTextSize("3D Viewport");
        tabs.push_back({ "3D Viewport", "icon_scene", (vpWin == activeWin), ts.x + 40.0f, vpWin });
    }

    // 2. Panels (Docked dynamically into the center)
    for (auto* p : panels) {
        std::string name = p->Name;
        std::string icon = "icon_folder_bold"; // default
        if (name == "Asset Browser") icon = "icon_folder_bold";
        else if (name == "Material Editor") icon = "icon_node_editor_bold";
        else if (name == "Console") icon = "icon_script_bold";
        
        ImVec2 ts = ImGui::CalcTextSize(name.c_str());
        tabs.push_back({ name, icon, (p == activeWin), ts.x + 40.0f, p });
    }

    // 3. Mock Files (Hardcoded for now)
    tabs.push_back({ "MainScript.luau", "icon_script", false, 120.0f, nullptr });
    tabs.push_back({ "Database.luau", "icon_script", false, 115.0f, nullptr });
    tabs.push_back({ "Implementation Plan", "icon_script", false, 140.0f, nullptr });

    ImGuiContext& g = *GImGui;
    ImGuiWindow* fileListWindow = ImGui::GetCurrentWindow();

    for (auto& t : tabs) {
        ImVec2 tMin = ImVec2(cx, win.y);
        ImVec2 tMax = ImVec2(cx + t.width, win.y + H);
        
        ImGui::SetCursorScreenPos(tMin);
        
        // Use sibling->ID as the ID stack base so the button ID is stable
        // even if the active window changes during a click (preserves drag state).
        if (t.targetWindow)
            fileListWindow->IDStack.push_back(t.targetWindow->ID);
        else
            ImGui::PushID(t.label.c_str());

        ImGuiID btnId = fileListWindow->GetID("##tabBtn");
        ImGui::SetCursorScreenPos(tMin);
        ImGui::InvisibleButton("##tabBtn", ImVec2(t.width, H));

        if (t.targetWindow)
            fileListWindow->IDStack.pop_back();
        else
            ImGui::PopID();
        
        if (ImGui::IsItemClicked()) {
            if (t.targetWindow) {
                // CRITICAL: Set NoClearOnFocusLoss BEFORE FocusWindow.
                // FocusWindow clears ActiveId when g.ActiveIdWindow->RootWindow != target->RootWindow
                // (imgui.cpp:13950-13952). This kills our drag state on the very first press frame.
                // Setting this flag bypasses that ClearActiveID() call.
                g.ActiveIdNoClearOnFocusLoss = true;

                ImGui::FocusWindow(t.targetWindow);

                ImGuiDockNode* tgtNode = t.targetWindow->DockNode;
                if (tgtNode) {
                    tgtNode->SelectedTabId = t.targetWindow->TabId;
                    if (tgtNode->TabBar) {
                        tgtNode->TabBar->NextSelectedTabId = t.targetWindow->TabId;
                        tgtNode->TabBar->SelectedTabId = t.targetWindow->TabId;
                    }
                    tgtNode->VisibleWindow = t.targetWindow;
                }
            }
        }

        
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            // Viewport sekmesi FileListBar'dan drag edilemesin
            bool isViewport = t.targetWindow && (std::string(t.targetWindow->Name) == "Viewport");
            if (t.targetWindow && t.targetWindow->DockNode && !isViewport) {
                ImGuiWindow* sibling = t.targetWindow;

                // Queue undock for next frame
                ImGui::DockContextQueueUndockWindow(GImGui, sibling);

                // StartMouseMovingWindow may not set g.MovingWindow when the window
                // is docked into a non-moveable host. We mirror DockNodeStartMouseMovingWindow
                // which explicitly overrides g.MovingWindow after the call.
                ImGui::StartMouseMovingWindow(sibling);
                g.MovingWindow = sibling; // Force override (mirrors DockNodeStartMouseMovingWindow:L19262)

                // Spoof ActiveIdClickOffset.y = 0 so the is_drag_docking rect check in
                // BeginDockableDragDropSource (imgui.cpp:21620) succeeds.
                // ImRect(0,0,w,GetFrameHeight()).Contains(ActiveIdClickOffset) must be true.
                float offsetX = g.IO.MouseClickedPos[0].x - sibling->Pos.x;
                g.ActiveIdClickOffset = ImVec2(offsetX, 0.0f);

                // DO NOT call BeginDockableDragDropSource here — it asserts g.CurrentWindow==window.
                // ImGui's own Begin() (imgui.cpp:8754) will call it automatically next frame
                // once g.MovingWindow == sibling && g.ActiveId == sibling->MoveId.
                break;
            }
        }

        
        bool hov = ImGui::IsItemHovered();

        
        // Background
        if (t.active) {
            dl->AddRectFilled(tMin, tMax, COL(T.bgDeepest));
            // Top indicator
            ImU32 indicatorCol = COL(T.accent); // default accent Blue
            if (t.label == "Database.luau") indicatorCol = COL(T.accentYellow);
            else if (t.label == "Implementation Plan") indicatorCol = COL(T.accentGreen);
            else if (t.label == "MainScript.luau") indicatorCol = COL(T.border);
            
            dl->AddRectFilled(tMin, ImVec2(tMax.x, tMin.y + 2.0f), indicatorCol);
        } else if (hov) {
            dl->AddRectFilled(tMin, tMax, COL(T.bgCard));
        }
        
        // Right border
        dl->AddLine(ImVec2(tMax.x, tMin.y + 6.0f), ImVec2(tMax.x, tMax.y - 6.0f), COL(T.border));
        
        // Icon (14x14)
        ImTextureID icon = IconRegistry::instance().get(t.icon.c_str());
        float ix = tMin.x + 7.0f; // sol padding 7px
        float iy = win.y + (H - 14.0f) * 0.5f;
        
        if (icon) {
            ImU32 iconCol = t.active ? COL(T.accent) : COL(T.textMuted);
            dl->AddImage(icon, ImVec2(ix, iy), ImVec2(ix + 14.0f, iy + 14.0f), ImVec2(0,0), ImVec2(1,1), iconCol);
        } else if (t.label == "3D Viewport") {
            // Add cyan dot if no icon
            dl->AddCircleFilled(ImVec2(ix + 7.0f, iy + 7.0f), 3.0f, t.active ? COL(T.accent) : COL(T.textMuted));
        }
        
        // Text at ix + 14 + 4 = 25px
        ImGuiIO& io = ImGui::GetIO();
        bool pushedFont = false;
        if (io.Fonts->Fonts.Size > 2) {
            ImGui::PushFont(io.Fonts->Fonts[2]); // Tiny font 10px (Spec: Inter 9px 600)
            pushedFont = true;
        }
        
        ImVec2 ts = ImGui::CalcTextSize(t.label.c_str());
        ImU32 textCol = t.active ? COL(T.textPrimary) : COL(T.textSecondary);
        dl->AddText(ImVec2(tMin.x + 25.0f, win.y + (H - ts.y)*0.5f), textCol, t.label.c_str());
        
        if (pushedFont) ImGui::PopFont();
        
        cx += t.width;
    }
    
    ImGui::End();
    ImGui::PopStyleVar(2);
}
