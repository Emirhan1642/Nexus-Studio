#include "TopBar.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "IconRegistry.h"
#include "NexusTheme.h"
#include "EditorLayout.h"
#include "Engine/Networking/Transport/NetworkContext.h"
#include "Engine/Networking/Transport/NetworkServer.h"
#include "Engine/Networking/Transport/NetworkClient.h"

static ImU32 COL(const ImVec4& v)  { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a = 1.0f) {
    return IM_COL32((hex>>16)&0xFF, (hex>>8)&0xFF, hex&0xFF, (uint8_t)(a*255));
}

static const float BAR_H = 44.0f; // HTML uses h-11 which is 44px

void TopBar::draw(bool isSimulating, bool& toggleSim) {
    auto& T = NexusTheme::instance();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, BAR_H));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.panel);

    ImGui::Begin("##TopBarReal", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoDocking  | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImVec2      win  = ImGui::GetWindowPos();
    float       W    = ImGui::GetWindowWidth();

    // Bottom Border
    dl->AddLine(ImVec2(win.x, win.y + BAR_H - 1.0f),
                ImVec2(win.x + W, win.y + BAR_H - 1.0f),
                COL(T.border));

    float currentX = win.x + 16.0f; // px-4 = 16px

    // ─────────────────────────────────────────────────────────────────────────
    // LOGO & TITLE
    // ─────────────────────────────────────────────────────────────────────────
    {
        // Logo Box: w-8 h-8 rounded-lg bg-studio-accent/20 border border-studio-accent/40 shadow-[glow]
        float logoS = 32.0f;
        ImVec2 pMin = ImVec2(currentX, win.y + (BAR_H - logoS) * 0.5f);
        ImVec2 pMax = ImVec2(pMin.x + logoS, pMin.y + logoS);
        
        // Glow shadow
        dl->AddRectFilled(ImVec2(pMin.x - 3, pMin.y - 3), ImVec2(pMax.x + 3, pMax.y + 3), COLA(0x00d2ff, 0.2f), 10.0f);
        dl->AddRectFilled(pMin, pMax, COLA(0x00d2ff, 0.2f), 8.0f);
        dl->AddRect(pMin, pMax, COLA(0x00d2ff, 0.4f), 8.0f);

        // "V" Icon
        ImTextureID logo = IconRegistry::instance().get("logo_nexus");
        if (logo) {
            dl->AddImage(logo, ImVec2(pMin.x+4, pMin.y+4), ImVec2(pMax.x-4, pMax.y-4));
        } else {
            ImVec2 ts = ImGui::CalcTextSize("N");
            dl->AddText(ImVec2(pMin.x + (logoS - ts.x)*0.5f, pMin.y + (logoS - ts.y)*0.5f), COL(T.accent), "N");
        }
        
        currentX += logoS + 12.0f;

        // VIBE Engine Title (bold white)
        const char* title = "NEXUS Studio";
        ImVec2 titleSize = ImGui::CalcTextSize(title);
        dl->AddText(ImVec2(currentX, win.y + (BAR_H - titleSize.y)*0.5f), COL(T.textPrimary), title);
        
        currentX += titleSize.x + 24.0f;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // MENUS
    // ─────────────────────────────────────────────────────────────────────────
    {
        struct MenuEntry { const char* label; const char* popup; };
        static const MenuEntry menus[] = {
            {"File","##mFile"}, {"Edit","##mEdit"}, {"View","##mView"},
            {"Scene","##mScene"}, {"Object","##mObj"}, {"Material","##mMat"},
            {"Physics","##mPhys"}, {"Plugins","##mPlug"}, {"Networking","##mNet"}
        };

        ImGui::SetCursorScreenPos(ImVec2(currentX, win.y));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL(T.panelHover));
        ImGui::PushStyleColor(ImGuiCol_Text, COL(T.textMuted));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        
        for (auto& m : menus) {
            float tw = ImGui::CalcTextSize(m.label).x;
            ImGui::SetCursorPosY((BAR_H - 28) * 0.5f);
            if (ImGui::Button(m.label, ImVec2(tw + 16, 28)))
                ImGui::OpenPopup(m.popup);
            
            // Hover effect on text
            if (ImGui::IsItemHovered()) {
                ImVec2 rMin = ImGui::GetItemRectMin();
                ImVec2 rMax = ImGui::GetItemRectMax();
                dl->AddText(ImVec2(rMin.x + 8, rMin.y + (28 - ImGui::CalcTextSize(m.label).y)*0.5f), COL(T.textPrimary), m.label);
            }
            ImGui::SameLine(0, 4);
        }
        
        currentX = ImGui::GetCursorScreenPos().x;
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        // Popups (simplified for layout)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, COL(T.panel));
        if (ImGui::BeginPopup("##mFile")) { ImGui::MenuItem("New Project"); ImGui::EndPopup(); }
        if (ImGui::BeginPopup("##mEdit")) { ImGui::MenuItem("Undo"); ImGui::EndPopup(); }
        if (ImGui::BeginPopup("##mView")) {
            auto& L = EditorLayout::instance();
            ImGui::MenuItem("Left Toolbar",   nullptr, &L.showLeftToolbar);
            ImGui::MenuItem("Material Editor",nullptr, &L.showMaterialEditor);
            ImGui::MenuItem("Asset Browser",  nullptr, &L.showAssetBrowser);
            ImGui::MenuItem("AI Copilot",     nullptr, &L.showAICopilot);
            ImGui::EndPopup();
        }
        // ... (other popups)
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    currentX += 16.0f;

    // ─────────────────────────────────────────────────────────────────────────
    // AI COPILOT BUTTON
    // ─────────────────────────────────────────────────────────────────────────
    {
        const char* aiText = "✨ AI Copilot [MCP]";
        float aiW = ImGui::CalcTextSize(aiText).x + 24.0f;
        float aiH = 28.0f;
        ImVec2 btnP = ImVec2(currentX, win.y + (BAR_H - aiH) * 0.5f);

        ImGui::SetCursorScreenPos(btnP);
        ImGui::InvisibleButton("##aiCopilotBtn", ImVec2(aiW, aiH));
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        
        if (clk) EditorLayout::instance().showAICopilot = !EditorLayout::instance().showAICopilot;

        // Glow & bg
        dl->AddRectFilled(ImVec2(btnP.x - 2, btnP.y - 2), ImVec2(btnP.x + aiW + 2, btnP.y + aiH + 2), COLA(0x00d2ff, hov ? 0.2f : 0.1f), 6.0f);
        dl->AddRectFilled(btnP, ImVec2(btnP.x + aiW, btnP.y + aiH), COLA(0x00d2ff, hov ? 0.2f : 0.1f), 4.0f);
        dl->AddRect(btnP, ImVec2(btnP.x + aiW, btnP.y + aiH), COLA(0x00d2ff, 0.3f), 4.0f);

        ImVec2 ts = ImGui::CalcTextSize(aiText);
        dl->AddText(ImVec2(btnP.x + (aiW - ts.x)*0.5f, btnP.y + (aiH - ts.y)*0.5f), COL(T.accent), aiText);

        currentX += aiW + 16.0f;
    }

    // Vertical Separator
    dl->AddLine(ImVec2(currentX, win.y + 12.0f), ImVec2(currentX, win.y + BAR_H - 12.0f), COL(T.border));
    currentX += 16.0f;

    // ─────────────────────────────────────────────────────────────────────────
    // TABS
    // ─────────────────────────────────────────────────────────────────────────
    {
        struct TabDef { const char* id; const char* label; bool active; };
        static const TabDef tabs[] = {
            {"##tab0", "3D Viewport",         true},
            {"##tab1", "PBR_Gold.mat [Node]", false},
            {"##tab2", "MainCharacter.luau",  false},
        };

        for (int i = 0; i < 3; i++) {
            auto& t = tabs[i];
            float tw = ImGui::CalcTextSize(t.label).x + 40.0f;
            ImVec2 tMin = ImVec2(currentX, win.y);
            ImVec2 tMax = ImVec2(currentX + tw, win.y + BAR_H);

            ImGui::SetCursorScreenPos(tMin);
            ImGui::InvisibleButton(t.id, ImVec2(tw, BAR_H));
            bool hov = ImGui::IsItemHovered();

            if (t.active) {
                // Gradient background (simulated with solid for now, or multi-rect)
                dl->AddRectFilledMultiColor(tMin, tMax, 
                                            COLA(0x00d2ff, 0.1f), COLA(0x00d2ff, 0.1f),
                                            COLA(0x00d2ff, 0.0f), COLA(0x00d2ff, 0.0f));
                // Top border accent
                dl->AddLine(tMin, ImVec2(tMax.x, tMin.y), COL(T.accent), 2.0f);
            } else if (hov) {
                dl->AddRectFilled(tMin, tMax, COL(T.panelHover));
            }

            // Icon + Label
            float cx = tMin.x + 12.0f;
            const char* iconKey = (i==0) ? nullptr : (i==1 ? "icon_material" : "icon_script");
            ImTextureID icn = iconKey ? IconRegistry::instance().get(iconKey) : (ImTextureID)nullptr;
            
            if (icn) {
                dl->AddImage(icn, ImVec2(cx, win.y + 14), ImVec2(cx + 16, win.y + 30), ImVec2(0,0), ImVec2(1,1),
                             t.active ? COL(T.accent) : COL(T.textMuted));
                cx += 24.0f;
            } else if (i == 0) {
                // Active cyan dot
                dl->AddCircleFilled(ImVec2(cx + 4, win.y + 22), 4.0f, t.active ? COL(T.accent) : COL(T.textMuted));
                cx += 16.0f;
            }

            ImVec2 ts = ImGui::CalcTextSize(t.label);
            dl->AddText(ImVec2(cx, win.y + (BAR_H - ts.y)*0.5f), 
                        t.active ? COL(T.textPrimary) : COL(T.textMuted), t.label);

            // Right border for tabs
            dl->AddLine(ImVec2(tMax.x, tMin.y + 12), ImVec2(tMax.x, tMax.y - 12), COL(T.border));

            currentX += tw;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // RIGHT SECTION: Build Target + Play/Stop
    // ─────────────────────────────────────────────────────────────────────────
    {
        float targetW = 250.0f;
        float rightX = win.x + W - targetW;
        
        // Separator before right section
        dl->AddLine(ImVec2(rightX - 16.0f, win.y + 12.0f), ImVec2(rightX - 16.0f, win.y + BAR_H - 12.0f), COL(T.border));

        // "Target:" label
        ImVec2 ts1 = ImGui::CalcTextSize("Target:");
        dl->AddText(ImVec2(rightX, win.y + (BAR_H - ts1.y)*0.5f), COL(T.textMuted), "Target:");
        rightX += ts1.x + 8.0f;

        // PC / DX12 Dropdown (bg-studio-bg border)
        float dropW = 90.0f, dropH = 24.0f;
        ImVec2 dMin = ImVec2(rightX, win.y + (BAR_H - dropH)*0.5f);
        ImVec2 dMax = ImVec2(dMin.x + dropW, dMin.y + dropH);
        
        dl->AddRectFilled(dMin, dMax, COL(T.bg), 4.0f);
        dl->AddRect(dMin, dMax, COL(T.border), 4.0f);
        
        ImVec2 ts2 = ImGui::CalcTextSize("PC / DX12");
        dl->AddText(ImVec2(dMin.x + 8.0f, dMin.y + (dropH - ts2.y)*0.5f), COL(T.textPrimary), "PC / DX12");
        // Chevron down
        dl->AddText(ImVec2(dMax.x - 20.0f, dMin.y + (dropH - ts2.y)*0.5f), COL(T.textMuted), "v");

        ImGui::SetCursorScreenPos(dMin);
        ImGui::InvisibleButton("##targetDrop", ImVec2(dropW, dropH));

        rightX += dropW + 16.0f;

        // Play Simulation button (cyan glow)
        const char* playText = isSimulating ? "Stop" : "Play Simulation";
        float playW = ImGui::CalcTextSize(playText).x + 40.0f;
        float playH = 28.0f;
        ImVec2 pMin = ImVec2(rightX, win.y + (BAR_H - playH)*0.5f);
        ImVec2 pMax = ImVec2(pMin.x + playW, pMin.y + playH);

        ImGui::SetCursorScreenPos(pMin);
        ImGui::InvisibleButton("##playBtn", ImVec2(playW, playH));
        bool phov = ImGui::IsItemHovered();
        bool pclk = ImGui::IsItemClicked();
        if (pclk) toggleSim = true;

        ImU32 playCol = phov ? COLA(0x22d3ee, 1.0f) : COL(T.accent); // hover: cyan-400
        
        // Strong glow
        dl->AddRectFilled(ImVec2(pMin.x - 4, pMin.y - 4), ImVec2(pMax.x + 4, pMax.y + 4), COLA(0x00d2ff, 0.4f), 10.0f);
        dl->AddRectFilled(pMin, pMax, playCol, 6.0f);

        // Play Icon (triangle) or Stop Icon (square)
        float icnY = pMin.y + playH*0.5f;
        if (!isSimulating) {
            dl->AddTriangleFilled(ImVec2(pMin.x + 12, icnY - 5),
                                  ImVec2(pMin.x + 12, icnY + 5),
                                  ImVec2(pMin.x + 22, icnY), IM_COL32(0,0,0,255));
        } else {
            dl->AddRectFilled(ImVec2(pMin.x + 12, icnY - 4), ImVec2(pMin.x + 20, icnY + 4), IM_COL32(0,0,0,255), 1.0f);
        }

        ImVec2 ts3 = ImGui::CalcTextSize(playText);
        dl->AddText(ImVec2(pMin.x + 28.0f, pMin.y + (playH - ts3.y)*0.5f), IM_COL32(0,0,0,255), playText);
    }

    ImGui::End();
    ImGui::PopStyleColor();  // WindowBg
    ImGui::PopStyleVar(3);
}
