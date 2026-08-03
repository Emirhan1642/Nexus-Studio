#include "FileListBar.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "NexusTheme.h"
#include "IconRegistry.h"
#include <string>

static ImU32 COL(const ImVec4& v)  { return ImGui::ColorConvertFloat4ToU32(v); }

void FileListBar::draw() {
    auto& T = NexusTheme::instance();
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.bgPanel);

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
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
    
    struct Tab { const char* label; const char* icon; bool active; float width; };
    Tab tabs[] = {
        {"3D Viewport", "icon_scene", true, 110.0f},
        {"MainScript.luau", "icon_script", false, 120.0f},
        {"Database.luau", "icon_script", false, 115.0f},
        {"Implementation Plan", "icon_script", false, 140.0f}
    };
    
    for (auto& t : tabs) {
        ImVec2 tMin = ImVec2(cx, win.y);
        ImVec2 tMax = ImVec2(cx + t.width, win.y + H);
        
        ImGui::SetCursorScreenPos(tMin);
        ImGui::InvisibleButton(t.label, ImVec2(t.width, H));
        bool hov = ImGui::IsItemHovered();
        
        // Background
        if (t.active) {
            dl->AddRectFilled(tMin, tMax, COL(T.bgDeepest));
            // Top indicator
            ImU32 indicatorCol = COL(T.accent); // default accent Blue
            std::string labelStr = t.label;
            if (labelStr == "Database.luau") indicatorCol = COL(T.accentYellow);
            else if (labelStr == "Implementation Plan") indicatorCol = COL(T.accentGreen);
            else if (labelStr == "MainScript.luau") indicatorCol = COL(T.border);
            
            dl->AddRectFilled(tMin, ImVec2(tMax.x, tMin.y + 2.0f), indicatorCol);
        } else if (hov) {
            dl->AddRectFilled(tMin, tMax, COL(T.bgCard));
        }
        
        // Right border
        dl->AddLine(ImVec2(tMax.x, tMin.y + 6.0f), ImVec2(tMax.x, tMax.y - 6.0f), COL(T.border));
        
        // Icon (14x14)
        ImTextureID icon = IconRegistry::instance().get(t.icon);
        float ix = tMin.x + 7.0f; // sol padding 7px
        float iy = win.y + (H - 14.0f) * 0.5f;
        
        if (icon) {
            ImU32 iconCol = t.active ? COL(T.accent) : COL(T.textMuted);
            dl->AddImage(icon, ImVec2(ix, iy), ImVec2(ix + 14.0f, iy + 14.0f), ImVec2(0,0), ImVec2(1,1), iconCol);
        } else if (std::string(t.label) == "3D Viewport") {
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
        
        ImVec2 ts = ImGui::CalcTextSize(t.label);
        ImU32 textCol = t.active ? COL(T.textPrimary) : COL(T.textSecondary);
        dl->AddText(ImVec2(tMin.x + 25.0f, win.y + (H - ts.y)*0.5f), textCol, t.label);
        
        if (pushedFont) ImGui::PopFont();
        
        cx += t.width;
    }
    
    ImGui::End();
    ImGui::PopStyleVar(2);
}
