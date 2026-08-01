#include "TopBar.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "IconRegistry.h"
#include "NexusTheme.h"
#include "EditorLayout.h"
#include "Engine/Networking/Transport/NetworkContext.h"
#include "Engine/Networking/Transport/NetworkServer.h"
#include "Engine/Networking/Transport/NetworkClient.h"

// ─── Renk kısayolları ──────────────────────────────────────────────────────
static ImU32 COL(const ImVec4& v)  { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a = 1.0f) {
    return IM_COL32((hex>>16)&0xFF, (hex>>8)&0xFF, hex&0xFF, (uint8_t)(a*255));
}

static const float BAR_H = 40.0f;

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

    // ── Alt border ──────────────────────────────────────────────────────────
    dl->AddLine(ImVec2(win.x, win.y + BAR_H - 1.0f),
                ImVec2(win.x + W, win.y + BAR_H - 1.0f),
                COL(T.border));

    // ── Yardımcı lambda: dikey ayırıcı çiz ─────────────────────────────────
    auto vSep = [&](float x) {
        dl->AddLine(ImVec2(win.x + x, win.y + 8),
                    ImVec2(win.x + x, win.y + BAR_H - 8),
                    COL(T.border));
    };

    // ─────────────────────────────────────────────────────────────────────────
    // SOL BÖLÜM: Logo + Menüler + AI Butonu
    // ─────────────────────────────────────────────────────────────────────────

    // Logo kutucuğu (HTML: "V" ikonu, #00d2ff, glow)
    {
        ImVec2 logoMin = ImVec2(win.x + 8,  win.y + 10);
        ImVec2 logoMax = ImVec2(win.x + 28, win.y + 30);
        dl->AddRectFilled(logoMin, logoMax, COL(T.accent), 4.0f);
        // Glow
        dl->AddRectFilled(ImVec2(logoMin.x - 2, logoMin.y - 2),
                          ImVec2(logoMax.x + 2, logoMax.y + 2),
                          COLA(0x00d2ff, 0.08f), 6.0f);

        ImTextureID logo = IconRegistry::instance().get("logo_nexus");
        if (logo) {
            dl->AddImage(logo, logoMin, logoMax);
        } else {
            // "N" harfi fallback
            ImVec2 tc = ImVec2((logoMin.x + logoMax.x) * 0.5f - 4,
                               (logoMin.y + logoMax.y) * 0.5f - 7);
            dl->AddText(tc, IM_COL32(0, 0, 0, 255), "N");
        }
    }

    // "NEXUS" yazısı
    {
        ImVec2 textPos = ImVec2(win.x + 36, win.y + 13);
        dl->AddText(textPos, IM_COL32(255, 255, 255, 255), "NEXUS");
    }

    // Logo bölümü sağ border
    float afterLogo = 80.0f;
    vSep(afterLogo);

    // ── Menüler ─────────────────────────────────────────────────────────────
    // ImGui menüleri için kursor konumlandır
    ImGui::SetCursorPos(ImVec2(afterLogo + 6, 0));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 0));
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, T.panelHover);
    ImGui::PushStyleColor(ImGuiCol_Text,          T.textMuted);

    // Menü butonları: basınca popup aç
    struct MenuEntry { const char* label; const char* popup; };
    static const MenuEntry menus[] = {
        {"File","##mFile"}, {"Edit","##mEdit"}, {"View","##mView"},
        {"Scene","##mScene"}, {"Object","##mObj"}, {"Material","##mMat"},
        {"Physics","##mPhys"}, {"Plugins","##mPlug"}, {"Networking","##mNet"}
    };

    for (auto& m : menus) {
        ImGui::SetCursorPosY(0);
        if (ImGui::Button(m.label, ImVec2(0, BAR_H)))
            ImGui::OpenPopup(m.popup);
        ImGui::SameLine();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);

    // ── Popuplar ────────────────────────────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, T.panel);

    if (ImGui::BeginPopup("##mFile")) {
        if (ImGui::MenuItem("New Project"))  {}
        if (ImGui::MenuItem("Open Project")) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Save"))  {}
        if (ImGui::MenuItem("Save As…")) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Exit"))  {}
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##mEdit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
        if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Cut"))   {}
        if (ImGui::MenuItem("Copy"))  {}
        if (ImGui::MenuItem("Paste")) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Select All")) {}
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##mView")) {
        auto& L = EditorLayout::instance();
        ImGui::MenuItem("Left Toolbar",   nullptr, &L.showLeftToolbar);
        ImGui::MenuItem("Material Editor",nullptr, &L.showMaterialEditor);
        ImGui::MenuItem("Asset Browser",  nullptr, &L.showAssetBrowser);
        ImGui::MenuItem("AI Copilot",     nullptr, &L.showAICopilot);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) L.loadPreset("Default");
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##mScene"))  { ImGui::MenuItem("Scene Settings"); ImGui::EndPopup(); }
    if (ImGui::BeginPopup("##mObj"))    { ImGui::MenuItem("Add Part"); ImGui::MenuItem("Add Light"); ImGui::EndPopup(); }
    if (ImGui::BeginPopup("##mMat"))    { ImGui::MenuItem("New Material"); ImGui::MenuItem("Material Editor"); ImGui::EndPopup(); }
    if (ImGui::BeginPopup("##mPhys"))   { ImGui::MenuItem("Physics Settings"); ImGui::EndPopup(); }
    if (ImGui::BeginPopup("##mPlug"))   { ImGui::MenuItem("Plugin Manager"); ImGui::EndPopup(); }
    if (ImGui::BeginPopup("##mNet")) {
        auto mode = Engine::Networking::NetworkContext::mode();
        if (ImGui::MenuItem("Host Server", nullptr,
            mode == Engine::Networking::NetworkMode::Server)) {
            Engine::Networking::NetworkContext::setMode(Engine::Networking::NetworkMode::Server);
            Engine::Networking::NetworkServer::instance().start(7777);
        }
        if (ImGui::MenuItem("Connect (localhost)", nullptr,
            mode == Engine::Networking::NetworkMode::Client)) {
            Engine::Networking::NetworkContext::setMode(Engine::Networking::NetworkMode::Client);
            Engine::Networking::NetworkClient::instance().connect("127.0.0.1", 7777);
        }
        if (ImGui::MenuItem("Disconnect / Stop")) {
            if (mode == Engine::Networking::NetworkMode::Server)
                Engine::Networking::NetworkServer::instance().stop();
            else
                Engine::Networking::NetworkClient::instance().disconnect();
            Engine::Networking::NetworkContext::setMode(Engine::Networking::NetworkMode::Standalone);
        }
        ImGui::EndPopup();
    }

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // ── AI Assistant butonu (accent, siyah yazı, glow) ───────────────────
    float aiX = ImGui::GetCursorPosX() + 6;
    ImGui::SetCursorPos(ImVec2(aiX, (BAR_H - 22) * 0.5f));
    {
        ImVec2 bMin = ImVec2(win.x + aiX - 2, win.y + (BAR_H - 24) * 0.5f);
        // Glow arkaya
        dl->AddRectFilled(ImVec2(bMin.x - 2, bMin.y - 2),
                          ImVec2(bMin.x + 164, bMin.y + 28),
                          COLA(0x00d2ff, 0.12f), 6.0f);
    }
    ImGui::PushStyleColor(ImGuiCol_Button,        T.accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(T.accent.x*0.9f, T.accent.y*0.9f, T.accent.z*0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0,0,0,1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 3));
    if (ImGui::Button("✨ AI Assistant [MCP]")) {
        EditorLayout::instance().showAICopilot = !EditorLayout::instance().showAICopilot;
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    // ─────────────────────────────────────────────────────────────────────────
    // ORTA BÖLÜM: IDE Document Tabs
    // ─────────────────────────────────────────────────────────────────────────

    // Sol border
    float tabsStart = ImGui::GetCursorPosX() + 10;
    vSep(tabsStart - 4);
    ImGui::SameLine(0, 0);

    // Tab yardımcı: active tab
    struct TabDef { const char* id; const char* label; bool active; bool hasAccent; };
    static const TabDef tabs[] = {
        {"##tab0", " 3D Viewport",         true,  true  },
        {"##tab1", " PBR_Gold.mat [Node]",  false, true  },
        {"##tab2", " MainCharacter.luau",   false, false },
    };

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

    for (int i = 0; i < 3; i++) {
        auto& t = tabs[i];
        float tw = (i == 0) ? 108 : (i == 1 ? 168 : 148);

        ImGui::SetCursorPosY(0);
        if (t.active) {
            ImGui::PushStyleColor(ImGuiCol_Button,  T.bg);
            ImGui::PushStyleColor(ImGuiCol_Text,    T.textPrimary);
        } else if (t.hasAccent) {
            ImGui::PushStyleColor(ImGuiCol_Button,  NexusTheme::HexColorAlpha(0x171717, 0.4f));
            ImGui::PushStyleColor(ImGuiCol_Text,    T.accent);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,  T.panel);
            ImGui::PushStyleColor(ImGuiCol_Text,    T.textMuted);
        }

        ImGui::Button(t.id, ImVec2(tw, BAR_H));

        // Gerçek label (ikon + metin)
        {
            ImVec2 r = ImGui::GetItemRectMin();
            const char* iconKey = (i==0) ? nullptr : (i==1 ? "icon_material" : "icon_script");
            ImTextureID icn = iconKey ? IconRegistry::instance().get(iconKey) : (ImTextureID)nullptr;
            float cx = r.x + 10;
            if (icn) {
                dl->AddImage(icn, ImVec2(cx, r.y + 13), ImVec2(cx + 14, r.y + 27));
                cx += 18;
            } else if (i == 0) {
                // Aktif dot
                dl->AddCircleFilled(ImVec2(cx + 3, r.y + 20), 3.5f,
                    t.active ? COL(T.accent) : COLA(0x00d2ff, 0.5f));
                cx += 10;
            }
            dl->AddText(ImVec2(cx, r.y + 13), t.active ? COL(T.textPrimary)
                        : (t.hasAccent ? COL(T.accent) : COL(T.textMuted)),
                        t.label + 1); // +1: baştaki boşluğu atla

            // Üst accent çizgi
            float lineA = t.active ? 1.0f : 0.4f;
            dl->AddLine(ImVec2(r.x, r.y + 1),
                        ImVec2(r.x + tw, r.y + 1),
                        t.active || t.hasAccent ? COLA(0x00d2ff, lineA) : COLA(0x242424, 0.0f),
                        2.0f);
        }

        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 0);
    }

    ImGui::PopStyleVar(3);

    // Sağ border
    float tabsEnd = ImGui::GetCursorPosX();
    vSep(tabsEnd + 2);

    // ─────────────────────────────────────────────────────────────────────────
    // SAĞ BÖLÜM: Build Target + Play/Stop
    // ─────────────────────────────────────────────────────────────────────────
    float rightW = 220.0f;
    float rightX = W - rightW;
    ImGui::SetCursorPos(ImVec2(rightX, (BAR_H - 22) * 0.5f));

    // "Target:" label
    ImGui::PushStyleColor(ImGuiCol_Text, T.textMuted);
    ImGui::Text("Target:");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 6);

    // Build target dropdown
    ImGui::PushStyleColor(ImGuiCol_Button,  T.bg);
    ImGui::PushStyleColor(ImGuiCol_Text,    T.textPrimary);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(8, 3));
    ImGui::Button("PC / DX12  v", ImVec2(90, 22));
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 10);

    // Play / Stop butonu — glow efekti
    {
        ImVec2 bPos  = ImGui::GetCursorScreenPos();
        ImVec2 bSize = ImVec2(58, 22);
        // Glow
        dl->AddRectFilled(ImVec2(bPos.x - 3, bPos.y - 3),
                          ImVec2(bPos.x + bSize.x + 3, bPos.y + bSize.y + 3),
                          COLA(0x00d2ff, 0.15f), 6.0f);
    }
    ImGui::PushStyleColor(ImGuiCol_Button,        T.accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(T.accent.x*0.9f,T.accent.y*0.9f,T.accent.z*0.9f,1));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0,0,0,1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(8, 3));

    // Play ikonu (SVG'den dönüştürülen DrawList üçgeni)
    const char* playLabel = isSimulating ? "  Stop" : "  Play";
    if (ImGui::Button(playLabel, ImVec2(58, 22))) {
        toggleSim = true;
    }
    // Play üçgeni
    if (!isSimulating) {
        ImVec2 br = ImGui::GetItemRectMin();
        dl->AddTriangleFilled(
            ImVec2(br.x + 10, br.y + 5),
            ImVec2(br.x + 10, br.y + 17),
            ImVec2(br.x + 20, br.y + 11),
            IM_COL32(0, 0, 0, 255));
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    ImGui::End();
    ImGui::PopStyleColor();  // WindowBg
    ImGui::PopStyleVar(3);
}
