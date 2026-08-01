#include "AICopilotPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "EditorLayout.h"
#include "NexusTheme.h"
#include "IconRegistry.h"
#include "SelectionManager.h"
#include "Engine/Core/DataModel/Instance.h"
#include <string>
#include <vector>
#include <chrono>
#include <ctime>

// ─── Renk kısayolları ───────────────────────────────────────────────────────
static ImU32 COL(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a) {
    return IM_COL32((hex>>16)&0xFF,(hex>>8)&0xFF,hex&0xFF,(uint8_t)(a*255));
}

// ─── Mesaj yapısı ───────────────────────────────────────────────────────────
struct ChatMessage {
    bool   isUser;
    std::string text;
    std::string timeLabel;
    bool   isMcpResponse; // Özel MCP formatı?
};

static std::vector<ChatMessage> s_messages = {
    // HTML'deki başlangıç mesajları
    {true,  "Optimize roughness for realistic brushed gold.", "Emirhan • 12:44", false},
    {false, "I adjusted Roughness to 0.18 and injected micro-surface Anisotropy "
            "into the PBR graph.\n\n+ Roughness = 0.18\n+ Metallic  = 0.92",
            "Nexus AI • Just now", true},
};

static char  s_inputBuf[512] = "";
static float s_mcpPulse = 0.0f; // animasyon için

void AICopilotPanel::draw() {
    if (!EditorLayout::instance().showAICopilot) return;

    auto& T = NexusTheme::instance();
    s_mcpPulse += ImGui::GetIO().DeltaTime * 2.0f;

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("AI Copilot", &EditorLayout::instance().showAICopilot);

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    float       width = ImGui::GetWindowWidth();

    // ─────────────────────────────────────────────────────────────────────────
    // HEADER (h=28)
    // HTML: "✨ AI Copilot" + MCP pulsing dot + "Connected"
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.panel);
    ImGui::BeginChild("##AIHdr", ImVec2(width, 28), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 base = ImGui::GetCursorScreenPos();
        dl->AddLine({base.x, base.y+27},{base.x+width, base.y+27}, COL(T.border));

        // ✨ AI Copilot (beyaz, bold)
        dl->AddText({base.x+10, base.y+7}, COL(T.accent), "\xe2\x9c\xa8"); // UTF-8 ✨
        dl->AddText({base.x+26, base.y+7}, COL(T.textPrimary), "AI Copilot");

        // MCP pulse dot (sine wave alpha)
        float pulse = (sinf(s_mcpPulse) + 1.0f) * 0.5f; // 0..1
        ImU32 dotCol = IM_COL32(74, 222, 128, (uint8_t)(180 + 75*pulse)); // green-400
        dl->AddCircleFilled({base.x+width-80, base.y+14}, 4.0f, dotCol);

        // "MCP: Connected"
        dl->AddText({base.x+width-72, base.y+7}, COL(T.textMuted), "MCP: ");
        dl->AddText({base.x+width-40, base.y+7}, COL(T.textPrimary), "Connected");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ─────────────────────────────────────────────────────────────────────────
    // ACTIVE SCENE CONTEXT STRIP
    // HTML: bg-studio-bg/80, ACTIVE SCENE CONTEXT + nesne adı
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, NexusTheme::HexColorAlpha(0x050505, 0.8f));
    ImGui::BeginChild("##AICtx", ImVec2(width, 52), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 base = ImGui::GetCursorScreenPos();
        dl->AddLine({base.x, base.y+51},{base.x+width, base.y+51}, COL(T.border));

        // "ACTIVE SCENE CONTEXT:" + lang badge
        dl->AddText({base.x+8, base.y+6}, COL(T.textMuted), "ACTIVE SCENE CONTEXT:");
        const char* langLabel = "Luau / C++";
        float lw = ImGui::CalcTextSize(langLabel).x;
        dl->AddText({base.x+width-lw-8, base.y+6}, COL(T.accent), langLabel);

        // Nesne kutusu: seçili varsa gerçek ad, yoksa placeholder
        auto selected = SelectionManager::instance().getSelected();
        std::string objLabel = selected
            ? "\xf0\x9f\x93\xa6 " + selected->name + " \xe2\x80\xa2 " + selected->getClassName()
            : "\xf0\x9f\x93\xa6 Box (Gold) \xe2\x80\xa2 PBR_Gold.mat";

        float boxX  = base.x + 6;
        float boxY  = base.y + 24;
        float boxW  = width - 12;
        dl->AddRectFilled({boxX,boxY},{boxX+boxW,boxY+20}, COL(T.panel), 4.0f);
        dl->AddRect({boxX,boxY},{boxX+boxW,boxY+20}, COL(T.border), 4.0f);
        dl->AddText({boxX+8, boxY+3}, COL(T.textPrimary), objLabel.c_str());

        // MeshPart badge (sağda)
        const char* cls = selected ? selected->getClassName().c_str() : "MeshPart";
        float cw = ImGui::CalcTextSize(cls).x + 8;
        float bx = boxX + boxW - cw - 4;
        dl->AddRectFilled({bx,boxY+3},{bx+cw,boxY+17},COLA(0x00d2ff,0.10f),4.0f);
        dl->AddText({bx+4,boxY+3}, COL(T.accent), cls);

        ImGui::Dummy(ImVec2(0, 52));
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ─────────────────────────────────────────────────────────────────────────
    // CHAT HISTORY (scrollable)
    // HTML: flex-1 overflow-y-auto p-3 space-y-3
    // ─────────────────────────────────────────────────────────────────────────
    float inputH   = 44.0f;
    float chatH    = ImGui::GetContentRegionAvail().y - inputH;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, NexusTheme::HexColorAlpha(0x050505, 0.40f));
    ImGui::BeginChild("##ChatHistory", ImVec2(width, chatH), false, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 10));

    for (auto& msg : s_messages) {
        float contentW = width - 20.0f;
        float maxBubbleW = contentW * 0.90f;

        if (msg.isUser) {
            // ── Kullanıcı balonu: sağa hizalı ────────────────────────────────
            // HTML: bg-studio-panelHover border border-studio-border rounded-lg
            //       rounded-tr-none, sağda
            float tw    = ImGui::CalcTextSize(msg.text.c_str(), nullptr, false, maxBubbleW).x;
            float th    = ImGui::CalcTextSize(msg.text.c_str(), nullptr, false, maxBubbleW).y;
            float bubW  = std::min(tw + 20.0f, maxBubbleW);
            float bubH  = th + 14.0f;

            float bx = width - bubW - 10.0f;
            ImVec2 p  = ImGui::GetCursorScreenPos();
            p.x += bx;

            dl->AddRectFilled(p,{p.x+bubW,p.y+bubH}, COL(T.panelHover), 8.0f);
            // Sağ üst köşe: rounded-tr-none (köşe düzleştirme)
            dl->AddRectFilled({p.x+bubW-8,p.y},{p.x+bubW,p.y+8},COL(T.panelHover));
            dl->AddRect(p,{p.x+bubW,p.y+bubH}, COL(T.border), 8.0f);

            // Metin
            ImGui::SetCursorScreenPos({p.x+10, p.y+7});
            ImGui::PushTextWrapPos(p.x + bubW - 10);
            ImGui::TextColored(T.textPrimary, "%s", msg.text.c_str());
            ImGui::PopTextWrapPos();

            // Zaman damgası
            ImGui::Dummy(ImVec2(0, bubH - (ImGui::GetCursorScreenPos().y - p.y)));
            float tw2 = ImGui::CalcTextSize(msg.timeLabel.c_str()).x;
            ImGui::SetCursorPosX(width - tw2 - 10);
            ImGui::TextColored(T.textMuted, "%s", msg.timeLabel.c_str());

        } else if (!msg.isMcpResponse) {
            // ── Normal AI balonu: sola hizalı ────────────────────────────────
            ImVec2 p = ImGui::GetCursorScreenPos();
            p.x += 10;
            float th   = ImGui::CalcTextSize(msg.text.c_str(),nullptr,false,maxBubbleW).y;
            float bubH = th + 14.0f;
            float bubW = maxBubbleW;

            dl->AddRectFilled(p,{p.x+bubW,p.y+bubH}, COL(T.panel), 8.0f);
            dl->AddRect(p,{p.x+bubW,p.y+bubH}, COLA(0x00d2ff,0.40f), 8.0f);

            ImGui::SetCursorScreenPos({p.x+10,p.y+7});
            ImGui::PushTextWrapPos(p.x+bubW-10);
            ImGui::TextColored(T.textPrimary, "%s", msg.text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::Dummy(ImVec2(0, bubH - (ImGui::GetCursorScreenPos().y-p.y)));
            ImGui::TextColored(T.textMuted, "  %s", msg.timeLabel.c_str());

        } else {
            // ── MCP Response balonu (HTML'deki özel kart) ─────────────────────
            // HTML: bg-studio-panel border border-studio-accent/40
            //       shadow-[0_0_15px_rgba(0,210,255,0.1)]
            //       header: "⚡ MCP MATERIAL SOLVER"
            //       code block: bg-studio-bg rounded
            //       Apply butonu

            ImVec2 p   = ImGui::GetCursorScreenPos();
            p.x += 8;
            float bubW = width - 18.0f;

            // Glow arka plan
            dl->AddRectFilled(
                {p.x-3,p.y-3},{p.x+bubW+3,p.y+140},
                COLA(0x00d2ff,0.06f), 10.0f);

            // Kart
            dl->AddRectFilled(p,{p.x+bubW,p.y+138}, COL(T.panel), 8.0f);
            dl->AddRect(p,{p.x+bubW,p.y+138}, COLA(0x00d2ff,0.40f), 8.0f);

            // Header şerit: "⚡ MCP MATERIAL SOLVER" (accent, bold)
            dl->AddLine({p.x+1,p.y+24},{p.x+bubW-1,p.y+24}, COLA(0x242424,0.6f));
            dl->AddText({p.x+8,p.y+6}, COL(T.accent), "\xe2\x9a\xa1 MCP MATERIAL SOLVER");
            const char* matName = "PBR_Gold.mat";
            float mw = ImGui::CalcTextSize(matName).x;
            dl->AddText({p.x+bubW-mw-8,p.y+6}, COL(T.accent), matName);

            // Metin paragraf
            ImGui::SetCursorScreenPos({p.x+10, p.y+30});
            ImGui::PushTextWrapPos(p.x+bubW-10);
            // HTML: "I adjusted Roughness to 0.18..."
            // Roughness kısmı beyaz/bold olacak şekilde
            ImGui::TextColored(T.textMuted, "I adjusted ");
            ImGui::SameLine(0,0);
            ImGui::TextColored(T.textPrimary, "Roughness to 0.18");
            ImGui::SameLine(0,0);
            ImGui::TextColored(T.textMuted, " and injected micro-surface");
            ImGui::SetCursorScreenPos({p.x+10, ImGui::GetCursorScreenPos().y});
            ImGui::TextColored(T.textMuted, "Anisotropy into the PBR graph.");
            ImGui::PopTextWrapPos();

            // Code block (HTML: bg-studio-bg rounded font-mono text-green-400)
            ImVec2 codeP = {p.x+10, ImGui::GetCursorScreenPos().y + 4};
            float  codeW = bubW - 20.0f, codeH = 30.0f;
            dl->AddRectFilled(codeP,{codeP.x+codeW,codeP.y+codeH},
                              COL(T.bg), 4.0f);
            dl->AddRect(codeP,{codeP.x+codeW,codeP.y+codeH},
                        COL(T.border), 4.0f);
            dl->AddText({codeP.x+8,codeP.y+4},  COLA(0x4ADE80,1.0f), "+ Roughness = 0.18");
            dl->AddText({codeP.x+8,codeP.y+16}, COLA(0x4ADE80,1.0f), "+ Metallic  = 0.92");

            // Apply butonu (HTML: w-full bg-studio-accent text-black font-bold)
            ImVec2 btnP = {p.x+10, codeP.y+codeH+6};
            float  btnW = bubW-20.0f, btnH = 24.0f;

            ImGui::SetCursorScreenPos(btnP);
            ImGui::PushStyleColor(ImGuiCol_Button,        COL(T.accent));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(T.accent.x*0.9f,T.accent.y*0.9f,T.accent.z*0.9f,1));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0,0,0,1));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            if (ImGui::Button("Apply Changes to Node Graph", ImVec2(btnW, btnH))) {
                // TODO: Shader graph'a değerleri ilet
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            ImGui::Dummy(ImVec2(0, btnP.y + btnH - ImGui::GetCursorScreenPos().y + 4));
            ImGui::SetCursorPosX(10);
            ImGui::TextColored(T.textMuted, "  %s", msg.timeLabel.c_str());
        }
        ImGui::Dummy(ImVec2(0, 4)); // space-y-3 arası
    }

    // Otomatik scroll to bottom (yeni mesaj gelince)
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10)
        ImGui::SetScrollHereY(1.0f);

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ─────────────────────────────────────────────────────────────────────────
    // INPUT BAR (h=44)
    // HTML: p-2 border-t, bg-studio-bg rounded, ✨ + input + Send
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.panel);
    ImGui::BeginChild("##AIInput", ImVec2(width, inputH), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 base = ImGui::GetCursorScreenPos();
        dl->AddLine({base.x,base.y},{base.x+width,base.y}, COL(T.border));

        // Dış kutu (rounded, focus → border-accent)
        float bx = base.x+6, by = base.y+8;
        float bw = width-12, bh = 28;
        bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        dl->AddRectFilled({bx,by},{bx+bw,by+bh}, COL(T.bg), 8.0f);
        dl->AddRect({bx,by},{bx+bw,by+bh},
            focused ? COL(T.accent) : COL(T.border), 8.0f);

        // ✨ ikonu
        dl->AddText({bx+8,by+6}, COL(T.textMuted), "\xe2\x9c\xa8");

        // Text input
        ImGui::SetCursorScreenPos({bx+26, by+5});
        ImGui::PushItemWidth(bw - 80);
        ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Text,          COL(T.textPrimary));
        ImGui::InputText("##AIChatIn", s_inputBuf, sizeof(s_inputBuf),
                         ImGuiInputTextFlags_None);
        bool enterPressed = ImGui::IsItemFocused() &&
                            ImGui::IsKeyPressed(ImGuiKey_Enter);
        ImGui::PopStyleColor(3);
        ImGui::PopItemWidth();

        // Send butonu (accent ghost)
        float sendW = 44, sendH = 20;
        float sx = bx+bw-sendW-4, sy = by+4;
        ImGui::SetCursorScreenPos({sx,sy});
        ImGui::PushStyleColor(ImGuiCol_Button,        COLA(0x00d2ff,0.20f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL(T.accent));
        ImGui::PushStyleColor(ImGuiCol_Text,          COL(T.accent));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6,2));
        bool sendClicked = ImGui::Button("Send##aiSend", ImVec2(sendW, sendH));
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);

        if ((sendClicked || enterPressed) && s_inputBuf[0] != '\0') {
            // Kullanıcı mesajını ekle
            auto now = std::time(nullptr);
            char timeBuf[32];
            std::strftime(timeBuf, sizeof(timeBuf), "You • %H:%M", std::localtime(&now));

            s_messages.push_back({true, s_inputBuf, timeBuf, false});
            s_inputBuf[0] = '\0';
            // TODO: Gerçek AI çağrısı
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
}
