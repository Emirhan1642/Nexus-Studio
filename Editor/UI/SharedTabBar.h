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
            ImGui::FocusWindow(window);
            if (node) {
                ImGui::DockContextQueueUndockWindow(GImGui, window);
            }
            ImGuiWindow* old_root = window->RootWindow;
            ImGuiWindow* old_root_title = window->RootWindowForTitleBarHighlight;
            ImGuiWindow* old_root_nav = window->RootWindowForNav;
            ImGuiWindow* old_root_dock = window->RootWindowDockTree;

            window->RootWindow = window;
            window->RootWindowForTitleBarHighlight = window;
            window->RootWindowForNav = window;
            window->RootWindowDockTree = window;

            ImGui::StartMouseMovingWindow(window);

            window->RootWindow = old_root;
            window->RootWindowForTitleBarHighlight = old_root_title;
            window->RootWindowForNav = old_root_nav;
            window->RootWindowDockTree = old_root_dock;
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
        std::sort(displayWindows.begin(), displayWindows.end(), [](ImGuiWindow* a, ImGuiWindow* b) {
            return std::string(a->Name) < std::string(b->Name);
        });

        for (size_t k = 0; k < displayWindows.size(); k++) {
            ImGuiWindow* sibling = displayWindows[k];
            
            // Determine tab properties based on the window name
            const char* sLabel = sibling->Name;
            const char* sIcon = "icon_folder_bold"; // default
            
            if (strstr(sLabel, "Asset Browser")) {
                sIcon = "icon_folder_bold";
            } else if (strstr(sLabel, "Material Editor")) {
                sIcon = "icon_node_editor_bold";
            } else if (strstr(sLabel, "Console")) {
                sIcon = "icon_script_bold";
            } else if (strstr(sLabel, "Viewport")) {
                sIcon = "icon_3d_cube"; // Assuming this icon exists, if not it will just be text.
            }

            ImGuiIO& io = ImGui::GetIO();
            bool pushedFont = false;
            if (io.Fonts->Fonts.Size > 1) {
                ImGui::PushFont(io.Fonts->Fonts[1]); 
                pushedFont = true;
            }
            ImVec2 ts = ImGui::CalcTextSize(sLabel);
            if (pushedFont) ImGui::PopFont();
            
            float sWidth = 10.0f + 20.0f + 10.0f + ts.x + 10.0f;
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
                
                // Hack: Transfer the ActiveId ownership to the newly activated window.
                // Otherwise, ImGui sees the OLD window become hidden and brutally cancels our Drag state!
                if (g.ActiveId == btnId) {
                    g.ActiveIdWindow = sibling;
                }
                
                // When HiddenTabBar is used, ImGui forces VisibleWindow to Windows[0].
                // We must swap this clicked window to index 0 so it actually becomes visible!
                int actualIdx = -1;
                for (int i = 0; i < node->Windows.Size; i++) {
                    if (node->Windows[i] == sibling) { actualIdx = i; break; }
                }
                if (actualIdx > 0) {
                    ImGuiWindow* temp = node->Windows[0];
                    node->Windows[0] = node->Windows[actualIdx];
                    node->Windows[actualIdx] = temp;
                }
            }
            
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                ImGui::FocusWindow(sibling);
                if (sibling->DockNode) {
                    ImGui::DockContextQueueUndockWindow(GImGui, sibling);
                }
                ImGuiWindow* old_root = sibling->RootWindow;
                ImGuiWindow* old_root_title = sibling->RootWindowForTitleBarHighlight;
                ImGuiWindow* old_root_nav = sibling->RootWindowForNav;
                ImGuiWindow* old_root_dock = sibling->RootWindowDockTree;

                sibling->RootWindow = sibling;
                sibling->RootWindowForTitleBarHighlight = sibling;
                sibling->RootWindowForNav = sibling;
                sibling->RootWindowDockTree = sibling;

                ImGui::StartMouseMovingWindow(sibling);

                sibling->RootWindow = old_root;
                sibling->RootWindowForTitleBarHighlight = old_root_title;
                sibling->RootWindowForNav = old_root_nav;
                sibling->RootWindowDockTree = old_root_dock;
                break; // Break to avoid iterating on modified DockNode structure
            }

            bool hov = ImGui::IsItemHovered();
            bool held = ImGui::IsItemActive();
            
            ImVec2 tMin = btnPos;
            ImVec2 tMax = ImVec2(btnPos.x + sWidth, btnPos.y + H);
            
            // Transparent background

            
            if (k < displayWindows.size() - 1) {
                float divW = 3.0f;
                float divH = 20.0f;
                ImVec2 divMin = ImVec2(tMax.x + 3.5f, tMin.y + (H - divH) * 0.5f);
                ImVec2 divMax = ImVec2(divMin.x + divW, divMin.y + divH);
                ImU32 divCol = IM_COL32(255, 255, 255, 25);
                dl->AddRectFilled(divMin, divMax, divCol, 3.0f);
            }
            
            ImTextureID iconTex = IconRegistry::instance().get(sIcon);
            float ix = tMin.x + 10.0f;
            float iy = tMin.y + (H - 20.0f) * 0.5f;
            if (iconTex) {
                ImU32 iconCol = active ? ImGui::ColorConvertFloat4ToU32(T.textPrimary) : ImGui::ColorConvertFloat4ToU32(T.textMuted);
                dl->AddImage(iconTex, ImVec2(ix, iy), ImVec2(ix + 20.0f, iy + 20.0f), ImVec2(0,0), ImVec2(1,1), iconCol);
            }
            
            if (pushedFont) ImGui::PushFont(io.Fonts->Fonts[1]);
            
            ImU32 textCol = active ? ImGui::ColorConvertFloat4ToU32(T.textPrimary) : ImGui::ColorConvertFloat4ToU32(T.textSecondary);
            dl->AddText(ImVec2(tMin.x + 40.0f, tMin.y + (H - ts.y)*0.5f), textCol, sLabel);
            
            if (pushedFont) ImGui::PopFont();
            
            btnPos.x += sWidth + 10.0f;
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
