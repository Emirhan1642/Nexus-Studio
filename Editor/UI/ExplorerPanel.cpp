#include "ExplorerPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "SelectionManager.h"
#include "../Undo/UndoStack.h"
#include "Engine/Core/DataModel/DataModel.h"
#include "Engine/Core/Reflection/TypeRegistry.h"
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Scripting/Script.h"
#include "IconRegistry.h"
#include "NexusTheme.h"

// ─── Renk kısayolları ───────────────────────────────────────────────────────
static ImU32 COL(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a) {
    return IM_COL32((hex>>16)&0xFF,(hex>>8)&0xFF,hex&0xFF,(uint8_t)(a*255));
}

// ─── Aktif tab durumu ───────────────────────────────────────────────────────
static int s_activeTab = 0; // 0=Explorer, 1=World, 2=History

// ─── Sınıf → ikon/renk tablosu ─────────────────────────────────────────────
struct ClassMeta { const char* iconKey; ImU32 barColor; };

static ClassMeta getClassMeta(const std::string& cls) {
    if (cls == "Part"           ) return {"icon_mesh",      COLA(0x00d2ff, 1.0f)}; // cyan
    if (cls == "Script"         ) return {"icon_script",    COLA(0x2DD4BF, 1.0f)}; // teal
    if (cls == "Camera"         ) return {"icon_camera",    COLA(0xF97316, 1.0f)}; // orange
    if (cls == "DirectionalLight") return {"icon_light_dir", COLA(0xFACC15, 1.0f)}; // yellow
    if (cls == "ParticleSystem" ) return {"icon_particle",  COLA(0xFBBF24, 1.0f)}; // amber
    if (cls == "Workspace"      ) return {"icon_world",     COLA(0x9CA3AF, 1.0f)}; // grey
    if (cls == "Folder"         ) return {"icon_folder",    COLA(0x6B7280, 1.0f)}; // grey
    return                               {"icon_mesh",      COLA(0x6B7280, 1.0f)};
}

// ─── Toggle pill (HTML'den ─────────────────────────────────────────────────
static void DrawTogglePill(ImDrawList* dl, ImVec2 p, bool value) {
    float w = 24.0f, h = 14.0f;
    auto& T = NexusTheme::instance();
    ImU32 bg = value ? COL(T.toggleOn) : IM_COL32(36,36,36,255);
    dl->AddRectFilled(p, {p.x+w, p.y+h}, bg, h*0.5f);
    float cx = value ? p.x + w - 7 : p.x + 7;
    dl->AddCircleFilled({cx, p.y + h*0.5f}, 5.0f, IM_COL32(255,255,255,255));
}

// ─── Küçük ikon çizici ──────────────────────────────────────────────────────
static void DrawInlineIcon(ImDrawList* dl, const char* key, ImVec2 pos, float size, ImU32 col) {
    ImTextureID tex = IconRegistry::instance().get(key);
    if (tex)
        dl->AddImage(tex, pos, {pos.x+size, pos.y+size}, {0,0},{1,1}, col);
}

void ExplorerPanel::draw() {
    auto& T = NexusTheme::instance();

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Explorer");

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    float       width = ImGui::GetWindowWidth();

    // ─────────────────────────────────────────────────────────────────────────
    // HEADER (h=28, tab bar)
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.panel);
    ImGui::BeginChild("##ExplHdr", ImVec2(width, 28), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 base = ImGui::GetCursorScreenPos();
        // Alt border
        dl->AddLine(ImVec2(base.x, base.y + 27), ImVec2(base.x + width, base.y + 27), COL(T.border));

        struct TabDef { const char* label; float w; };
        static const TabDef tabDefs[] = {
            {" v Explorer ", 82.0f},
            {" World ",      58.0f},
            {" History ",    62.0f}
        };

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0,0));

        float cx = 0.0f;
        for (int i = 0; i < 3; i++) {
            bool active = (s_activeTab == i);
            ImGui::SetCursorPos(ImVec2(cx, 0));

            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, T.bg);
                ImGui::PushStyleColor(ImGuiCol_Text,   T.textPrimary);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                ImGui::PushStyleColor(ImGuiCol_Text,   T.textMuted);
            }

            char id[32]; snprintf(id, sizeof(id), "##etab%d", i);
            if (ImGui::Button(id, ImVec2(tabDefs[i].w, 28))) s_activeTab = i;

            // Tab label metin
            ImVec2 r = ImGui::GetItemRectMin();
            dl->AddText(ImVec2(r.x + 6, r.y + 7),
                active ? COL(T.textPrimary) : COL(T.textMuted), tabDefs[i].label);

            // Aktif: üst accent çizgi
            if (active)
                dl->AddLine(ImVec2(r.x, r.y + 1), ImVec2(r.x + tabDefs[i].w, r.y + 1),
                            COL(T.accent), 2.0f);

            ImGui::PopStyleColor(2);
            cx += tabDefs[i].w;
            ImGui::SameLine(0,0);
        }
        ImGui::PopStyleVar(3);

        // + ve ⋯ butonları (sağa hizalı)
        ImGui::SetCursorPos(ImVec2(width - 48, 4));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Text,   T.textMuted);
        if (ImGui::Button("+##eadd", ImVec2(20, 20)))
            ImGui::OpenPopup("ExplorerInsertPopup");
        ImGui::SameLine(0, 2);
        ImGui::Button("...##emor", ImVec2(20, 20));
        ImGui::PopStyleColor(2);

        if (ImGui::BeginPopup("ExplorerInsertPopup")) {
            drawInsertObjectMenu(DataModel::instance());
            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ─────────────────────────────────────────────────────────────────────────
    // TREE BODY
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.panel);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
    ImGui::BeginChild("##ExplBody", ImVec2(width, ImGui::GetContentRegionAvail().y), false, 0);

    drawInstanceNode(DataModel::instance());

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
}

void ExplorerPanel::drawInstanceNode(const std::shared_ptr<Instance>& inst) {
    auto& T = NexusTheme::instance();

    bool isSelected   = (inst == SelectionManager::instance().getSelected());
    bool isDataModel  = (inst == DataModel::instance());
    const auto& cls   = inst->getClassName();
    ClassMeta meta    = getClassMeta(cls);
    bool hasChildren  = !inst->getChildren().empty();

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow    |
        ImGuiTreeNodeFlags_SpanAvailWidth |
        ImGuiTreeNodeFlags_FramePadding;

    if (isDataModel) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    // ── Satır arka planı (seçili veya hover) ──────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Header,       isSelected ? COLA(0x00d2ff, 0.15f) : (ImU32)0);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, COL(T.panelHover));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  COL(T.panelHover));

    bool open = ImGui::TreeNodeEx((void*)inst.get(), flags, "");

    ImGui::PopStyleColor(3);

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImVec2 rowMin    = ImGui::GetItemRectMin();
    ImVec2 rowMax    = ImGui::GetItemRectMax();

    // ── Seçim vurgusu (HTML: bg-studio-accent/15 + border accent/40 + sol çizgi) ──
    if (isSelected) {
        dl->AddRectFilled(rowMin, rowMax, COLA(0x00d2ff, 0.15f), 4.0f);
        dl->AddRect(rowMin, rowMax, COLA(0x00d2ff, 0.40f), 4.0f, 0, 1.0f);
        // Sol 2px accent çizgi
        dl->AddLine(ImVec2(rowMin.x + 2, rowMin.y + 3),
                    ImVec2(rowMin.x + 2, rowMax.y - 3),
                    COL(T.accent), 2.0f);
    }

    // ── Tip renk çubuğu (3px) ─────────────────────────────────────────────
    if (!isDataModel) {
        float barX = rowMin.x + ImGui::GetTreeNodeToLabelSpacing() - 12.0f;
        dl->AddRectFilled(ImVec2(barX, rowMin.y + 3),
                          ImVec2(barX + 3, rowMax.y - 3),
                          meta.barColor, 1.5f);
    }

    // ── İkon + İsim ────────────────────────────────────────────────────────
    ImGui::SameLine(0, 4);
    float iconY = ImGui::GetCursorPosY() + (rowMax.y - rowMin.y - 14)*0.5f;
    ImVec2 iconPos = ImGui::GetCursorScreenPos();
    iconPos.y = rowMin.y + (rowMax.y - rowMin.y - 14)*0.5f;

    DrawInlineIcon(dl, meta.iconKey, iconPos, 14.0f,
        isSelected ? COL(T.textPrimary) : COL(T.textMuted));

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 18.0f);
    ImGui::SameLine(0, 18);
    ImGui::TextColored(
        isSelected ? T.textPrimary : T.textMuted,
        "%s", inst->name.c_str());

    // ── Sağ: "PBR" badge (seçili Part için) ───────────────────────────────
    if (isSelected && cls == "Part") {
        ImVec2 badgePos = ImVec2(rowMax.x - 38, rowMin.y + 3);
        dl->AddRectFilled(badgePos, ImVec2(badgePos.x+32, badgePos.y+16),
                          COLA(0x00d2ff, 0.20f), 4.0f);
        dl->AddText(ImVec2(badgePos.x + 6, badgePos.y + 2), COL(T.accent), "PBR");
    }

    // ── Click seçim ────────────────────────────────────────────────────────
    if (ImGui::IsItemClicked()) {
        SelectionManager::instance().select(inst);
    }

    // ── Context menu ───────────────────────────────────────────────────────
    if (ImGui::BeginPopupContextItem(inst->name.c_str())) {
        drawInsertObjectMenu(inst);
        if (!isDataModel) {
            ImGui::Separator();
            if (ImGui::MenuItem("Rename"))   { /* TODO */ }
            if (ImGui::MenuItem("Duplicate")){ /* TODO */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                inst->setParent(nullptr);
                SelectionManager::instance().clear();
            }
        }
        ImGui::EndPopup();
    }

    // ── Alt düğümler ───────────────────────────────────────────────────────
    if (open && hasChildren) {
        for (auto& child : inst->getChildren())
            drawInstanceNode(child);
        ImGui::TreePop();
    }
}

void ExplorerPanel::drawInsertObjectMenu(const std::shared_ptr<Instance>& parent) {
    if (ImGui::BeginMenu("Insert Object")) {
        if (ImGui::MenuItem("Part")) {
            auto obj = std::make_shared<Part>();
            UndoStack::instance().pushCreateCommand(obj, parent);
        }
        if (ImGui::MenuItem("Script")) {
            auto obj = std::make_shared<Script>();
            UndoStack::instance().pushCreateCommand(obj, parent);
        }
        ImGui::EndMenu();
    }
}
