#include "BottomBar.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "NexusTheme.h"
#include "IconRegistry.h"
#include <cstdio>

static ImU32 COL(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float alpha) { return IM_COL32((hex>>16)&0xFF,(hex>>8)&0xFF,hex&0xFF,(uint8_t)(alpha*255)); }

void BottomBar::draw(float deltaTime) {
    auto& T = NexusTheme::instance();
    
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 workPos = vp->WorkPos;
    ImVec2 workSize = vp->WorkSize;
    
    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDocking;
    ImGui::SetNextWindowClass(&window_class);
    
    ImGui::SetNextWindowPos(ImVec2(workPos.x, workPos.y + workSize.y - 20.0f));
    ImGui::SetNextWindowSize(ImVec2(workSize.x, 20.0f));
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.bgPanel);
    
    ImGui::Begin("BottomBar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);
    
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float w = ImGui::GetWindowWidth();
    
    dl->AddLine(ImVec2(p.x, p.y), ImVec2(p.x + w, p.y), COL(T.border));
    
    float fps = deltaTime > 0.0001f ? (1.0f / deltaTime) : 0.0f;
    
    float cx = p.x + 10.0f;
    float cy = p.y + 1.0f; 
    
    dl->AddText(ImVec2(cx, cy + 3.0f), COL(T.textPrimary), "Ready");
    cx += 100.0f; // Gap after ready
    
    auto drawItem = [&](const char* iconKey, const char* label, const char* val, ImU32 valCol = 0) {
        if (valCol == 0) valCol = COL(T.textPrimary);
        ImTextureID tex = IconRegistry::instance().get(iconKey);
        if (tex) {
            dl->AddImage(tex, ImVec2(cx, cy + 2.0f), ImVec2(cx + 14.0f, cy + 16.0f));
            cx += 18.0f;
        }
        
        ImGuiIO& io = ImGui::GetIO();
        bool pushedFont = false;
        if (io.Fonts->Fonts.Size > 2) {
            ImGui::PushFont(io.Fonts->Fonts[2]); // Tiny font
            pushedFont = true;
        }
        
        dl->AddText(ImVec2(cx, cy + 3.0f), COL(T.textSecondary), label);
        cx += ImGui::CalcTextSize(label).x + 4.0f;
        
        dl->AddText(ImVec2(cx, cy + 3.0f), valCol, val);
        cx += ImGui::CalcTextSize(val).x + 10.0f;
        
        if (pushedFont) ImGui::PopFont();
    };
    
    // Push the stats to the right
    cx = p.x + w - 550.0f; 
    
    char fpsStr[32]; snprintf(fpsStr, sizeof(fpsStr), "%.2f", fps);
    drawItem("icon_fps", "FPS:", fpsStr);
    drawItem("icon_speed", "Camera Speed:", "1.25x");
    drawItem("icon_snap_bold", "Grid:", "32x");
    drawItem("icon_vram", "VRam:", "1.4/8GB");
    
    drawItem("icon_net", "Ping:", "8ms", COLA(0x22c55e, 1.0f));
    drawItem("icon_warning", "Warning:", "12", COLA(0xf59e0b, 1.0f));
    drawItem("icon_error", "Error:", "3", COLA(0xef4444, 1.0f));
    drawItem("icon_info", "Info:", "26", COL(T.accent));
    
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
