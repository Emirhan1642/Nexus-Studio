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
bool  s_wireframe  = false;
bool  s_collision  = false;
bool  s_worldSpace = true; // World vs Local
int   s_camSpeed   = 4;
int   s_viewMode   = 0;   // 0=Perspective 1=Top 2=Front 3=Right
int   s_litMode    = 0;   // 0=Lit(PBR) 1=Wireframe 2=Unlit

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

#include "SharedTabBar.h"

void ViewportPanel::draw(Engine::Renderer::Camera& camera) {
    auto& T = NexusTheme::instance();

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport");
    
    // Draw tab bar if there are multiple windows in this dock node
    Editor::UI::DrawSingleTabHeader("Viewport", "icon_3d_cube", 150.0f, ImGui::ColorConvertFloat4ToU32(T.accent), false);


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
    float tbH = 28.0f; // Kept for orientation cube offset
    // Viewport toolbar will be a transparent floating overlay.
    // Removed the background bar and border line.

    // Kursor'u toolbar başına koy
    ImGui::SetCursorScreenPos({panelMin.x+12, panelMin.y+6});

    // TOOLBAR COMPLETELY REMOVED AS PER USER REQUEST.
    // ALL BUTTONS AND OVERLAYS ARE DELETED.

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
