#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include "NexusTheme.h"
#include "IconRegistry.h"
#include "EditorLayout.h"
#include <string>
#include <vector>
#include <algorithm>

namespace Editor::UI {

inline void DrawSingleTabHeader(const char* label, const char* icon, float width, ImU32 indicatorCol, bool drawIfSingle = true) {
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiDockNode* node = window->DockNode;
    
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 basePos = ImGui::GetCursorScreenPos(); 
    
    // Check if this window is docked in the same node as the Viewport.
    // If so, we delegate tab drawing to FileListBar instead.
    ImGuiWindow* vpWindow = ImGui::FindWindowByName("Viewport");
    if (vpWindow && node && vpWindow->DockNode == node) {
        return; // Do not draw the tab bar, FileListBar handles this node!
    }

    float H = 30.0f;
    float W = ImGui::GetWindowWidth();
    
    // Draw background for the tab bar area
    dl->AddRectFilled(basePos, ImVec2(basePos.x + W, basePos.y + H), ImGui::ColorConvertFloat4ToU32(T.bgPanel));
    dl->AddLine(ImVec2(basePos.x, basePos.y + H - 1.0f), ImVec2(basePos.x + W, basePos.y + H - 1.0f), ImGui::ColorConvertFloat4ToU32(T.border));
    
    // If not docked or alone, just draw the single header
    if (!node || node->Windows.Size <= 1) {
        if (!drawIfSingle) return;
        
        ImGui::InvisibleButton(label, ImVec2(W, H));
        
        // Start moving the window natively when this custom header is dragged
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            if (node) {
                ImGui::DockContextQueueUndockWindow(GImGui, window);
            }

            ImGui::StartMouseMovingWindow(window);
            g.MovingWindow = window;

            float offsetX = g.IO.MouseClickedPos[0].x - window->Pos.x;
            g.ActiveIdClickOffset = ImVec2(offsetX, 0.0f);
        }
        
        ImVec2 tMin = basePos;
        
        ImGuiIO& io = ImGui::GetIO();
        bool pushedFont = false;
        if (io.Fonts->Fonts.Size > 1) {
            ImGui::PushFont(io.Fonts->Fonts[1]); 
            pushedFont = true;
        }
        ImVec2 ts = ImGui::CalcTextSize(label);
        
        float sWidth = 10.0f + 20.0f + 10.0f + ts.x + 10.0f;
        ImVec2 tMax = ImVec2(basePos.x + sWidth, basePos.y + H);
        
        // Background (Transparent)

        
        // Icon (20x20)
        ImTextureID iconTex = IconRegistry::instance().get(icon);
        float ix = tMin.x + 10.0f;
        float iy = tMin.y + (H - 20.0f) * 0.5f;
        ImU32 iconCol = ImGui::ColorConvertFloat4ToU32(T.textPrimary);
        if (iconTex) {
            dl->AddImage(iconTex, ImVec2(ix, iy), ImVec2(ix + 20.0f, iy + 20.0f), ImVec2(0,0), ImVec2(1,1), iconCol);
        }
        
        // Text
        ImU32 textCol = ImGui::ColorConvertFloat4ToU32(T.textPrimary);
        dl->AddText(ImVec2(tMin.x + 40.0f, tMin.y + (H - ts.y)*0.5f), textCol, label);
        
        if (pushedFont) ImGui::PopFont();
    } else {
        // Draw multiple custom tabs for all sibling windows in this dock node!
        ImVec2 btnPos = basePos;
        
        // Copy and sort windows to maintain stable visual tab ordering
        // regardless of ImGui's internal order (which we modify below for HiddenTabBar)
        std::vector<ImGuiWindow*> displayWindows;
        for (int i = 0; i < node->Windows.Size; i++) {
            displayWindows.push_back(node->Windows[i]);
        }
        auto getTabPriority = [](const std::string& name) -> int {
            if (name.find("Asset") != std::string::npos) return 0;
            if (name.find("Console") != std::string::npos) return 1;
            if (name.find("Material") != std::string::npos) return 2;
            if (name.find("Explorer") != std::string::npos) return 3;
            if (name.find("Properties") != std::string::npos) return 4;
            return 10;
        };
        std::sort(displayWindows.begin(), displayWindows.end(), [&](ImGuiWindow* a, ImGuiWindow* b) {
            int pa = getTabPriority(a->Name);
            int pb = getTabPriority(b->Name);
            if (pa != pb) return pa < pb;
            return std::string(a->Name) < std::string(b->Name);
        });

        for (size_t k = 0; k < displayWindows.size(); k++) {
            ImGuiWindow* sibling = displayWindows[k];
            
            // Determine tab properties based on the window name
            const char* sLabel = sibling->Name;
            const char* sIcon = "icon_folder_bold"; // default
            
            if (strstr(sLabel, "Asset Browser")) {
                sLabel = "Asset Manager";
                sIcon = "icon_folder_bold";
            } else if (strstr(sLabel, "Material Editor")) {
                sIcon = "icon_node_editor_bold";
            } else if (strstr(sLabel, "Console")) {
                sIcon = "icon_script_bold";
            } else if (strstr(sLabel, "Viewport")) {
                sIcon = "icon_3d_cube";
            } else if (strstr(sLabel, "Explorer")) {
                sIcon = "icon_explorer_bold";
            } else if (strstr(sLabel, "Properties")) {
                sIcon = "icon_properties_bold";
            } else if (strstr(sLabel, "AI Copilot")) {
                sIcon = "icon_ai_bold";
            }

            ImGuiIO& io = ImGui::GetIO();
            bool pushedFont = false;
            if (io.Fonts->Fonts.Size > 1) {
                ImGui::PushFont(io.Fonts->Fonts[1]); 
                pushedFont = true;
            }
            ImVec2 ts = ImGui::CalcTextSize(sLabel);
            if (pushedFont) ImGui::PopFont();
            
            float iconSize = 18.0f;
            float sWidth = 20.0f + 6.0f + ts.x + 10.0f;
            bool active = (sibling == window); // Active tab is the one currently rendering!
            
            // Push an absolute ID (sibling->ID) so the button's ID remains consistent 
            // even if the active window swaps during a click! This preserves drag state.
            window->IDStack.push_back(sibling->ID);
            ImGuiID btnId = window->GetID(sLabel);
            ImGui::SetCursorScreenPos(btnPos);
            ImGui::InvisibleButton(sLabel, ImVec2(sWidth, H));
            window->IDStack.pop_back();
            
            if (ImGui::IsItemClicked()) {
                ImGui::FocusWindow(sibling);
                
                if (g.ActiveId == btnId) {
                    g.ActiveIdWindow = sibling;
                }
                
                int actualIdx = -1;
                for (int i = 0; i < node->Windows.Size; i++) {
                    if (node->Windows[i] == sibling) { actualIdx = i; break; }
                }
                if (actualIdx > 0) {
                    ImGuiWindow* temp = node->Windows[0];
                    node->Windows[0] = node->Windows[actualIdx];
                    node->Windows[actualIdx] = temp;
                }
                node->VisibleWindow = sibling;
            }
            
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                if (sibling->DockNode) {
                    ImGui::DockContextQueueUndockWindow(GImGui, sibling);
                }

                ImGui::StartMouseMovingWindow(sibling);
                g.MovingWindow = sibling;

                float offsetX = g.IO.MouseClickedPos[0].x - sibling->Pos.x;
                g.ActiveIdClickOffset = ImVec2(offsetX, 0.0f);
                break;
            }

            bool hov = ImGui::IsItemHovered();
            bool held = ImGui::IsItemActive();
            
            ImVec2 tMin = btnPos;
            ImVec2 tMax = ImVec2(btnPos.x + sWidth, btnPos.y + H);
            
            if (hov && !active) {
                dl->AddRectFilled(tMin, tMax, ImGui::ColorConvertFloat4ToU32(NexusTheme::HexColorAlpha(0xFFFFFF, 0.04f)), 4.0f);
            }
            
            if (k < displayWindows.size() - 1) {
                float divW = 3.0f;
                float divH = 20.0f;
                ImVec2 divMin = ImVec2(tMax.x + 3.0f, tMin.y + 5.0f);
                ImVec2 divMax = ImVec2(divMin.x + divW, tMin.y + 25.0f);
                ImU32 divCol = ImGui::ColorConvertFloat4ToU32(NexusTheme::HexColorAlpha(0xFFFFFF, 0.12f));
                dl->AddRectFilled(divMin, divMax, divCol, 2.0f);
            }
            
            ImTextureID iconTex = IconRegistry::instance().get(sIcon);
            float iy = tMin.y + (H - iconSize) * 0.5f;
            if (iconTex) {
                ImU32 iconCol = active ? ImGui::ColorConvertFloat4ToU32(T.textPrimary) : (hov ? ImGui::ColorConvertFloat4ToU32(T.textPrimary) : ImGui::ColorConvertFloat4ToU32(T.textMuted));
                dl->AddImage(iconTex, ImVec2(tMin.x, iy), ImVec2(tMin.x + iconSize, iy + iconSize), ImVec2(0,0), ImVec2(1,1), iconCol);
            }
            
            if (pushedFont) ImGui::PushFont(io.Fonts->Fonts[1]);
            ImU32 textCol = active ? ImGui::ColorConvertFloat4ToU32(T.textPrimary) : (hov ? ImGui::ColorConvertFloat4ToU32(T.textPrimary) : ImGui::ColorConvertFloat4ToU32(T.textMuted));
            dl->AddText(ImVec2(tMin.x + iconSize + 6.0f, tMin.y + (H - ts.y)*0.5f), textCol, sLabel);
            if (pushedFont) ImGui::PopFont();
            
            btnPos.x += sWidth + 6.0f + 3.0f + 6.0f;
        }
    }
    
    ImGui::SetCursorScreenPos(ImVec2(basePos.x, basePos.y + H));
    
    // Fix: Allow dragging the panel background to show docking targets
    ImGuiWindow* currentWindow = ImGui::GetCurrentWindow();
    if (g.MovingWindow == currentWindow && g.ActiveId == currentWindow->MoveId) {
        if (!g.DragDropActive) {
            ImVec2 old_offset = g.ActiveIdClickOffset;
            g.ActiveIdClickOffset = ImVec2(0.0f, 0.0f); // Spoof to bypass ImGui's title bar check
            ImGui::BeginDockableDragDropSource(currentWindow);
            g.ActiveIdClickOffset = old_offset;
        }
    }
}

} // namespace Editor::UI
