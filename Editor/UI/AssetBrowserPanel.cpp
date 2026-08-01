#include "AssetBrowserPanel.h"
#include "../../Engine/Assets/AssetDatabase.h"
#include "../../Engine/Assets/ThumbnailCache.h"
#include "../../Engine/Assets/AssetImportPipeline.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "IconRegistry.h"
#include "NexusTheme.h"

namespace fs = std::filesystem;
namespace Editor::UI {

// ─── Renk kısayolları ───────────────────────────────────────────────────────
static ImU32 COL(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a) {
    return IM_COL32((hex>>16)&0xFF,(hex>>8)&0xFF,hex&0xFF,(uint8_t)(a*255));
}

// ─── Tab durumu ──────────────────────────────────────────────────────────────
static int s_activeTab = 0; // 0=Asset Manager, 1=Console, 2=Profiler

void AssetBrowserPanel::initialize() {
    m_currentFolder = Engine::Assets::AssetDatabase::instance().getProjectRoot() + "/Assets";
}

void AssetBrowserPanel::draw() {
    auto& T = NexusTheme::instance();

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Asset Browser");

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    float       width = ImGui::GetWindowWidth();

    // ─────────────────────────────────────────────────────────────────────────
    // HEADER (h=28) — Tab bar + sağ performans istatistikleri
    // HTML: h-7 border-b px-2 flex items-center justify-between text-xs
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.panel);
    ImGui::BeginChild("##ABHeader", ImVec2(width, 28), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 base = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(base.x, base.y + 27),
                    ImVec2(base.x + width, base.y + 27), COL(T.border));

        // Tabs
        struct TabDef { const char* label; float w; };
        static const TabDef tabs[] = {
            {" Asset Manager ", 106.0f},
            {" Output / Console ", 122.0f},
            {" Live Profiler ", 100.0f},
        };

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0, 0));

        float cx = 0.0f;
        for (int i = 0; i < 3; i++) {
            bool active = (s_activeTab == i);
            ImGui::SetCursorPos(ImVec2(cx, 0));

            ImGui::PushStyleColor(ImGuiCol_Button,
                active ? COL(T.bg) : (ImU32)0);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL(T.panelHover));

            char id[32]; snprintf(id, sizeof(id), "##abtab%d", i);
            if (ImGui::Button(id, ImVec2(tabs[i].w, 28))) s_activeTab = i;

            ImVec2 r = ImGui::GetItemRectMin();
            // Tab label
            ImU32 textCol = active ? COL(T.textPrimary) : COL(T.textMuted);
            dl->AddText(ImVec2(r.x + 8, r.y + 7), textCol, tabs[i].label);

            // Hata badge (Console tab için)
            if (i == 1) {
                float bx = r.x + tabs[i].w - 22;
                dl->AddRectFilled(ImVec2(bx, r.y+8), ImVec2(bx+18, r.y+20),
                                  COLA(0xEF4444,0.20f), 8.0f);
                dl->AddText(ImVec2(bx+5, r.y+8), COLA(0xF87171,1.0f), "0");
            }

            // Aktif: üst accent çizgi
            if (active)
                dl->AddLine(ImVec2(r.x, r.y+1), ImVec2(r.x+tabs[i].w, r.y+1),
                            COL(T.accent), 2.0f);

            ImGui::PopStyleColor(2);
            cx += tabs[i].w;
            ImGui::SameLine(0, 0);
        }
        ImGui::PopStyleVar(3);

        // Sağ: performans istatistikleri (font mono, text-studio-text)
        // HTML: Draw Calls · Ping · VRAM
        const char* stats = "Draw Calls: 142  •  Ping: 12ms  •  VRAM: 1.2/8.0 GB";
        float sw = ImGui::CalcTextSize(stats).x;
        dl->AddText(ImVec2(base.x + width - sw - 12, base.y + 7),
                    COL(T.textMuted), stats);
        // Ping değerini yeşil yap (basit üst çizim)
        const char* pingPart = "12ms";
        float pingOff = ImGui::CalcTextSize("Draw Calls: 142  •  Ping: ").x;
        dl->AddText(ImVec2(base.x + width - sw - 12 + pingOff, base.y + 7),
                    COLA(0x4ADE80, 1.0f), pingPart);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Arka plan pipeline güncellemeleri
    Engine::Assets::AssetImportPipeline::instance().update();
    Engine::Assets::ThumbnailCache::instance().processPendingRenders(2);

    if (s_activeTab != 0) {
        // Console / Profiler placeholder
        ImGui::BeginChild("##ABEmpty");
        ImGui::SetCursorPos(ImVec2(12, 12));
        if (s_activeTab == 1)
            ImGui::TextDisabled("Output log will appear here.");
        else
            ImGui::TextDisabled("Profiler data will appear here.");
        ImGui::EndChild();
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // ASSET MANAGER BODY: Sol folder tree + Sağ grid
    // HTML: flex-1 flex overflow-hidden
    // ─────────────────────────────────────────────────────────────────────────
    float bodyH  = ImGui::GetContentRegionAvail().y;
    float treeW  = 192.0f; // HTML: w-48

    // ── Sol: Folder Tree ─────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, NexusTheme::HexColorAlpha(0x050505, 0.3f));
    ImGui::BeginChild("##FolderTree", ImVec2(treeW, bodyH), false, 0);
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

        // Project root (HTML: font-bold text-white bg-studio-panelHover border)
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            float rw = treeW - 4;
            dl->AddRectFilled(p, {p.x+rw, p.y+22}, COL(T.panelHover), 4.0f);
            dl->AddRect(p, {p.x+rw, p.y+22}, COLA(0x242424,0.8f), 4.0f);
            ImTextureID foldTex = IconRegistry::instance().get("icon_folder");
            if (foldTex) dl->AddImage(foldTex,{p.x+6,p.y+4},{p.x+18,p.y+16});
            dl->AddText({p.x+22, p.y+4}, COL(T.textPrimary), "Project_Root");
            ImGui::Dummy(ImVec2(rw, 22));
        }

        // Recursive folder helper
        struct FolderNode { const char* name; bool open; bool active; int count; };
        static const FolderNode folders[] = {
            {"Content",       true,  false, 0},
            {"3D Models",     false, false, 0},
            {"Materials",     false, false, 0},
            {"Shaders (PBR)", false, true,  12},
            {"Scripts (Luau)",false, false, 0},
        };

        ImGui::SetCursorPosX(12);
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 14.0f);
        ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, COL(T.panelHover));

        // Content (ana)
        ImGuiTreeNodeFlags rootF = ImGuiTreeNodeFlags_DefaultOpen |
                                   ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_SpanAvailWidth;
        bool rootOpen = ImGui::TreeNodeEx("##content", rootF, "");
        {
            ImVec2 r = ImGui::GetItemRectMin();
            ImTextureID ft = IconRegistry::instance().get("icon_folder");
            if (ft) dl->AddImage(ft,{r.x+18,r.y+1},{r.x+30,r.y+13});
            dl->AddText({r.x+34,r.y+1}, COL(T.textPrimary), "Content");
        }

        if (rootOpen) {
            const char* subFolders[] = {"3D Models","Materials","Shaders (PBR)","Scripts (Luau)"};
            for (int i = 0; i < 4; i++) {
                bool isSel = (i == 2); // Shaders aktif (HTML'de seçili)
                ImGuiTreeNodeFlags sf = ImGuiTreeNodeFlags_Leaf |
                                        ImGuiTreeNodeFlags_SpanAvailWidth |
                                        ImGuiTreeNodeFlags_NoTreePushOnOpen;
                ImGui::TreeNodeEx(subFolders[i], sf, "");
                ImVec2 r = ImGui::GetItemRectMin();
                ImVec2 rMax = ImGui::GetItemRectMax();

                if (isSel) {
                    dl->AddRectFilled(r, rMax, COLA(0x00d2ff,0.15f), 4.0f);
                    dl->AddRect(r, rMax, COLA(0x00d2ff,0.40f), 4.0f);
                }

                ImTextureID ft = IconRegistry::instance().get("icon_folder");
                ImU32 fc = isSel ? COL(T.accent) : COL(T.textMuted);
                if (ft) dl->AddImage(ft,{r.x+18,r.y+1},{r.x+30,r.y+13},{0,0},{1,1},fc);

                ImU32 tc = isSel ? COL(T.textPrimary) : COL(T.textMuted);
                dl->AddText({r.x+34,r.y+1}, tc, subFolders[i]);

                // Count badge (Shaders'da 12)
                if (isSel) {
                    const char* cntLbl = "12";
                    float cw = ImGui::CalcTextSize(cntLbl).x + 8;
                    float bx = rMax.x - cw - 4;
                    dl->AddRectFilled({bx,r.y+2},{bx+cw,r.y+12},COLA(0x00d2ff,0.20f),4.0f);
                    dl->AddText({bx+4,r.y+1}, COL(T.accent), cntLbl);
                }
            }
            ImGui::TreePop();
        }

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Ayırıcı çizgi
    ImVec2 sepTop = ImGui::GetItemRectMin();
    sepTop.x += treeW;
    dl->AddLine(sepTop, {sepTop.x, sepTop.y + bodyH}, COL(T.border));

    ImGui::SameLine(0, 0);

    // ── Sağ: Asset Grid ───────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, NexusTheme::HexColorAlpha(0x050505, 0.6f));
    ImGui::BeginChild("##AssetGrid", ImVec2(0, bodyH), false, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
    drawAssetGrid();
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Import progress overlay ───────────────────────────────────────────────
    auto& prog = Engine::Assets::AssetImportPipeline::instance().getProgress();
    if (prog.completedAssets < prog.totalAssets) {
        ImVec2 winPos  = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();
        ImGui::SetNextWindowPos(
            {winPos.x + winSize.x - 310, winPos.y + winSize.y - 70},
            ImGuiCond_Always);
        ImGui::SetNextWindowSize({300, 55}, ImGuiCond_Always);
        if (ImGui::Begin("##ImportProg", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs)) {
            float f = (float)prog.completedAssets / (float)prog.totalAssets;
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, COL(T.accent));
            ImGui::ProgressBar(f, ImVec2(280, 18), "Importing...");
            ImGui::PopStyleColor();
            ImGui::Text("%d / %d completed",
                prog.completedAssets.load(), prog.totalAssets.load());
        }
        ImGui::End();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
// ASSET GRID
// HTML: grid grid-cols-5 gap-2, her kart h-20, border-radius
// ─────────────────────────────────────────────────────────────────────────────
void AssetBrowserPanel::drawAssetGrid() {
    auto& T  = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float cardW = 92.0f, cardH = 80.0f;
    float panelW = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, (int)(panelW / (cardW + 8.0f)));

    // Alt-asset (FBX içi) görünümü
    if (m_currentFolder.starts_with("guid:")) {
        Engine::Assets::AssetGuid parentGuid =
            Engine::Assets::AssetGuid::fromString(m_currentFolder.substr(5));
        const auto* parentMeta = Engine::Assets::AssetDatabase::instance().find(parentGuid);

        ImGui::SetCursorPosX(8);
        ImGui::PushStyleColor(ImGuiCol_Button, T.panelHover);
        if (ImGui::Button("< Back")) {
            m_currentFolder =
                Engine::Assets::AssetDatabase::instance().getProjectRoot() + "/Assets";
        }
        ImGui::PopStyleColor();

        if (parentMeta) {
            nlohmann::json settings;
            try { settings = nlohmann::json::parse(parentMeta->importSettings); }
            catch (...) {}
            if (settings.contains("subAssets")) {
                int colIndex = 0;
                for (auto& [name, guidStr] : settings["subAssets"].items()) {
                    Engine::Assets::AssetGuid sg =
                        Engine::Assets::AssetGuid::fromString(guidStr.get<std::string>());
                    ImGui::PushID(sg.toString().c_str());
                    drawSingleCard(sg, name.c_str(), "Mesh", false, cardW, cardH);
                    ImGui::PopID();
                    
                    colIndex++;
                    if (colIndex < cols) ImGui::SameLine(0, 8.0f);
                    else colIndex = 0;
                }
                if (colIndex != 0) ImGui::NewLine();
            }
        }
        return;
    }

    auto assets = Engine::Assets::AssetDatabase::instance().getAllAssets();
    
    int colIndex = 0;
    for (auto& guid : assets) {
        const auto* meta = Engine::Assets::AssetDatabase::instance().find(guid);
        if (meta && meta->importerType == "Virtual") continue;

        std::string fname = meta
            ? fs::path(meta->relativePath).filename().string()
            : "Unknown";
        std::string typeStr = meta ? meta->importerType : "?";

        ImGui::PushID(guid.toString().c_str());
        drawSingleCard(guid, fname.c_str(), typeStr.c_str(), false, cardW, cardH);
        ImGui::PopID();
        
        colIndex++;
        if (colIndex < cols) ImGui::SameLine(0, 8.0f);
        else colIndex = 0;
    }
    if (colIndex != 0) ImGui::NewLine();
}

// ─── Tek kart ────────────────────────────────────────────────────────────────
// HTML: bg-studio-panel border border-studio-border rounded-lg h-20
//       hover → hover:border-studio-accent
//       active → border-2 border-studio-accent shadow-[glow]
void AssetBrowserPanel::drawSingleCard(
    const Engine::Assets::AssetGuid& guid,
    const char* label,
    const char* typeLbl,
    bool isActive,
    float cardW,
    float cardH)
{
    auto& T  = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 p   = ImGui::GetCursorScreenPos();
    ImVec2 pMax= {p.x + cardW, p.y + cardH};

    // InvisibleButton (click/hover)
    ImGui::InvisibleButton(label, ImVec2(cardW, cardH));
    bool hov = ImGui::IsItemHovered();
    bool clk = ImGui::IsItemClicked();

    // Çift tıklama: FBX içine gir
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        const auto* meta = Engine::Assets::AssetDatabase::instance().find(guid);
        if (meta && meta->importerType == "Mesh")
            m_currentFolder = "guid:" + guid.toString();
    }

    // Drag-drop
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        Engine::Assets::AssetGuid dragGuid = guid;
        ImGui::SetDragDropPayload("ASSET_GUID", &dragGuid, sizeof(Engine::Assets::AssetGuid));
        ImGui::Text("%s", label);
        ImGui::EndDragDropSource();
    }

    // Kart arka planı
    ImU32 bgCol     = COL(T.panel);
    ImU32 borderCol = isActive ? COLA(0x00d2ff,1.0f)
                     : hov    ? COLA(0x00d2ff,0.7f)
                              : COL(T.border);
    float borderW   = isActive ? 2.0f : 1.0f;

    dl->AddRectFilled(p, pMax, bgCol, 8.0f);
    dl->AddRect(p, pMax, borderCol, 8.0f, 0, borderW);

    // Active glow
    if (isActive)
        dl->AddRectFilled({p.x-2,p.y-2},{pMax.x+2,pMax.y+2},
                          COLA(0x00d2ff,0.08f), 10.0f);

    // Thumbnail (üst orta)
    float thumbS = 32.0f;
    float thumbX = p.x + (cardW - thumbS) * 0.5f;
    float thumbY = p.y + 8.0f;

    bgfx::TextureHandle thumb =
        Engine::Assets::ThumbnailCache::instance().get(guid);

    if (thumb.idx != bgfx::kInvalidHandle) {
        dl->AddImageRounded(
            (ImTextureID)(uintptr_t)thumb.idx,
            {thumbX, thumbY}, {thumbX+thumbS, thumbY+thumbS},
            {0,0},{1,1}, IM_COL32_WHITE, thumbS * 0.4f);
    } else {
        // Tip rengi + ikon fallback
        ImU32 typeColor = COLA(0x00d2ff, 0.2f);
        const char* iconKey = "icon_mesh";
        if (std::string(typeLbl).find("Script") != std::string::npos ||
            std::string(label).ends_with(".luau"))
        { typeColor = COLA(0x14B8A6, 0.2f); iconKey = "icon_script"; }
        else if (std::string(typeLbl).find("Material") != std::string::npos ||
                 std::string(label).ends_with(".mat"))
        { typeColor = COLA(0xF59E0B, 0.2f); iconKey = "icon_material"; }
        else if (std::string(label).ends_with(".png") ||
                 std::string(label).ends_with(".jpg"))
        { typeColor = COLA(0x8B5CF6, 0.2f); iconKey = "icon_particle"; }

        dl->AddRectFilled({thumbX,thumbY},{thumbX+thumbS,thumbY+thumbS},
                          typeColor, thumbS*0.4f);
        ImTextureID icn = IconRegistry::instance().get(iconKey);
        if (icn) {
            float is = thumbS * 0.55f;
            float ix = thumbX + (thumbS-is)*0.5f;
            float iy = thumbY + (thumbS-is)*0.5f;
            dl->AddImage(icn,{ix,iy},{ix+is,iy+is},{0,0},{1,1},COL(T.textMuted));
        }
    }

    // Label (alt orta, truncated)
    std::string name = label;
    if (name.size() > 14) name = name.substr(0,11) + "...";
    float nw = ImGui::CalcTextSize(name.c_str()).x;
    dl->AddText({p.x+(cardW-nw)*0.5f, p.y+cardH-34},
        isActive ? COL(T.textPrimary) : COL(T.textPrimary), name.c_str());

    // Type label (alt, küçük renkli)
    float tw = ImGui::CalcTextSize(typeLbl).x;
    ImU32 typeTextCol = isActive ? COL(T.accent) : COL(T.textMuted);
    dl->AddText({p.x+(cardW-tw)*0.5f, p.y+cardH-20},
                typeTextCol, typeLbl);

    // Active badge (HTML: "Node Shader [Active]")
    if (isActive) {
        const char* badge = "Active";
        float bw = ImGui::CalcTextSize(badge).x + 8;
        dl->AddRectFilled({p.x+4,p.y+4},{p.x+4+bw,p.y+16},
                          COLA(0x00d2ff,0.20f), 4.0f);
        dl->AddText({p.x+8,p.y+4}, COL(T.accent), badge);
    }

    ImGui::Dummy(ImVec2(0,4)); // alt boşluk
}

} // namespace Editor::UI
