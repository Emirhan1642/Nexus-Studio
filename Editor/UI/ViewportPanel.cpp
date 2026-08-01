#define IMGUI_DEFINE_MATH_OPERATORS
#include "ViewportPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <widgets/gizmo.h>
#include "NexusTheme.h"
#include "IconRegistry.h"
#include "SelectionManager.h"
#include "../Undo/UndoStack.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Core/DataModel/DataModel.h"
#include "Engine/Core/DataModel/Constraint.h"
#include "Engine/Assets/AssetDatabase.h"
#include <functional>
#include <cstdio>

// ─── Renk kısayolları ───────────────────────────────────────────────────────
static ImU32 COL(const ImVec4& v)           { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a)    {
    return IM_COL32((hex>>16)&0xFF,(hex>>8)&0xFF,hex&0xFF,(uint8_t)(a*255));
}

// ─── Viewport durumu ─────────────────────────────────────────────────────────
static bool  s_wireframe  = false;
static bool  s_collision  = false;
static bool  s_worldSpace = true; // World vs Local
static int   s_camSpeed   = 4;
static int   s_viewMode   = 0;   // 0=Perspective 1=Top 2=Front 3=Right
static int   s_litMode    = 0;   // 0=Lit(PBR) 1=Wireframe 2=Unlit

static const char* s_viewNames[] = {"Perspective","Top","Front","Right"};
static const char* s_litNames[]  = {"Lit (PBR)","Wireframe","Unlit"};

void ViewportPanel::resize(uint16_t width, uint16_t height) {
    if (width == currentWidth && height == currentHeight) return;
    if (width == 0 || height == 0) return;

    if (bgfx::isValid(frameBuffer)) {
        bgfx::destroy(frameBuffer);
        frameBuffer   = BGFX_INVALID_HANDLE;
        colorTexture  = BGFX_INVALID_HANDLE;
        depthTexture  = BGFX_INVALID_HANDLE;
    }

    colorTexture = bgfx::createTexture2D(width, height, false, 1,
                       bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT);
    depthTexture = bgfx::createTexture2D(width, height, false, 1,
                       bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT);
    bgfx::TextureHandle att[] = {colorTexture, depthTexture};
    frameBuffer = bgfx::createFrameBuffer(2, att, true);

    currentWidth  = width;
    currentHeight = height;
}

void ViewportPanel::draw(Engine::Renderer::Camera& camera) {
    auto& T = NexusTheme::instance();

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport");

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x <= 0 || avail.y <= 0) {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    resize((uint16_t)avail.x, (uint16_t)avail.y);
    Engine::Renderer::RendererSystem::instance()
        .renderFrame(camera, currentWidth, currentHeight, frameBuffer);

    ImVec2 screenPos = ImGui::GetCursorScreenPos();
    ImGui::Image((ImTextureID)(uintptr_t)colorTexture.idx, avail);

    // ── Drag-drop asset ───────────────────────────────────────────────────────
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_GUID")) {
            Engine::Assets::AssetGuid guid = *(Engine::Assets::AssetGuid*)pl->Data;
            auto part = std::make_shared<Part>();
            part->name = "ImportedAsset";
            part->setMeshFromAsset(guid);
            part->setPosition(camera.position + camera.forward * 5.0f);
            part->setParent(DataModel::instance());
            UndoStack::instance().pushCreateCommand(part, DataModel::instance());
        }
        ImGui::EndDragDropTarget();
    }

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImVec2 panelMin  = screenPos;
    ImVec2 panelMax  = {screenPos.x + avail.x, screenPos.y + avail.y};

    // ── Constraint çizgilerini render et ─────────────────────────────────────
    {
        Engine::Math::Matrix4 view     = camera.getViewMatrix();
        Engine::Math::Matrix4 proj     = camera.getProjectionMatrix((float)currentWidth/(float)currentHeight);
        Engine::Math::Matrix4 viewProj = proj * view;

        auto project = [&](const Engine::Math::Vector3& pos, ImVec2& out) -> bool {
            float x = pos.x*viewProj.m[0]+pos.y*viewProj.m[4]+pos.z*viewProj.m[8] +viewProj.m[12];
            float y = pos.x*viewProj.m[1]+pos.y*viewProj.m[5]+pos.z*viewProj.m[9] +viewProj.m[13];
            float w = pos.x*viewProj.m[3]+pos.y*viewProj.m[7]+pos.z*viewProj.m[11]+viewProj.m[15];
            if (w < 0.001f) return false;
            x/=w; y/=w;
            out.x = screenPos.x + (x*0.5f+0.5f)*avail.x;
            out.y = screenPos.y + (1.0f-(y*0.5f+0.5f))*avail.y;
            return true;
        };

        std::function<void(const std::shared_ptr<Instance>&)> drawConstraints
            = [&](const std::shared_ptr<Instance>& inst) {
            if (auto c = std::dynamic_pointer_cast<Constraint>(inst)) {
                if (c->getVisible() && c->getEnabled()) {
                    auto p0 = std::dynamic_pointer_cast<Part>(c->getPart0());
                    auto p1 = std::dynamic_pointer_cast<Part>(c->getPart1());
                    if (p0 && p1) {
                        ImVec2 s0, s1;
                        if (project(p0->getPosition(),s0) && project(p1->getPosition(),s1)) {
                            dl->AddLine(s0, s1, IM_COL32(0,255,0,255), 3.0f);
                            dl->AddCircleFilled(s0, 5.0f, IM_COL32(255,255,0,255));
                            dl->AddCircleFilled(s1, 5.0f, IM_COL32(255,255,0,255));
                        }
                    }
                }
            }
            for (auto& child : inst->getChildren()) drawConstraints(child);
        };
        drawConstraints(DataModel::instance());
    }

    // ── Gizmo ─────────────────────────────────────────────────────────────────
    handleGizmoInput(camera);

    // ═════════════════════════════════════════════════════════════════════════
    // VIEWPORT TOOLBAR STRIP (h=28, üst overlay)
    // HTML: h-7 bg-studio-panel/90 border-b border-studio-border/60
    //       px-3 flex items-center justify-between text-[11px]
    // ═════════════════════════════════════════════════════════════════════════
    float tbH = 28.0f;
    // Yarı şeffaf arka plan
    dl->AddRectFilled(panelMin,
                      {panelMax.x, panelMin.y+tbH},
                      COLA(0x0e0e0e, 0.92f));
    dl->AddLine({panelMin.x, panelMin.y+tbH},
                {panelMax.x, panelMin.y+tbH},
                COLA(0x242424, 0.6f));

    // Kursor'u toolbar başına koy
    ImGui::SetCursorScreenPos({panelMin.x+12, panelMin.y+6});

    // ── Sol: Perspective ▾ | Lit (PBR) ▾ | World/Local toggle ────────────────
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COLA(0x171717,1.0f));

    // Perspective dropdown (tıklanabilir)
    ImGui::PushStyleColor(ImGuiCol_Text, COL(T.accent));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4,1));
    if (ImGui::Button(s_viewNames[s_viewMode]))
        ImGui::OpenPopup("##vpView");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if (ImGui::BeginPopup("##vpView")) {
        for (int i=0;i<4;i++) {
            if (ImGui::MenuItem(s_viewNames[i], nullptr, s_viewMode==i))
                s_viewMode = i;
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine(0,8);
    ImGui::PushStyleColor(ImGuiCol_Text, COLA(0x242424,1.0f));
    ImGui::TextColored(T.textMuted, "|");
    ImGui::PopStyleColor();
    ImGui::SameLine(0,8);

    // Lit dropdown
    ImGui::PushStyleColor(ImGuiCol_Text, COL(T.textPrimary));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4,1));
    if (ImGui::Button(s_litNames[s_litMode]))
        ImGui::OpenPopup("##vpLit");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if (ImGui::BeginPopup("##vpLit")) {
        for (int i=0;i<3;i++) {
            if (ImGui::MenuItem(s_litNames[i],nullptr,s_litMode==i))
                s_litMode = i;
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine(0,8);
    ImGui::TextColored(T.textMuted, "|");
    ImGui::SameLine(0,8);

    // World/Local toggle pill
    // HTML: flex bg-studio-bg border border-studio-border rounded p-0.5
    {
        ImVec2 pillP = ImGui::GetCursorScreenPos();
        float pw = 78, ph = 18;
        dl->AddRectFilled(pillP,{pillP.x+pw,pillP.y+ph}, COL(T.bg), 4.0f);
        dl->AddRect(pillP,{pillP.x+pw,pillP.y+ph}, COL(T.border), 4.0f);

        // World
        if (s_worldSpace) {
            dl->AddRectFilled({pillP.x+1,pillP.y+1},{pillP.x+39,pillP.y+ph-1},
                              COL(T.accent), 3.0f);
            dl->AddText({pillP.x+7,pillP.y+2}, IM_COL32(0,0,0,255), "World");
        } else {
            dl->AddText({pillP.x+7,pillP.y+2}, COL(T.textMuted), "World");
        }
        // Local
        if (!s_worldSpace) {
            dl->AddRectFilled({pillP.x+40,pillP.y+1},{pillP.x+pw-1,pillP.y+ph-1},
                              COL(T.accent), 3.0f);
            dl->AddText({pillP.x+46,pillP.y+2}, IM_COL32(0,0,0,255), "Local");
        } else {
            dl->AddText({pillP.x+46,pillP.y+2}, COL(T.textMuted), "Local");
        }

        ImGui::InvisibleButton("##worldPill", ImVec2(pw, ph));
        if (ImGui::IsItemClicked()) {
            float mx = ImGui::GetMousePos().x - pillP.x;
            s_worldSpace = (mx < pw/2);
        }
    }

    ImGui::PopStyleColor(2); // Button colors

    // ── Sağ: Cam speed | wireframe | collision | FPS ─────────────────────────
    float rightX = panelMax.x - 210.0f;
    ImGui::SetCursorScreenPos({rightX, panelMin.y+6});

    // Cam speed
    ImGui::TextColored(T.textMuted, "Cam:");
    ImGui::SameLine(0,4);
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        char spd[16]; snprintf(spd,sizeof(spd),"%dx  v", s_camSpeed);
        float sw2 = ImGui::CalcTextSize(spd).x + 12;
        dl->AddRectFilled(p,{p.x+sw2,p.y+16}, COL(T.bg), 4.0f);
        dl->AddRect(p,{p.x+sw2,p.y+16}, COL(T.border), 4.0f);
        dl->AddText({p.x+5,p.y+1}, COL(T.textPrimary), spd);
        ImGui::InvisibleButton("##camSpd", ImVec2(sw2,16));
    }

    ImGui::SameLine(0,8);
    ImGui::TextColored(T.textMuted, "|");
    ImGui::SameLine(0,8);

    // Wireframe toggle
    {
        ImU32 wcol = s_wireframe ? COL(T.textPrimary) : COL(T.textMuted);
        ImTextureID wireTex = IconRegistry::instance().get("icon_wireframe");
        ImVec2 bp = ImGui::GetCursorScreenPos();
        if (wireTex) {
            dl->AddImage(wireTex,bp,{bp.x+16,bp.y+16},{0,0},{1,1},wcol);
            ImGui::InvisibleButton("##wire",ImVec2(16,16));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(wcol>>16&0xFF,wcol>>8&0xFF,wcol&0xFF,1)/255.0f);
            if (ImGui::Button("W##wire",ImVec2(18,18))) s_wireframe=!s_wireframe;
            ImGui::PopStyleColor(2);
        }
        if (ImGui::IsItemClicked()) s_wireframe=!s_wireframe;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Wireframe Mode");
    }

    ImGui::SameLine(0,4);

    // Collision toggle
    {
        ImU32 ccol = s_collision ? COL(T.textPrimary) : COL(T.textMuted);
        ImTextureID colTex = IconRegistry::instance().get("icon_collision");
        ImVec2 bp = ImGui::GetCursorScreenPos();
        if (colTex) {
            dl->AddImage(colTex,bp,{bp.x+16,bp.y+16},{0,0},{1,1},ccol);
            ImGui::InvisibleButton("##coll",ImVec2(16,16));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
            if (ImGui::Button("B##coll",ImVec2(18,18))) s_collision=!s_collision;
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemClicked()) s_collision=!s_collision;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Collision Bounds");
    }

    ImGui::SameLine(0,8);
    ImGui::TextColored(T.textMuted, "|");
    ImGui::SameLine(0,8);

    // FPS dot + sayaç
    {
        ImVec2 dotP = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled({dotP.x+4, dotP.y+8}, 3.5f, COLA(0x22c55e,1.0f));
        ImGui::Dummy(ImVec2(10, 16));
        ImGui::SameLine(0,4);
    }
    ImGui::TextColored(T.textMuted, "FPS:");
    ImGui::SameLine(0,4);
    float fps = ImGui::GetIO().Framerate;
    char fpsBuf[16]; snprintf(fpsBuf,sizeof(fpsBuf),"%.1f",fps);
    ImGui::TextColored(T.textPrimary, "%s", fpsBuf);

    // ═════════════════════════════════════════════════════════════════════════
    // ORIENTATION CUBE (sağ üst köşe, top-right offset: tbH+12)
    // HTML: w-10 h-10 bg-studio-panel/90 border-studio-border rounded-lg
    //       TOP / FRONT / RIGHT etiketleri
    // ═════════════════════════════════════════════════════════════════════════
    {
        float cs = 44.0f;
        ImVec2 cMin = {panelMax.x - cs - 10, panelMin.y + tbH + 10};
        ImVec2 cMax = {cMin.x + cs, cMin.y + cs};

        dl->AddRectFilled(cMin, cMax, COLA(0x0e0e0e, 0.92f), 8.0f);
        dl->AddRect(cMin, cMax, COL(T.border), 8.0f);

        // Hover → accent border
        ImGui::SetCursorScreenPos(cMin);
        ImGui::InvisibleButton("##oriCube", ImVec2(cs, cs));
        if (ImGui::IsItemHovered())
            dl->AddRect(cMin, cMax, COL(T.accent), 8.0f);
        if (ImGui::IsItemClicked()) {
            // Sırasıyla view modunu döndür
            s_viewMode = (s_viewMode + 1) % 4;
        }

        // Etiketler (HTML: TOP muted, FRONT white, RIGHT accent)
        dl->AddText({cMin.x+12, cMin.y+5},  COL(T.textMuted),  "TOP");
        dl->AddText({cMin.x+6,  cMin.y+17}, COL(T.textPrimary),"FRONT");
        dl->AddText({cMin.x+6,  cMin.y+29}, COL(T.accent),     "RIGHT");
    }

    // ═════════════════════════════════════════════════════════════════════════
    // BOTTOM STATUS STRIP (h=20)
    // HTML footer: "Console: Shader compiled." | "Grid: 32x32 | UDP: OK"
    // ═════════════════════════════════════════════════════════════════════════
    {
        float sh = 20.0f;
        ImVec2 sMin = {panelMin.x, panelMax.y - sh};
        ImVec2 sMax = panelMax;

        dl->AddRectFilled(sMin, sMax, COL(T.panel));
        dl->AddLine(sMin, {sMax.x, sMin.y}, COL(T.border));

        // Seçili nesne adı
        auto sel = SelectionManager::instance().getSelected();
        char leftBuf[128];
        if (sel)
            snprintf(leftBuf,sizeof(leftBuf),
                     "Selected: %s [%s]", sel->name.c_str(), sel->getClassName().c_str());
        else
            snprintf(leftBuf,sizeof(leftBuf),"No selection");

        dl->AddText({sMin.x+8, sMin.y+3}, COL(T.textMuted), leftBuf);

        // Sağ: grid + network
        const char* rightInfo = "Grid: 32x32  |  UDP Replication: OK";
        float rw = ImGui::CalcTextSize(rightInfo).x;
        dl->AddText({sMax.x - rw - 8, sMin.y+3}, COL(T.textMuted), rightInfo);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void ViewportPanel::handleGizmoInput(Engine::Renderer::Camera& camera) {
    auto sel  = SelectionManager::instance().getSelected();
    auto part = std::dynamic_pointer_cast<Part>(sel);
    if (!part) return;

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y,
                      ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

    Engine::Math::Matrix4 transform =
        Engine::Math::Matrix4::fromPositionAndSize(part->getPosition(), part->getSize());
    Engine::Math::Matrix4 view = camera.getViewMatrix();
    Engine::Math::Matrix4 proj = camera.getProjectionMatrix(
        (float)currentWidth / (float)currentHeight);

    static ImGuizmo::OPERATION currentOp = ImGuizmo::TRANSLATE;
    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) currentOp = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) currentOp = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) currentOp = ImGuizmo::SCALE;
    }

    ImGuizmo::Manipulate(view.m.data(), proj.m.data(),
                         currentOp, ImGuizmo::WORLD, transform.m.data());

    if (ImGuizmo::IsUsing()) {
        if (!isDraggingGizmo) {
            isDraggingGizmo    = true;
            dragStartPosition  = part->getPosition();
        }
        part->setPosition(transform.getTranslation());
    } else {
        if (isDraggingGizmo) {
            UndoStack::instance().pushPropertyChangeCommand(
                part, "Position", dragStartPosition, part->getPosition());
            isDraggingGizmo = false;
        }
    }
}
