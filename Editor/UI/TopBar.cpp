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

static const float BAR_H = 30.0f;

void TopBar::draw(bool isSimulating, bool& toggleSim) {
    auto& T = NexusTheme::instance();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, BAR_H));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.bgPanel);

    ImGui::Begin("##TopBarReal", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoDocking  | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImVec2      win  = ImGui::GetWindowPos();
    float       W    = ImGui::GetWindowWidth();

    // Bottom Border
    dl->AddLine(ImVec2(win.x, win.y + BAR_H - 1.0f),
                ImVec2(win.x + W, win.y + BAR_H - 1.0f),
                COL(T.border));

    float currentX = win.x + 20.0f; // Sola hizalama için 20px padding (tahmini)

    // ─────────────────────────────────────────────────────────────────────────
    // LOGO & TITLE
    // ─────────────────────────────────────────────────────────────────────────
    {
        // Logo (20px)
        float logoS = 20.0f;
        ImVec2 pMin = ImVec2(currentX, win.y + (BAR_H - logoS) * 0.5f);
        ImVec2 pMax = ImVec2(pMin.x + logoS, pMin.y + logoS);
        
        ImTextureID logo = IconRegistry::instance().get("logo_nexus_bold");
        if (logo) {
            dl->AddImage(logo, pMin, pMax);
        } else {
            ImVec2 ts = ImGui::CalcTextSize("N");
            dl->AddText(ImVec2(pMin.x + (logoS - ts.x)*0.5f, pMin.y + (logoS - ts.y)*0.5f), COL(T.accent), "N");
        }
        
        currentX += logoS + 20.0f; // 20px gap

        // Title: "Nexus Studio" 
        ImGuiIO& io = ImGui::GetIO();
        bool pushedFont = false;
        // Font 4: New Amsterdam (18px) - if loaded
        if (io.Fonts->Fonts.Size > 4) {
            ImGui::PushFont(io.Fonts->Fonts[4]); 
            pushedFont = true;
        }

        const char* title = "Nexus";
        ImVec2 titleSize = ImGui::CalcTextSize(title);
        dl->AddText(ImVec2(currentX, win.y + (BAR_H - titleSize.y)*0.5f - 1.0f), COL(T.textPrimary), title);
        currentX += titleSize.x + 8.0f; 

        if (pushedFont) ImGui::PopFont();
        
        pushedFont = false;
        if (io.Fonts->Fonts.Size > 4) {
            ImGui::PushFont(io.Fonts->Fonts[4]); 
            pushedFont = true;
        }

        const char* studio = "Studio";
        titleSize = ImGui::CalcTextSize(studio);
        dl->AddText(ImVec2(currentX, win.y + (BAR_H - titleSize.y)*0.5f - 1.0f), COL(T.textPrimary), studio);

        if (pushedFont) ImGui::PopFont();
    }

    // ─────────────────────────────────────────────────────────────────────────
    // MERKEZ: TAB GRUBU
    // ─────────────────────────────────────────────────────────────────────────
    {
        const char* tabs[] = {"Scene", "Object", "Material", "Physic", "Plugin"};
        int numTabs = 5;
        
        ImGuiIO& io = ImGui::GetIO();
        bool pushedFont = false;
        if (io.Fonts->Fonts.Size > 2) { 
            ImGui::PushFont(io.Fonts->Fonts[2]); // Tiny font (10px, close to 9px)
            pushedFont = true;
        }
        
        float totalTabsWidth = 0.0f;
        for (int i=0; i<numTabs; i++) {
            totalTabsWidth += ImGui::CalcTextSize(tabs[i]).x;
            if (i < numTabs - 1) totalTabsWidth += 10.0f; // 10px gap
        }
        
        float startX = win.x + (W - totalTabsWidth) * 0.5f;
        float cx = startX;
        
        for (int i=0; i<numTabs; i++) {
            ImVec2 ts = ImGui::CalcTextSize(tabs[i]);
            ImVec2 tMin = ImVec2(cx, win.y);
            ImVec2 tMax = ImVec2(cx + ts.x, win.y + BAR_H);
            
            ImGui::SetCursorScreenPos(ImVec2(cx, win.y + (BAR_H - ts.y)*0.5f));
            ImGui::InvisibleButton(tabs[i], ImVec2(ts.x, ts.y));
            bool hov = ImGui::IsItemHovered();
            
            ImU32 textCol = hov ? COL(T.textPrimary) : COL(T.textMuted);
            
            dl->AddText(ImVec2(cx, win.y + (BAR_H - ts.y)*0.5f), textCol, tabs[i]);
            cx += ts.x + 10.0f;
        }
        
        if (pushedFont) ImGui::PopFont();
    }

    // ─────────────────────────────────────────────────────────────────────────
    // SAĞ: PLAY BUTONU (50x16)
    // ─────────────────────────────────────────────────────────────────────────
    {
        float playW = 50.0f;
        float playH = 16.0f;
        float rightPad = 20.0f;
        float rightX = win.x + W - rightPad - playW;
        
        ImVec2 pMin = ImVec2(rightX, win.y + (BAR_H - playH)*0.5f);
        ImVec2 pMax = ImVec2(pMin.x + playW, pMin.y + playH);

        ImGui::SetCursorScreenPos(pMin);
        ImGui::InvisibleButton("##playBtn", ImVec2(playW, playH));
        bool phov = ImGui::IsItemHovered();
        bool pclk = ImGui::IsItemClicked();
        if (pclk) toggleSim = !isSimulating;

        ImU32 playCol = phov ? COLA(0x22d3ee, 1.0f) : COL(T.accent);
        if (isSimulating) playCol = COL(T.accentRed); // Red if simulating for stop

        // border-radius: 5px
        dl->AddRectFilled(pMin, pMax, playCol, 5.0f);

        ImGuiIO& io = ImGui::GetIO();
        bool pushedFont = false;
        if (io.Fonts->Fonts.Size > 2) { 
            ImGui::PushFont(io.Fonts->Fonts[2]); // 10px 
            pushedFont = true;
        }
        
        const char* playText = isSimulating ? "Stop" : "Play";
        ImVec2 ts = ImGui::CalcTextSize(playText);
        
        dl->AddText(ImVec2(pMin.x + (playW - ts.x)*0.5f, pMin.y + (playH - ts.y)*0.5f), IM_COL32(14,14,14,255), playText);

        if (pushedFont) ImGui::PopFont();
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
}
