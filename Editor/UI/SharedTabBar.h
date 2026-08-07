#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include "NexusTheme.h"
#include "IconRegistry.h"
#include "EditorLayout.h"
#include <string>

namespace Editor::UI {

inline void DrawBottomTabBar(const char* activeTab) {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 basePos = ImGui::GetCursorScreenPos(); 
    
    float H = 34.0f; // Tab bar height
    float W = ImGui::GetWindowWidth();
    
    // Draw background for the tab bar area
    dl->AddRectFilled(basePos, ImVec2(basePos.x + W, basePos.y + H), ImGui::ColorConvertFloat4ToU32(T.bgPanel));
    
    // Draw bottom divider
    dl->AddLine(ImVec2(basePos.x, basePos.y + H - 1.0f), ImVec2(basePos.x + W, basePos.y + H - 1.0f), ImGui::ColorConvertFloat4ToU32(T.border));

    struct Tab { const char* label; const char* icon; float width; ImU32 indicatorCol; };
    std::vector<Tab> tabs;
    if (!EditorLayout::instance().isAssetBrowserTornOff)
        tabs.push_back({"Asset Browser", "icon_folder_bold", 150.0f, ImGui::ColorConvertFloat4ToU32(T.accent)});
    if (!EditorLayout::instance().isMaterialEditorTornOff)
        tabs.push_back({"Material Editor", "icon_node_editor_bold", 160.0f, ImGui::ColorConvertFloat4ToU32(T.accentGreen)});
    if (!EditorLayout::instance().isConsoleTornOff)
        tabs.push_back({"Console", "icon_script_bold", 110.0f, ImGui::ColorConvertFloat4ToU32(T.textMuted)});
    
    ImVec2 btnPos = basePos;
    for (size_t i = 0; i < tabs.size(); i++) {
        auto& t = tabs[i];
        bool active = (strcmp(t.label, activeTab) == 0);
        
        // This registers the clickable area properly with ImGui's layout system
        ImGui::InvisibleButton(t.label, ImVec2(t.width, H));
        
        // Accept drag and drop to restore torn off panels
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("_IMWINDOW")) {
                ImGuiWindow* droppedWindow = *(ImGuiWindow**)payload->Data;
                if (strcmp(droppedWindow->Name, "Asset Browser") == 0) {
                    EditorLayout::instance().isAssetBrowserTornOff = false;
                    EditorLayout::instance().activeBottomTab = "Asset Browser";
                } else if (strcmp(droppedWindow->Name, "Material Editor") == 0) {
                    EditorLayout::instance().isMaterialEditorTornOff = false;
                    EditorLayout::instance().activeBottomTab = "Material Editor";
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::IsItemClicked()) {
            EditorLayout::instance().activeBottomTab = t.label;
        }
        
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 2.0f)) {
            if (strcmp(t.label, "Material Editor") == 0) {
                EditorLayout::instance().isMaterialEditorTornOff = true;
                EditorLayout::instance().justTornOffMaterialEditor = true;
                EditorLayout::instance().tearOffPos = ImGui::GetMousePos();
                EditorLayout::instance().wantStartMovingMaterialEditorFromTab = true;
                if (EditorLayout::instance().activeBottomTab == "Material Editor") EditorLayout::instance().activeBottomTab = "Asset Browser";
                ImGui::ClearActiveID(); // Release the button drag state
            } else if (strcmp(t.label, "Asset Browser") == 0) {
                EditorLayout::instance().isAssetBrowserTornOff = true;
                EditorLayout::instance().justTornOffAssetBrowser = true;
                EditorLayout::instance().tearOffPos = ImGui::GetMousePos();
                EditorLayout::instance().wantStartMovingAssetBrowserFromTab = true;
                if (EditorLayout::instance().activeBottomTab == "Asset Browser") EditorLayout::instance().activeBottomTab = "Material Editor";
                ImGui::ClearActiveID(); // Release the button drag state
            }
        }
        
        bool hov = ImGui::IsItemHovered();
        bool held = ImGui::IsItemActive();
        
        ImVec2 tMin = btnPos;
        ImVec2 tMax = ImVec2(btnPos.x + t.width, btnPos.y + H);
        
        // Background
        if (active) {
            dl->AddRectFilled(tMin, tMax, ImGui::ColorConvertFloat4ToU32(T.bgDeepest));
            dl->AddRectFilled(tMin, ImVec2(tMax.x, tMin.y + 2.0f), t.indicatorCol);
        } else if (held) {
            dl->AddRectFilled(tMin, tMax, ImGui::ColorConvertFloat4ToU32(T.border)); // Clear visual feedback on click
        } else if (hov) {
            dl->AddRectFilled(tMin, tMax, ImGui::ColorConvertFloat4ToU32(T.bgCard));
        }
        
        // Right border (vertical divider)
        dl->AddLine(ImVec2(tMax.x, tMin.y + 6.0f), ImVec2(tMax.x, tMax.y - 6.0f), ImGui::ColorConvertFloat4ToU32(T.border));
        
        // Icon (14x14)
        ImTextureID icon = IconRegistry::instance().get(t.icon);
        float ix = tMin.x + 8.0f;
        float iy = tMin.y + (H - 14.0f) * 0.5f;
        
        if (icon) {
            ImU32 iconCol = active ? ImGui::ColorConvertFloat4ToU32(T.accent) : ImGui::ColorConvertFloat4ToU32(T.textMuted);
            dl->AddImage(icon, ImVec2(ix, iy), ImVec2(ix + 14.0f, iy + 14.0f), ImVec2(0,0), ImVec2(1,1), iconCol);
        }
        
        // Text
        ImGuiIO& io = ImGui::GetIO();
        bool pushedFont = false;
        if (io.Fonts->Fonts.Size > 2) {
            ImGui::PushFont(io.Fonts->Fonts[2]); 
            pushedFont = true;
        }
        
        ImVec2 ts = ImGui::CalcTextSize(t.label);
        ImU32 textCol = active ? ImGui::ColorConvertFloat4ToU32(T.textPrimary) : ImGui::ColorConvertFloat4ToU32(T.textSecondary);
        dl->AddText(ImVec2(tMin.x + 28.0f, tMin.y + (H - ts.y)*0.5f), textCol, t.label);
        
        if (pushedFont) ImGui::PopFont();
        
        btnPos.x += t.width;
        if (i < tabs.size() - 1) {
            ImGui::SameLine(0.0f, 0.0f);
        }
    }
    
    // Fill remaining space with a drop target to restore torn off panels
    float remainingWidth = W - (btnPos.x - basePos.x);
    if (remainingWidth > 0.0f) {
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::InvisibleButton("##BottomPanelEmptySpace", ImVec2(remainingWidth, H));
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("_IMWINDOW")) {
                ImGuiWindow* droppedWindow = *(ImGuiWindow**)payload->Data;
                if (strcmp(droppedWindow->Name, "Asset Browser") == 0) {
                    EditorLayout::instance().isAssetBrowserTornOff = false;
                    EditorLayout::instance().activeBottomTab = "Asset Browser";
                } else if (strcmp(droppedWindow->Name, "Material Editor") == 0) {
                    EditorLayout::instance().isMaterialEditorTornOff = false;
                    EditorLayout::instance().activeBottomTab = "Material Editor";
                }
            }
            ImGui::EndDragDropTarget();
        }
    }
    
    // Move cursor below the tab bar for the actual window content
    ImGui::SetCursorScreenPos(ImVec2(basePos.x, basePos.y + H));
}

inline void DrawSingleTabHeader(const char* label, const char* icon, float width, ImU32 indicatorCol) {
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiDockNode* node = window->DockNode;
    
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 basePos = ImGui::GetCursorScreenPos(); 
    
    float H = 34.0f;
    float W = ImGui::GetWindowWidth();
    
    // Draw background for the tab bar area
    dl->AddRectFilled(basePos, ImVec2(basePos.x + W, basePos.y + H), ImGui::ColorConvertFloat4ToU32(T.bgPanel));
    dl->AddLine(ImVec2(basePos.x, basePos.y + H - 1.0f), ImVec2(basePos.x + W, basePos.y + H - 1.0f), ImGui::ColorConvertFloat4ToU32(T.border));
    
    // If not docked or alone, just draw the single header
    if (!node || node->Windows.Size <= 1) {
        ImGui::InvisibleButton(label, ImVec2(W, H));
        
        // Start moving the window when this custom header is dragged (deferred to Main.cpp)
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            if (strcmp(label, "Asset Browser") == 0) {
                EditorLayout::instance().wantStartMovingAssetBrowser = true;
            } else if (strcmp(label, "Material Editor") == 0) {
                EditorLayout::instance().wantStartMovingMaterialEditor = true;
            }
            ImGui::ClearActiveID();
        }
        
        ImVec2 tMin = basePos;
        ImVec2 tMax = ImVec2(basePos.x + width, basePos.y + H);
        
        // Background
        dl->AddRectFilled(tMin, tMax, ImGui::ColorConvertFloat4ToU32(T.bgDeepest));
        // Line right
        dl->AddLine(ImVec2(tMax.x, tMin.y), ImVec2(tMax.x, tMax.y), ImGui::ColorConvertFloat4ToU32(T.border));
        // Top indicator
        dl->AddRectFilled(tMin, ImVec2(tMax.x, tMin.y + 2.0f), indicatorCol);

        // Icon (14x14)
        ImTextureID iconTex = IconRegistry::instance().get(icon);
        float ix = tMin.x + 8.0f;
        float iy = tMin.y + (H - 14.0f) * 0.5f;
        if (iconTex) {
            dl->AddImage(iconTex, ImVec2(ix, iy), ImVec2(ix + 14.0f, iy + 14.0f), ImVec2(0,0), ImVec2(1,1), indicatorCol);
        }

        // Text
        ImGuiIO& io = ImGui::GetIO();
        bool pushedFont = false;
        if (io.Fonts->Fonts.Size > 2) {
            ImGui::PushFont(io.Fonts->Fonts[2]); 
            pushedFont = true;
        }
        
        ImVec2 ts = ImGui::CalcTextSize(label);
        ImU32 textCol = ImGui::ColorConvertFloat4ToU32(T.textPrimary);
        dl->AddText(ImVec2(tMin.x + 28.0f, tMin.y + (H - ts.y)*0.5f), textCol, label);
        
        if (pushedFont) ImGui::PopFont();
    } else {
        // Draw multiple custom tabs for all sibling windows in this dock node!
        ImVec2 btnPos = basePos;
        for (int i = 0; i < node->Windows.Size; i++) {
            ImGuiWindow* sibling = node->Windows[i];
            
            // Determine tab properties based on the window name
            const char* sLabel = sibling->Name;
            const char* sIcon = "icon_folder_bold"; // default
            float sWidth = 150.0f;
            ImU32 sIndicatorCol = ImGui::ColorConvertFloat4ToU32(T.textMuted);
            
            if (strcmp(sLabel, "Asset Browser") == 0) {
                sIcon = "icon_folder_bold";
                sWidth = 150.0f;
                sIndicatorCol = ImGui::ColorConvertFloat4ToU32(T.accent);
            } else if (strcmp(sLabel, "Material Editor") == 0) {
                sIcon = "icon_node_editor_bold";
                sWidth = 160.0f;
                sIndicatorCol = ImGui::ColorConvertFloat4ToU32(T.accentGreen);
            } else if (strcmp(sLabel, "Console") == 0) {
                sIcon = "icon_script_bold";
                sWidth = 110.0f;
                sIndicatorCol = ImGui::ColorConvertFloat4ToU32(T.textMuted);
            }

            bool active = (sibling == window); // Active tab is the one currently rendering!
            
            ImGui::InvisibleButton(sLabel, ImVec2(sWidth, H));
            if (ImGui::IsItemClicked()) {
                ImGui::FocusWindow(sibling);
            }
            
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                if (strcmp(sLabel, "Asset Browser") == 0) {
                    EditorLayout::instance().wantStartMovingAssetBrowser = true;
                } else if (strcmp(sLabel, "Material Editor") == 0) {
                    EditorLayout::instance().wantStartMovingMaterialEditor = true;
                }
                ImGui::ClearActiveID();
            }

            bool hov = ImGui::IsItemHovered();
            bool held = ImGui::IsItemActive();
            
            ImVec2 tMin = btnPos;
            ImVec2 tMax = ImVec2(btnPos.x + sWidth, btnPos.y + H);
            
            if (active) {
                dl->AddRectFilled(tMin, tMax, ImGui::ColorConvertFloat4ToU32(T.bgDeepest));
                dl->AddRectFilled(tMin, ImVec2(tMax.x, tMin.y + 2.0f), sIndicatorCol);
            } else if (held) {
                dl->AddRectFilled(tMin, tMax, ImGui::ColorConvertFloat4ToU32(T.border));
            } else if (hov) {
                dl->AddRectFilled(tMin, tMax, ImGui::ColorConvertFloat4ToU32(T.bgCard));
            }
            
            dl->AddLine(ImVec2(tMax.x, tMin.y + 6.0f), ImVec2(tMax.x, tMax.y - 6.0f), ImGui::ColorConvertFloat4ToU32(T.border));
            
            ImTextureID iconTex = IconRegistry::instance().get(sIcon);
            float ix = tMin.x + 8.0f;
            float iy = tMin.y + (H - 14.0f) * 0.5f;
            if (iconTex) {
                ImU32 iconCol = active ? sIndicatorCol : ImGui::ColorConvertFloat4ToU32(T.textMuted);
                dl->AddImage(iconTex, ImVec2(ix, iy), ImVec2(ix + 14.0f, iy + 14.0f), ImVec2(0,0), ImVec2(1,1), iconCol);
            }
            
            ImGuiIO& io = ImGui::GetIO();
            bool pushedFont = false;
            if (io.Fonts->Fonts.Size > 2) {
                ImGui::PushFont(io.Fonts->Fonts[2]); 
                pushedFont = true;
            }
            
            ImVec2 ts = ImGui::CalcTextSize(sLabel);
            ImU32 textCol = active ? ImGui::ColorConvertFloat4ToU32(T.textPrimary) : ImGui::ColorConvertFloat4ToU32(T.textSecondary);
            dl->AddText(ImVec2(tMin.x + 28.0f, tMin.y + (H - ts.y)*0.5f), textCol, sLabel);
            
            if (pushedFont) ImGui::PopFont();
            
            btnPos.x += sWidth;
            if (i < node->Windows.Size - 1) {
                ImGui::SameLine(0.0f, 0.0f);
            }
        }
    }
    
    ImGui::SetCursorScreenPos(ImVec2(basePos.x, basePos.y + H));
}

} // namespace Editor::UI
