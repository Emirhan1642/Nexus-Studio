#define IMGUI_DEFINE_MATH_OPERATORS
#include "ViewportPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <widgets/gizmo.h>
#include "NexusTheme.h"
#include "IconRegistry.h"
#include "SelectionManager.h"
#include "EditorLayout.h"
#include "../Undo/UndoStack.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Core/DataModel/DataModel.h"
#include "Engine/Core/DataModel/Constraint.h"
#include "Engine/Assets/AssetDatabase.h"
#include "Engine/Core/Math/Quaternion.h"
#include "ModelingOperatorPanel.h"
#include "Editor/Modeling/ModelingContext.h"
#include <functional>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <set>
#include <vector>
#include <map>

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
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_HiddenTabBar;
    ImGui::SetNextWindowClass(&window_class);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoMove);

    // Draw tab bar if there are multiple windows in this dock node
    Editor::UI::DrawSingleTabHeader("Viewport", "icon_3d_cube", 150.0f, ImGui::ColorConvertFloat4ToU32(T.accent), false);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 16.0f || avail.y < 16.0f) {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    resize((uint16_t)avail.x, (uint16_t)avail.y);
    Engine::Renderer::RendererSystem::instance()
        .setShadingMode((Engine::Renderer::ShadingMode)EditorLayout::instance().shadingMode);
    Engine::Renderer::RendererSystem::instance()
        .renderFrame(camera, currentWidth, currentHeight, frameBuffer);

    ImVec2 screenPos = ImGui::GetCursorScreenPos();
    if (bgfx::isValid(colorTexture)) {
        ImGui::Image((ImTextureID)(uintptr_t)colorTexture.idx, avail);
    }

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

    bool isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

    Engine::Math::Matrix4 view     = camera.getViewMatrix();
    Engine::Math::Matrix4 proj     = camera.getProjectionMatrix((float)currentWidth/(float)currentHeight);
    Engine::Math::Matrix4 viewProj = proj * view;

    auto project = [&](const Engine::Math::Vector3& pos, ImVec2& out) -> bool {
        float x = pos.x*viewProj.m[0]+pos.y*viewProj.m[4]+pos.z*viewProj.m[8] +viewProj.m[12];
        float y = pos.x*viewProj.m[1]+pos.y*viewProj.m[5]+pos.z*viewProj.m[9] +viewProj.m[13];
        float w = pos.x*viewProj.m[3]+pos.y*viewProj.m[7]+pos.z*viewProj.m[11]+viewProj.m[15];
        if (w < 0.001f && !camera.isOrthographic) return false;
        if (camera.isOrthographic) w = 1.0f;
        x/=w; y/=w;
        out.x = screenPos.x + (x*0.5f+0.5f)*avail.x;
        out.y = screenPos.y + (1.0f-(y*0.5f+0.5f))*avail.y;
        return true;
    };

    // ── Render 3D Cursor ──────────────────────────────────────────────────────
    {
        ImVec2 cursorScreen;
        if (project(EditorLayout::instance().cursor3DPosition, cursorScreen)) {
            float r = 13.0f;
            dl->AddCircle(cursorScreen, r, IM_COL32(230, 40, 40, 255), 32, 2.0f);
            dl->AddCircle(cursorScreen, r - 3.0f, IM_COL32(255, 255, 255, 200), 32, 1.2f);

            dl->AddLine(ImVec2(cursorScreen.x - r - 6.0f, cursorScreen.y),
                        ImVec2(cursorScreen.x + r + 6.0f, cursorScreen.y),
                        IM_COL32(40, 40, 40, 220), 2.5f);
            dl->AddLine(ImVec2(cursorScreen.x - r - 6.0f, cursorScreen.y),
                        ImVec2(cursorScreen.x + r + 6.0f, cursorScreen.y),
                        IM_COL32(255, 255, 255, 255), 1.2f);

            dl->AddLine(ImVec2(cursorScreen.x, cursorScreen.y - r - 6.0f),
                        ImVec2(cursorScreen.x, cursorScreen.y + r + 6.0f),
                        IM_COL32(40, 40, 40, 220), 2.5f);
            dl->AddLine(ImVec2(cursorScreen.x, cursorScreen.y - r - 6.0f),
                        ImVec2(cursorScreen.x, cursorScreen.y + r + 6.0f),
                        IM_COL32(255, 255, 255, 255), 1.2f);

            dl->AddCircleFilled(cursorScreen, 2.5f, IM_COL32(255, 50, 50, 255));
        }
    }

    ImVec2 mousePos = ImGui::GetIO().MousePos;
    auto& mCtx = Editor::Modeling::ModelingContext::instance();

    // ── Blender / Nexus Modeling Shortcuts (Guarded against Camera WASDEQ) ──
    if (!ImGui::GetIO().WantTextInput && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        auto selPart = std::dynamic_pointer_cast<Part>(SelectionManager::instance().getSelected());

        // Tab: Toggle Object / Edit (Face) Mode
        if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
            if (EditorLayout::instance().shadingMode == EditorShadingMode::Object) {
                EditorLayout::instance().shadingMode = EditorShadingMode::Face;
                if (selPart) selPart->ensureEditableMesh();
            } else {
                EditorLayout::instance().shadingMode = EditorShadingMode::Object;
                if (mCtx.activeModal != Editor::Modeling::ModalTool::None) mCtx.cancelModal();
            }
        }

        // 1, 2, 3: Vertex, Edge, Face Mode shortcuts
        if (ImGui::IsKeyPressed(ImGuiKey_1)) {
            EditorLayout::instance().shadingMode = EditorShadingMode::Vertex;
            if (selPart) selPart->ensureEditableMesh();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_2)) {
            EditorLayout::instance().shadingMode = EditorShadingMode::Edge;
            if (selPart) selPart->ensureEditableMesh();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_3)) {
            EditorLayout::instance().shadingMode = EditorShadingMode::Face;
            if (selPart) selPart->ensureEditableMesh();
        }

        // Alt + Z: Toggle Wireframe / X-Ray mode
        if (ImGui::IsKeyDown(ImGuiMod_Alt) && ImGui::IsKeyPressed(ImGuiKey_Z)) {
            s_wireframe = !s_wireframe;
        }

        // Alt + A or Ctrl + A: Select All (or Deselect All if already selected)
        if ((ImGui::IsKeyDown(ImGuiMod_Alt) || ImGui::IsKeyDown(ImGuiMod_Ctrl)) && ImGui::IsKeyPressed(ImGuiKey_A) && isHovered) {
            if (EditorLayout::instance().shadingMode != EditorShadingMode::Object && selPart) {
                bool hasSelection = !mCtx.selectedFaces.empty() || !mCtx.selectedEdges.empty() || !mCtx.selectedVertices.empty();
                if (hasSelection) {
                    mCtx.clearSelection();
                } else {
                    int mode = (EditorLayout::instance().shadingMode == EditorShadingMode::Face) ? 3 :
                               (EditorLayout::instance().shadingMode == EditorShadingMode::Edge) ? 2 : 1;
                    mCtx.selectAll(selPart, mode);
                }
            }
        }

        // Modeling Operator Shortcuts:
        if (EditorLayout::instance().shadingMode != EditorShadingMode::Object && selPart) {
            // Extrude: Ctrl + E or Alt + E (Does not conflict with camera E key)
            if ((ImGui::IsKeyDown(ImGuiMod_Ctrl) || ImGui::IsKeyDown(ImGuiMod_Alt)) && ImGui::IsKeyPressed(ImGuiKey_E) && mCtx.activeModal == Editor::Modeling::ModalTool::None) {
                mCtx.startExtrude(selPart);
            }
            // Inset: I or Ctrl + I
            if (ImGui::IsKeyPressed(ImGuiKey_I) && mCtx.activeModal == Editor::Modeling::ModalTool::None) {
                mCtx.startInset(selPart);
            }
            // Bevel: Ctrl + B
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_B) && mCtx.activeModal == Editor::Modeling::ModalTool::None) {
                mCtx.startBevel(selPart);
            }
            // Loop Cut: Ctrl + R
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_R) && mCtx.activeModal == Editor::Modeling::ModalTool::None) {
                mCtx.startLoopCut(selPart);
            }
            // Knife: K
            if (ImGui::IsKeyPressed(ImGuiKey_K) && mCtx.activeModal == Editor::Modeling::ModalTool::None) {
                mCtx.startKnife(selPart);
            }
            // Merge: M
            if (ImGui::IsKeyPressed(ImGuiKey_M)) {
                mCtx.executeMerge(selPart, Engine::Geometry::MergeMode::Center);
            }
            // Delete / Dissolve: X or Delete
            if (ImGui::IsKeyPressed(ImGuiKey_X) || ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                auto subType = (EditorLayout::instance().shadingMode == EditorShadingMode::Face) ? Engine::Geometry::SubElementType::Face :
                               (EditorLayout::instance().shadingMode == EditorShadingMode::Edge) ? Engine::Geometry::SubElementType::Edge :
                               Engine::Geometry::SubElementType::Vertex;
                mCtx.executeDelete(selPart, subType);
            }
            // Fill Face: F
            if (ImGui::IsKeyPressed(ImGuiKey_F)) {
                mCtx.executeFill(selPart);
            }
            // Poke Faces: Alt + P
            if (ImGui::IsKeyDown(ImGuiMod_Alt) && ImGui::IsKeyPressed(ImGuiKey_P)) {
                mCtx.executePoke(selPart, 0.0f);
            }
            // Triangulate Faces: Ctrl + T
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_T)) {
                mCtx.executeTriangulate(selPart);
            }
            // Tris to Quads: Alt + J
            if (ImGui::IsKeyDown(ImGuiMod_Alt) && ImGui::IsKeyPressed(ImGuiKey_J)) {
                mCtx.executeTrisToQuads(selPart);
            }
            // Flip Normals: Alt + N
            if (ImGui::IsKeyDown(ImGuiMod_Alt) && ImGui::IsKeyPressed(ImGuiKey_N)) {
                mCtx.executeFlipNormals(selPart);
            }
            // Select Linked: Ctrl + L
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_L)) {
                int mode = (EditorLayout::instance().shadingMode == EditorShadingMode::Face) ? 3 :
                           (EditorLayout::instance().shadingMode == EditorShadingMode::Edge) ? 2 : 1;
                mCtx.selectLinked(selPart, mode);
            }
            // Select More: Ctrl + KeypadAdd / Ctrl + Equal
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && (ImGui::IsKeyPressed(ImGuiKey_KeypadAdd) || ImGui::IsKeyPressed(ImGuiKey_Equal))) {
                int mode = (EditorLayout::instance().shadingMode == EditorShadingMode::Face) ? 3 :
                           (EditorLayout::instance().shadingMode == EditorShadingMode::Edge) ? 2 : 1;
                mCtx.selectMore(selPart, mode);
            }
            // Select Less: Ctrl + KeypadSubtract / Ctrl + Minus
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && (ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract) || ImGui::IsKeyPressed(ImGuiKey_Minus))) {
                int mode = (EditorLayout::instance().shadingMode == EditorShadingMode::Face) ? 3 :
                           (EditorLayout::instance().shadingMode == EditorShadingMode::Edge) ? 2 : 1;
                mCtx.selectLess(selPart, mode);
            }
            // Select Invert: Ctrl + I
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_I)) {
                int mode = (EditorLayout::instance().shadingMode == EditorShadingMode::Face) ? 3 :
                           (EditorLayout::instance().shadingMode == EditorShadingMode::Edge) ? 2 : 1;
                mCtx.selectInvert(selPart, mode);
            }
            // Separate: P
            if (ImGui::IsKeyPressed(ImGuiKey_P) && !ImGui::IsKeyDown(ImGuiMod_Alt)) {
                mCtx.executeSeparate(selPart);
            }
        }

        // Join Selected Parts: Ctrl + J
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_J) && !ImGui::IsKeyDown(ImGuiMod_Alt)) {
            mCtx.executeJoin(SelectionManager::instance().getSelectionList());
        }

        // Shift + S: Open Snap Pie Menu
        if (ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyPressed(ImGuiKey_S)) {
            showSnapPieMenu = true;
            showModesPieMenu = false;
            pieMenuCenter = mousePos;
        }

        // Ctrl + F: Open Faces / Modes Menu
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_F)) {
            showModesPieMenu = true;
            showSnapPieMenu = false;
            pieMenuCenter = mousePos;
        }

        // Shift + Right Click in Viewport: Place 3D Cursor on Ground Plane
        if (isHovered && ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            float ndcX = ((mousePos.x - screenPos.x) / avail.x) * 2.0f - 1.0f;
            float ndcY = -(((mousePos.y - screenPos.y) / avail.y) * 2.0f - 1.0f);

            float tanHalfFov = std::tan((camera.fovDegrees * 0.5f * 3.14159265f) / 180.0f);
            Engine::Math::Vector3 camRight = camera.forward.cross(camera.up).normalized();
            Engine::Math::Vector3 camUp = camRight.cross(camera.forward).normalized();
            Engine::Math::Vector3 rayDir = (camera.forward + camRight * (ndcX * tanHalfFov * ((float)currentWidth / (float)currentHeight)) + camUp * (ndcY * tanHalfFov)).normalized();
            Engine::Math::Vector3 rayOrigin = camera.position;

            if (std::abs(rayDir.y) > 0.0001f) {
                float t = -rayOrigin.y / rayDir.y;
                if (t > 0.0f) {
                    EditorLayout::instance().cursor3DPosition = rayOrigin + rayDir * t;
                }
            }
        }
    }

    static bool s_justConfirmedModal = false;

    // Modal Tool Interaction Handling
    if (mCtx.activeModal != Editor::Modeling::ModalTool::None) {
        bool shiftHeld = ImGui::GetIO().KeyShift;
        bool ctrlHeld  = ImGui::GetIO().KeyCtrl;
        mCtx.updateModal(mousePos, shiftHeld, ctrlHeld);

        if (mCtx.activeModal == Editor::Modeling::ModalTool::Knife) {
            // Knife Tool: 'C' toggles Cut-Through mode, Enter / Space confirms, Right Click / Esc cancels
            if (ImGui::IsKeyPressed(ImGuiKey_C) || ImGui::IsKeyPressed(ImGuiKey_Z)) {
                mCtx.opCutThrough = !mCtx.opCutThrough;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space)) {
                mCtx.confirmModal();
                s_justConfirmedModal = true;
            } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                mCtx.cancelModal();
            }
        } else {
            // Standard Modal: Left Click confirms, Right Click / Esc cancels
            if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                mCtx.confirmModal();
                s_justConfirmedModal = true;
            } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                mCtx.cancelModal();
            }
        }

        // Draw On-Screen Modal Tool HUD
        const char* modalName = "MODAL TOOL";
        const char* modalHelp = "[Left Click: Confirm | Right Click/ESC: Cancel]";
        if (mCtx.activeModal == Editor::Modeling::ModalTool::Extrude) modalName = "EXTRUDE (Ctrl+E)";
        else if (mCtx.activeModal == Editor::Modeling::ModalTool::Inset) modalName = "INSET FACES (I)";
        else if (mCtx.activeModal == Editor::Modeling::ModalTool::Bevel) modalName = "BEVEL (Ctrl+B)";
        else if (mCtx.activeModal == Editor::Modeling::ModalTool::LoopCut) modalName = "LOOP CUT (Ctrl+R)";
        else if (mCtx.activeModal == Editor::Modeling::ModalTool::Knife) {
            modalName = "KNIFE CUT (K)";
            modalHelp = mCtx.opCutThrough 
                ? "[Click: Add Point | C: Cut-Through [ON] | Enter/Space: Confirm | ESC: Cancel]"
                : "[Click: Add Point | C: Cut-Through [OFF] | Enter/Space: Confirm | ESC: Cancel]";
        }

        char hudText[256];
        std::snprintf(hudText, sizeof(hudText), "%s | %s", modalName, modalHelp);
        dl->AddRectFilled(ImVec2(screenPos.x + 20, screenPos.y + 40), ImVec2(screenPos.x + 600, screenPos.y + 70), COLA(0x0a1018, 0.92f), 6.0f);
        dl->AddRect(ImVec2(screenPos.x + 20, screenPos.y + 40), ImVec2(screenPos.x + 600, screenPos.y + 70), IM_COL32(0, 200, 255, 220), 6.0f);
        dl->AddText(ImVec2(screenPos.x + 32, screenPos.y + 46), IM_COL32(255, 255, 255, 255), hudText);
    }

    // ── Sub-Element Edit Modes (Face, Edge, Vertex) Overlays & Picking ───────
    bool isObjectMode = (EditorLayout::instance().shadingMode == EditorShadingMode::Object);
    bool isFaceMode   = (EditorLayout::instance().shadingMode == EditorShadingMode::Face);
    bool isEdgeMode   = (EditorLayout::instance().shadingMode == EditorShadingMode::Edge);
    bool isVertexMode = (EditorLayout::instance().shadingMode == EditorShadingMode::Vertex);
    bool isModalActive = (mCtx.activeModal != Editor::Modeling::ModalTool::None);

    if (!isObjectMode) {
        auto pointInTriangle = [](ImVec2 pt, ImVec2 v1, ImVec2 v2, ImVec2 v3) -> bool {
            float d1 = (pt.x - v2.x) * (v1.y - v2.y) - (v1.x - v2.x) * (pt.y - v2.y);
            float d2 = (pt.x - v3.x) * (v2.y - v3.y) - (v2.x - v3.x) * (pt.y - v3.y);
            float d3 = (pt.x - v1.x) * (v3.y - v1.y) - (v3.x - v1.x) * (pt.y - v1.y);
            bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
            bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
            return !(has_neg && has_pos);
        };

        auto pointInQuad = [&](ImVec2 pt, ImVec2 p0, ImVec2 p1, ImVec2 p2, ImVec2 p3) -> bool {
            return pointInTriangle(pt, p0, p1, p2) || pointInTriangle(pt, p0, p2, p3);
        };

        auto isFrontFacing = [](ImVec2 p0, ImVec2 p1, ImVec2 p2) -> bool {
            return ((p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x)) < 0.0f;
        };

        auto distToSegment = [](ImVec2 p, ImVec2 a, ImVec2 b) -> float {
            float l2 = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
            if (l2 == 0.0f) {
                float dx = p.x - a.x, dy = p.y - a.y;
                return std::sqrt(dx * dx + dy * dy);
            }
            float t = ((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / l2;
            t = std::max(0.0f, std::min(1.0f, t));
            float projX = a.x + t * (b.x - a.x);
            float projY = a.y + t * (b.y - a.y);
            float dx = p.x - projX, dy = p.y - projY;
            return std::sqrt(dx * dx + dy * dy);
        };

        auto sel = SelectionManager::instance().getSelected();
        auto selList = SelectionManager::instance().getSelectionList();

        bool shiftHeld = ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl;
        bool canPick = isHovered && !showSnapPieMenu && !showModesPieMenu && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver() && !ImGui::IsMouseDown(ImGuiMouseButton_Right);

        int hoveredVertex = -1;
        float bestVertexDist = 14.0f;

        int hoveredEdge = -1;
        float bestEdgeDist = 12.0f;

        int hoveredFace = -1;

        auto contains = [](const std::vector<uint32_t>& vec, uint32_t val) {
            return std::find(vec.begin(), vec.end(), val) != vec.end();
        };

        std::function<void(const std::shared_ptr<Instance>&)> drawDeformOverlay = [&](const std::shared_ptr<Instance>& inst) {
            if (auto part = std::dynamic_pointer_cast<Part>(inst)) {
                part->ensureEditableMesh();
                auto mesh = part->getEditableMesh();
                if (!mesh) return;

                Engine::Math::Vector3 pos = part->getPosition();
                bool isSelected = (part == sel);
                if (!isSelected) {
                    for (auto& s : selList) { if (s == part) { isSelected = true; break; } }
                }

                const auto& vertices = mesh->getVertices();
                const auto& edges = mesh->getEdges();
                const auto& faces = mesh->getFaces();

                size_t numVerts = vertices.size();
                std::vector<ImVec2> sPts(numVerts);
                std::vector<bool> visible(numVerts);

                for (size_t i = 0; i < numVerts; ++i) {
                    if (vertices[i].deleted) continue;
                    visible[i] = project(pos + vertices[i].position, sPts[i]);
                }

                // Check Picking & Snapping for Selected Part
                if (isSelected && canPick && !isBoxSelecting) {
                    if (isVertexMode || mCtx.activeModal == Editor::Modeling::ModalTool::Knife) {
                        for (size_t i = 0; i < numVerts; ++i) {
                            if (visible[i] && !vertices[i].deleted) {
                                float dx = mousePos.x - sPts[i].x;
                                float dy = mousePos.y - sPts[i].y;
                                float dist = std::sqrt(dx * dx + dy * dy);
                                if (dist < bestVertexDist) {
                                    bestVertexDist = dist;
                                    hoveredVertex = static_cast<int>(i);
                                }
                            }
                        }
                    }
                    if (isEdgeMode || mCtx.activeModal == Editor::Modeling::ModalTool::Knife) {
                        for (size_t e = 0; e < edges.size(); ++e) {
                            if (edges[e].deleted) continue;
                            uint32_t i0 = edges[e].v0, i1 = edges[e].v1;
                            if (i0 < numVerts && i1 < numVerts && visible[i0] && visible[i1]) {
                                float dist = distToSegment(mousePos, sPts[i0], sPts[i1]);
                                if (dist < bestEdgeDist) {
                                    bestEdgeDist = dist;
                                    hoveredEdge = static_cast<int>(e);
                                }
                            }
                        }
                    }
                    // Face Picking and Face-specific LoopCut detection
                    for (size_t f = 0; f < faces.size(); ++f) {
                        if (faces[f].deleted || faces[f].vertices.size() < 3) continue;
                        const auto& fVerts = faces[f].vertices;
                        bool allVis = true;
                        for (uint32_t v : fVerts) {
                            if (v >= numVerts || !visible[v]) { allVis = false; break; }
                        }
                        if (allVis && fVerts.size() == 4) {
                            uint32_t i0 = fVerts[0], i1 = fVerts[1], i2 = fVerts[2], i3 = fVerts[3];
                            if (isFrontFacing(sPts[i0], sPts[i1], sPts[i2])) {
                                if (pointInQuad(mousePos, sPts[i0], sPts[i1], sPts[i2], sPts[i3])) {
                                    hoveredFace = static_cast<int>(f);
                                    if (mCtx.activeModal == Editor::Modeling::ModalTool::LoopCut) {
                                        float bestDistOnFace = 10000.0f;
                                        int closestQuadEdge = -1;
                                        for (int ei = 0; ei < 4; ++ei) {
                                            uint32_t vA = fVerts[ei], vB = fVerts[(ei + 1) % 4];
                                            int e = mesh->findEdge(vA, vB);
                                            if (e >= 0) {
                                                float dist = distToSegment(mousePos, sPts[vA], sPts[vB]);
                                                if (dist < bestDistOnFace) {
                                                    bestDistOnFace = dist;
                                                    closestQuadEdge = e;
                                                }
                                            }
                                        }
                                        if (closestQuadEdge != -1) {
                                            mCtx.previewLoopEdges = Engine::Geometry::MeshCutOperators::findEdgeLoop(*mesh, (uint32_t)closestQuadEdge);
                                        }
                                    }
                                }
                            }
                        } else if (allVis && fVerts.size() == 3) {
                            uint32_t i0 = fVerts[0], i1 = fVerts[1], i2 = fVerts[2];
                            if (isFrontFacing(sPts[i0], sPts[i1], sPts[i2])) {
                                if (pointInTriangle(mousePos, sPts[i0], sPts[i1], sPts[i2])) {
                                    hoveredFace = static_cast<int>(f);
                                }
                            }
                        }
                    }
                }

                // Handle Knife Snapped Click Point Insertion with Exact 3D Ray-Plane Intersection
                if (mCtx.activeModal == Editor::Modeling::ModalTool::Knife && isSelected && isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if (hoveredVertex != -1) {
                        mCtx.knifePoints.push_back(vertices[hoveredVertex].position);
                        if (hoveredFace != -1) mCtx.knifeTargetFaces.push_back((uint32_t)hoveredFace);
                    } else if (hoveredEdge != -1) {
                        uint32_t ev0 = edges[hoveredEdge].v0;
                        uint32_t ev1 = edges[hoveredEdge].v1;
                        ImVec2 p0 = sPts[ev0], p1 = sPts[ev1];
                        float l2 = (p1.x - p0.x) * (p1.x - p0.x) + (p1.y - p0.y) * (p1.y - p0.y);
                        float t = (l2 > 0.0f) ? std::clamp(((mousePos.x - p0.x) * (p1.x - p0.x) + (mousePos.y - p0.y) * (p1.y - p0.y)) / l2, 0.0f, 1.0f) : 0.5f;
                        Engine::Math::Vector3 edgePt = vertices[ev0].position + (vertices[ev1].position - vertices[ev0].position) * t;
                        mCtx.knifePoints.push_back(edgePt);
                        if (hoveredFace != -1) mCtx.knifeTargetFaces.push_back((uint32_t)hoveredFace);
                    } else if (hoveredFace != -1) {
                        float ndcX = ((mousePos.x - screenPos.x) / avail.x) * 2.0f - 1.0f;
                        float ndcY = -(((mousePos.y - screenPos.y) / avail.y) * 2.0f - 1.0f);
                        float tanHalfFov = std::tan((camera.fovDegrees * 0.5f * 3.14159265f) / 180.0f);
                        Engine::Math::Vector3 camRight = camera.forward.cross(camera.up).normalized();
                        Engine::Math::Vector3 camUp = camRight.cross(camera.forward).normalized();
                        Engine::Math::Vector3 rayDir = (camera.forward + camRight * (ndcX * tanHalfFov * ((float)currentWidth / (float)currentHeight)) + camUp * (ndcY * tanHalfFov)).normalized();
                        Engine::Math::Vector3 rayOrig = camera.position;

                        mesh->calculateFaceNormal((uint32_t)hoveredFace);
                        Engine::Math::Vector3 fn = faces[hoveredFace].normal;
                        Engine::Math::Vector3 fp = pos + vertices[faces[hoveredFace].vertices[0]].position;
                        float denom = fn.dot(rayDir);
                        if (std::abs(denom) > 1e-5f) {
                            float t = (fp - rayOrig).dot(fn) / denom;
                            if (t > 0.0f) {
                                Engine::Math::Vector3 worldHit = rayOrig + rayDir * t;
                                Engine::Math::Vector3 localHit = worldHit - pos;
                                mCtx.knifePoints.push_back(localHit);
                                mCtx.knifeTargetFaces.push_back((uint32_t)hoveredFace);
                            }
                        }
                    }
                }

                // Render Loop Cut Preview Quad Edge Connecting Lines and Dots (multi-cut support)
                if (mCtx.activeModal == Editor::Modeling::ModalTool::LoopCut && isSelected && !mCtx.previewLoopEdges.empty()) {
                    int cuts = std::max(1, std::min(6, mCtx.opCuts));
                    std::set<uint32_t> loopEdgeSet(mCtx.previewLoopEdges.begin(), mCtx.previewLoopEdges.end());

                    for (size_t f = 0; f < faces.size(); ++f) {
                        if (faces[f].deleted || faces[f].vertices.size() != 4) continue;
                        uint32_t v0 = faces[f].vertices[0], v1 = faces[f].vertices[1];
                        uint32_t v2 = faces[f].vertices[2], v3 = faces[f].vertices[3];

                        int e0 = mesh->findEdge(v0, v1);
                        int e1 = mesh->findEdge(v1, v2);
                        int e2 = mesh->findEdge(v2, v3);
                        int e3 = mesh->findEdge(v3, v0);

                        bool has0 = (e0 >= 0 && loopEdgeSet.count((uint32_t)e0));
                        bool has2 = (e2 >= 0 && loopEdgeSet.count((uint32_t)e2));
                        bool has1 = (e1 >= 0 && loopEdgeSet.count((uint32_t)e1));
                        bool has3 = (e3 >= 0 && loopEdgeSet.count((uint32_t)e3));

                        if (has0 && has2) {
                            for (int k = 0; k < cuts; ++k) {
                                float baseT = (float)(k + 1) / (float)(cuts + 1);
                                float t = std::clamp(baseT + mCtx.opSlide * (0.40f / (float)(cuts + 1)), 0.02f, 0.98f);

                                Engine::Math::Vector3 pA = vertices[v0].position + (vertices[v1].position - vertices[v0].position) * t;
                                Engine::Math::Vector3 pB = vertices[v3].position + (vertices[v2].position - vertices[v3].position) * t;

                                ImVec2 sA, sB;
                                if (project(pos + pA, sA) && project(pos + pB, sB)) {
                                    dl->AddLine(sA, sB, IM_COL32(255, 210, 0, 255), 3.5f);
                                    dl->AddLine(sA, sB, IM_COL32(255, 255, 255, 220), 1.5f);
                                    dl->AddCircleFilled(sA, 5.0f, IM_COL32(255, 200, 0, 255));
                                    dl->AddCircleFilled(sB, 5.0f, IM_COL32(255, 200, 0, 255));
                                }
                            }
                        } else if (has1 && has3) {
                            for (int k = 0; k < cuts; ++k) {
                                float baseT = (float)(k + 1) / (float)(cuts + 1);
                                float t = std::clamp(baseT + mCtx.opSlide * (0.40f / (float)(cuts + 1)), 0.02f, 0.98f);

                                Engine::Math::Vector3 pA = vertices[v1].position + (vertices[v2].position - vertices[v1].position) * t;
                                Engine::Math::Vector3 pB = vertices[v0].position + (vertices[v3].position - vertices[v0].position) * t;

                                ImVec2 sA, sB;
                                if (project(pos + pA, sA) && project(pos + pB, sB)) {
                                    dl->AddLine(sA, sB, IM_COL32(255, 210, 0, 255), 3.5f);
                                    dl->AddLine(sA, sB, IM_COL32(255, 255, 255, 220), 1.5f);
                                    dl->AddCircleFilled(sA, 5.0f, IM_COL32(255, 200, 0, 255));
                                    dl->AddCircleFilled(sB, 5.0f, IM_COL32(255, 200, 0, 255));
                                }
                            }
                        }
                    }
                }

                // Render Knife Tool Preview Path & Snap Indicator
                if (mCtx.activeModal == Editor::Modeling::ModalTool::Knife && isSelected) {
                    if (hoveredVertex != -1 && visible[hoveredVertex]) {
                        dl->AddCircle(sPts[hoveredVertex], 10.0f, IM_COL32(0, 255, 120, 255), 0, 2.5f);
                    } else if (hoveredEdge != -1) {
                        dl->AddCircle(mousePos, 7.0f, IM_COL32(0, 220, 255, 255), 0, 2.0f);
                    }

                    std::vector<ImVec2> projectedKnifePts;
                    for (const auto& kpt : mCtx.knifePoints) {
                        ImVec2 spt;
                        if (project(pos + kpt, spt)) {
                            projectedKnifePts.push_back(spt);
                        }
                    }

                    for (size_t i = 0; i < projectedKnifePts.size(); ++i) {
                        dl->AddCircleFilled(projectedKnifePts[i], 6.0f, IM_COL32(255, 60, 60, 255));
                        dl->AddCircle(projectedKnifePts[i], 8.0f, IM_COL32(255, 255, 255, 255), 0, 1.5f);
                        if (i > 0) {
                            dl->AddLine(projectedKnifePts[i-1], projectedKnifePts[i], IM_COL32(255, 60, 60, 255), 3.0f);
                        }
                    }
                    if (!projectedKnifePts.empty() && (hoveredFace != -1 || hoveredEdge != -1 || hoveredVertex != -1)) {
                        dl->AddLine(projectedKnifePts.back(), mousePos, IM_COL32(0, 255, 120, 255), 2.0f);
                    }
                }

                // In Face Mode: Render Face Highlights (Only when no modal tool is active to avoid visual clutter)
                if (isFaceMode && isSelected) {
                    for (size_t f = 0; f < faces.size(); ++f) {
                        if (faces[f].deleted || faces[f].vertices.size() < 3) continue;
                        const auto& fVerts = faces[f].vertices;
                        bool allVis = true;
                        for (uint32_t v : fVerts) {
                            if (v >= numVerts || !visible[v]) { allVis = false; break; }
                        }
                        if (!allVis) continue;

                        bool isHoveredF = !isModalActive && (hoveredFace == (int)f);
                        bool isCurrentF = contains(mCtx.selectedFaces, static_cast<uint32_t>(f));

                        ImVec2 fCenter(0, 0);
                        for (uint32_t v : fVerts) fCenter = ImVec2(fCenter.x + sPts[v].x, fCenter.y + sPts[v].y);
                        fCenter = ImVec2(fCenter.x / fVerts.size(), fCenter.y / fVerts.size());

                        if (fVerts.size() == 4) {
                            uint32_t i0 = fVerts[0], i1 = fVerts[1], i2 = fVerts[2], i3 = fVerts[3];
                            if (isCurrentF) {
                                dl->AddQuadFilled(sPts[i0], sPts[i1], sPts[i2], sPts[i3], IM_COL32(0, 200, 255, 100));
                                dl->AddQuad(sPts[i0], sPts[i1], sPts[i2], sPts[i3], IM_COL32(0, 230, 255, 255), 3.0f);
                            } else if (isHoveredF) {
                                dl->AddQuadFilled(sPts[i0], sPts[i1], sPts[i2], sPts[i3], IM_COL32(255, 230, 70, 80));
                                dl->AddQuad(sPts[i0], sPts[i1], sPts[i2], sPts[i3], IM_COL32(255, 230, 70, 255), 2.5f);
                            } else if (!isModalActive) {
                                dl->AddQuad(sPts[i0], sPts[i1], sPts[i2], sPts[i3], IM_COL32(130, 217, 255, 140), 1.5f);
                            }
                        } else if (fVerts.size() == 3) {
                            uint32_t i0 = fVerts[0], i1 = fVerts[1], i2 = fVerts[2];
                            if (isCurrentF) {
                                dl->AddTriangleFilled(sPts[i0], sPts[i1], sPts[i2], IM_COL32(0, 200, 255, 100));
                                dl->AddTriangle(sPts[i0], sPts[i1], sPts[i2], IM_COL32(0, 230, 255, 255), 3.0f);
                            } else if (isHoveredF) {
                                dl->AddTriangleFilled(sPts[i0], sPts[i1], sPts[i2], IM_COL32(255, 230, 70, 80));
                                dl->AddTriangle(sPts[i0], sPts[i1], sPts[i2], IM_COL32(255, 230, 70, 255), 2.5f);
                            } else if (!isModalActive) {
                                dl->AddTriangle(sPts[i0], sPts[i1], sPts[i2], IM_COL32(130, 217, 255, 140), 1.5f);
                            }
                        }
                    }
                }

                // In Edge & Vertex Modes: Render Edges
                if (isEdgeMode || isVertexMode) {
                    for (size_t e = 0; e < edges.size(); ++e) {
                        if (edges[e].deleted) continue;
                        uint32_t i0 = edges[e].v0, i1 = edges[e].v1;
                        if (i0 < numVerts && i1 < numVerts && visible[i0] && visible[i1]) {
                            bool isHoveredE = !isModalActive && (isSelected && isEdgeMode && hoveredEdge == (int)e);
                            bool isCurrentE = (isSelected && isEdgeMode && contains(mCtx.selectedEdges, static_cast<uint32_t>(e)));

                            if (isCurrentE) {
                                dl->AddLine(sPts[i0], sPts[i1], IM_COL32(0, 220, 255, 255), 4.5f);
                                dl->AddLine(sPts[i0], sPts[i1], IM_COL32(255, 255, 255, 255), 2.0f);
                            } else if (isHoveredE) {
                                dl->AddLine(sPts[i0], sPts[i1], IM_COL32(255, 230, 70, 255), 3.5f);
                            } else if (!isModalActive) {
                                ImU32 lineCol = isSelected ? IM_COL32(130, 217, 255, 220) : IM_COL32(180, 180, 180, 160);
                                dl->AddLine(sPts[i0], sPts[i1], lineCol, isSelected ? 2.5f : 1.8f);
                            }
                        }
                    }
                }

                // Vertex Corner dots
                if (isVertexMode || isEdgeMode) {
                    for (size_t i = 0; i < numVerts; ++i) {
                        if (visible[i] && !vertices[i].deleted) {
                            if (isVertexMode) {
                                bool isHoveredVert = !isModalActive && (isSelected && hoveredVertex == (int)i);
                                bool isCurrentVert = (isSelected && contains(mCtx.selectedVertices, static_cast<uint32_t>(i)));

                                if (isCurrentVert) {
                                    dl->AddCircleFilled(sPts[i], 6.5f, IM_COL32(0, 210, 255, 255));
                                    dl->AddCircleFilled(sPts[i], 4.0f, IM_COL32(255, 255, 255, 255));
                                    dl->AddCircle(sPts[i], 8.5f, IM_COL32(0, 140, 255, 200), 0, 1.8f);
                                } else if (isHoveredVert) {
                                    dl->AddCircleFilled(sPts[i], 5.5f, IM_COL32(255, 240, 80, 255));
                                    dl->AddCircle(sPts[i], 7.5f, IM_COL32(255, 180, 0, 220), 0, 1.5f);
                                } else if (!isModalActive) {
                                    ImU32 vCol = isSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 210, 40, 255);
                                    ImU32 borderCol = isSelected ? IM_COL32(0, 120, 215, 255) : IM_COL32(20, 20, 20, 240);
                                    dl->AddCircleFilled(sPts[i], 3.5f, vCol);
                                    dl->AddCircle(sPts[i], 4.0f, borderCol, 0, 1.2f);
                                }
                            }
                        }
                    }
                }
            }
            for (auto& child : inst->getChildren()) drawDeformOverlay(child);
        };
        drawDeformOverlay(DataModel::instance());

        // Handle Click Selection & Loop Selection (Alt + Left Click)
        if (s_justConfirmedModal) {
            s_justConfirmedModal = false;
        } else if (canPick && mCtx.activeModal == Editor::Modeling::ModalTool::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            bool altHeld = ImGui::GetIO().KeyAlt;
            mCtx.lastOp = Editor::Modeling::LastOpType::None;

            if (isVertexMode) {
                if (hoveredVertex != -1) {
                    if (shiftHeld) {
                        auto it = std::find(mCtx.selectedVertices.begin(), mCtx.selectedVertices.end(), (uint32_t)hoveredVertex);
                        if (it != mCtx.selectedVertices.end()) mCtx.selectedVertices.erase(it);
                        else mCtx.selectedVertices.push_back((uint32_t)hoveredVertex);
                    } else {
                        mCtx.selectedVertices = { (uint32_t)hoveredVertex };
                    }
                } else if (!shiftHeld && !ImGuizmo::IsOver()) {
                    isBoxSelecting = true;
                    boxSelectStart = mousePos;
                }
            } else if (isEdgeMode) {
                if (hoveredEdge != -1) {
                    if (altHeld && sel) {
                        if (auto p = std::dynamic_pointer_cast<Part>(sel)) {
                            p->ensureEditableMesh();
                            auto loop = Engine::Geometry::MeshCutOperators::findEdgeLoop(*p->getEditableMesh(), (uint32_t)hoveredEdge);
                            if (!shiftHeld) mCtx.selectedEdges = loop;
                            else mCtx.selectedEdges.insert(mCtx.selectedEdges.end(), loop.begin(), loop.end());
                        }
                    } else if (shiftHeld) {
                        auto it = std::find(mCtx.selectedEdges.begin(), mCtx.selectedEdges.end(), (uint32_t)hoveredEdge);
                        if (it != mCtx.selectedEdges.end()) mCtx.selectedEdges.erase(it);
                        else mCtx.selectedEdges.push_back((uint32_t)hoveredEdge);
                    } else {
                        mCtx.selectedEdges = { (uint32_t)hoveredEdge };
                    }
                } else if (!shiftHeld && !ImGuizmo::IsOver()) {
                    isBoxSelecting = true;
                    boxSelectStart = mousePos;
                }
            } else if (isFaceMode) {
                if (hoveredFace != -1) {
                    if (shiftHeld) {
                        auto it = std::find(mCtx.selectedFaces.begin(), mCtx.selectedFaces.end(), (uint32_t)hoveredFace);
                        if (it != mCtx.selectedFaces.end()) mCtx.selectedFaces.erase(it);
                        else mCtx.selectedFaces.push_back((uint32_t)hoveredFace);
                    } else {
                        mCtx.selectedFaces = { (uint32_t)hoveredFace };
                    }
                } else if (!shiftHeld && !ImGuizmo::IsOver()) {
                    mCtx.selectedFaces.clear();
                }
            }
        }

    // Handle Box Selection Dragging
    if (isBoxSelecting) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            ImVec2 bMin = ImVec2(std::min(boxSelectStart.x, mousePos.x), std::min(boxSelectStart.y, mousePos.y));
            ImVec2 bMax = ImVec2(std::max(boxSelectStart.x, mousePos.x), std::max(boxSelectStart.y, mousePos.y));

            // Draw Marquee Box
            dl->AddRectFilled(bMin, bMax, IM_COL32(0, 200, 255, 35));
            dl->AddRect(bMin, bMax, IM_COL32(0, 220, 255, 220), 0, 0, 1.5f);
        } else {
            // Mouse released: finalize box selection
            ImVec2 bMin = ImVec2(std::min(boxSelectStart.x, mousePos.x), std::min(boxSelectStart.y, mousePos.y));
            ImVec2 bMax = ImVec2(std::max(boxSelectStart.x, mousePos.x), std::max(boxSelectStart.y, mousePos.y));
            float boxArea = (bMax.x - bMin.x) * (bMax.y - bMin.y);

            if (boxArea > 25.0f) { // Dragged more than 5x5 px
                if (!shiftHeld) {
                    mCtx.clearSelection();
                }
                if (auto part = std::dynamic_pointer_cast<Part>(sel)) {
                    part->ensureEditableMesh();
                    auto mesh = part->getEditableMesh();
                    if (mesh) {
                        Engine::Math::Vector3 pos = part->getPosition();
                        const auto& vertices = mesh->getVertices();
                        const auto& edges = mesh->getEdges();
                        const auto& faces = mesh->getFaces();

                        size_t numVerts = vertices.size();
                        std::vector<ImVec2> sPts(numVerts);
                        std::vector<bool> visible(numVerts);

                        for (size_t i = 0; i < numVerts; ++i) {
                            if (vertices[i].deleted) continue;
                            visible[i] = project(pos + vertices[i].position, sPts[i]);
                        }

                        if (isVertexMode) {
                            for (size_t i = 0; i < numVerts; ++i) {
                                if (visible[i] && !vertices[i].deleted &&
                                    sPts[i].x >= bMin.x && sPts[i].x <= bMax.x &&
                                    sPts[i].y >= bMin.y && sPts[i].y <= bMax.y) {
                                    if (!contains(mCtx.selectedVertices, static_cast<uint32_t>(i))) {
                                        mCtx.selectedVertices.push_back(static_cast<uint32_t>(i));
                                    }
                                }
                            }
                        } else if (isEdgeMode) {
                            for (size_t e = 0; e < edges.size(); ++e) {
                                if (edges[e].deleted) continue;
                                uint32_t i0 = edges[e].v0, i1 = edges[e].v1;
                                if (i0 < numVerts && i1 < numVerts && visible[i0] && visible[i1]) {
                                    ImVec2 mid((sPts[i0].x + sPts[i1].x) * 0.5f, (sPts[i0].y + sPts[i1].y) * 0.5f);
                                    if ((sPts[i0].x >= bMin.x && sPts[i0].x <= bMax.x && sPts[i0].y >= bMin.y && sPts[i0].y <= bMax.y) ||
                                        (sPts[i1].x >= bMin.x && sPts[i1].x <= bMax.x && sPts[i1].y >= bMin.y && sPts[i1].y <= bMax.y) ||
                                        (mid.x >= bMin.x && mid.x <= bMax.x && mid.y >= bMin.y && mid.y <= bMax.y)) {
                                        if (!contains(mCtx.selectedEdges, static_cast<uint32_t>(e))) {
                                            mCtx.selectedEdges.push_back(static_cast<uint32_t>(e));
                                        }
                                    }
                                }
                            }
                        } else if (isFaceMode) {
                            for (size_t f = 0; f < faces.size(); ++f) {
                                if (faces[f].deleted || faces[f].vertices.empty()) continue;
                                ImVec2 center(0, 0);
                                bool allVis = true;
                                for (uint32_t v : faces[f].vertices) {
                                    if (v >= numVerts || !visible[v]) { allVis = false; break; }
                                    center.x += sPts[v].x;
                                    center.y += sPts[v].y;
                                }
                                if (allVis) {
                                    center.x /= faces[f].vertices.size();
                                    center.y /= faces[f].vertices.size();
                                    if (center.x >= bMin.x && center.x <= bMax.x && center.y >= bMin.y && center.y <= bMax.y) {
                                        if (!contains(mCtx.selectedFaces, static_cast<uint32_t>(f))) {
                                            mCtx.selectedFaces.push_back(static_cast<uint32_t>(f));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            } else if (!shiftHeld) {
                mCtx.clearSelection();
            }
            isBoxSelecting = false;
        }
    }
    } // Closes if (!isObjectMode)

    handleCameraControls(camera, isHovered);

    // ── Gizmo ─────────────────────────────────────────────────────────────────
    handleGizmoInput(camera);

    // ═════════════════════════════════════════════════════════════════════════
    // ORIENTATION CUBE (sağ üst köşe)
    // ═════════════════════════════════════════════════════════════════════════
    {
        float cs = 52.0f;
        float tbH = 28.0f;
        ImVec2 cMin = {panelMax.x - cs - 12, panelMin.y + tbH + 10};
        ImVec2 cMax = {cMin.x + cs, cMin.y + cs};

        dl->AddRectFilled(cMin, cMax, COLA(0x0e0e0e, 0.92f), 8.0f);
        dl->AddRect(cMin, cMax, COL(T.border), 8.0f);

        // Clickable regions on Orientation Cube
        ImVec2 topMin = ImVec2(cMin.x, cMin.y);
        ImVec2 topMax = ImVec2(cMax.x, cMin.y + 17.0f);
        
        ImVec2 frontMin = ImVec2(cMin.x, cMin.y + 17.0f);
        ImVec2 frontMax = ImVec2(cMax.x, cMin.y + 34.0f);
        
        ImVec2 rightMin = ImVec2(cMin.x, cMin.y + 34.0f);
        ImVec2 rightMax = ImVec2(cMax.x, cMax.y);

        ImGui::SetCursorScreenPos(topMin);
        ImGui::InvisibleButton("##oriTop", ImVec2(cs, 17.0f));
        if (ImGui::IsItemClicked()) {
            EditorLayout::instance().cameraMode = CameraViewMode::Degree90;
            EditorLayout::instance().degree90Index = 4; // Top
        }
        bool hovTop = ImGui::IsItemHovered();

        ImGui::SetCursorScreenPos(frontMin);
        ImGui::InvisibleButton("##oriFront", ImVec2(cs, 17.0f));
        if (ImGui::IsItemClicked()) {
            EditorLayout::instance().cameraMode = CameraViewMode::Degree90;
            EditorLayout::instance().degree90Index = 0; // Front
        }
        bool hovFront = ImGui::IsItemHovered();

        ImGui::SetCursorScreenPos(rightMin);
        ImGui::InvisibleButton("##oriRight", ImVec2(cs, 17.0f));
        if (ImGui::IsItemClicked()) {
            EditorLayout::instance().cameraMode = CameraViewMode::Degree90;
            EditorLayout::instance().degree90Index = 1; // Right
        }
        bool hovRight = ImGui::IsItemHovered();

        if (hovTop) dl->AddRectFilled(topMin, topMax, COLA(0x82D9FF, 0.20f), 4.0f);
        if (hovFront) dl->AddRectFilled(frontMin, frontMax, COLA(0x82D9FF, 0.20f), 4.0f);
        if (hovRight) dl->AddRectFilled(rightMin, rightMax, COLA(0x82D9FF, 0.20f), 4.0f);

        // Active indicator / labels
        bool isDegree90 = (EditorLayout::instance().cameraMode == CameraViewMode::Degree90);
        int degIdx = EditorLayout::instance().degree90Index;
        ImU32 topCol = (isDegree90 && degIdx == 4) ? COL(T.accent) : (hovTop ? COL(T.textPrimary) : COL(T.textMuted));
        ImU32 frontCol = (isDegree90 && degIdx == 0) ? COL(T.accent) : (hovFront ? COL(T.textPrimary) : COL(T.textSecondary));
        ImU32 rightCol = (isDegree90 && degIdx == 1) ? COL(T.accent) : (hovRight ? COL(T.textPrimary) : COL(T.textSecondary));

        dl->AddText({cMin.x + 14.0f, cMin.y + 3.0f}, topCol, "TOP");
        dl->AddText({cMin.x + 8.0f, cMin.y + 19.0f}, frontCol, "FRONT");
        dl->AddText({cMin.x + 8.0f, cMin.y + 35.0f}, rightCol, "RIGHT");
    }

    // ═════════════════════════════════════════════════════════════════════════
    // PIE MENUS (Snap Shift+S & Modes Ctrl+F)
    // ═════════════════════════════════════════════════════════════════════════
    if (showSnapPieMenu || showModesPieMenu) {
        ImVec2 mPos = ImGui::GetIO().MousePos;
        ImDrawList* fgDl = ImGui::GetForegroundDrawList();

        // Dark translucent fullscreen overlay backdrop
        fgDl->AddRectFilled(panelMin, panelMax, IM_COL32(0, 0, 0, 120));

        // Center hub
        fgDl->AddCircleFilled(pieMenuCenter, 14.0f, IM_COL32(24, 24, 28, 245));
        fgDl->AddCircle(pieMenuCenter, 14.0f, COL(T.accent), 0, 1.8f);
        fgDl->AddCircleFilled(pieMenuCenter, 3.5f, COL(T.accent));

        // Close on Escape or right click
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            showSnapPieMenu = false;
            showModesPieMenu = false;
        }

        struct PieItem {
            std::string label;
            std::string key;
            float angleDeg; // -90 = Top, 0 = Right, 90 = Bottom, 180 = Left
            std::function<void()> action;
        };
        std::vector<PieItem> items;

        if (showSnapPieMenu) {
            auto snapSelectionToGrid = [&]() {
                auto selList = SelectionManager::instance().getSelectionList();
                if (selList.empty()) {
                    if (auto single = SelectionManager::instance().getSelected()) selList.push_back(single);
                }
                if (EditorLayout::instance().shadingMode == EditorShadingMode::Object) {
                    for (auto& inst : selList) {
                        if (auto p = std::dynamic_pointer_cast<Part>(inst)) {
                            Engine::Math::Vector3 pos = p->getPosition();
                            pos.x = std::round(pos.x);
                            pos.y = std::round(pos.y);
                            pos.z = std::round(pos.z);
                            p->setPosition(pos);
                        }
                    }
                } else {
                    static const int edges[12][2] = {
                        {0, 1}, {1, 2}, {2, 3}, {3, 0},
                        {4, 5}, {5, 6}, {6, 7}, {7, 4},
                        {0, 4}, {1, 5}, {2, 6}, {3, 7}
                    };
                    struct FaceDef { int idx[4]; };
                    static const FaceDef s_boxFaces[6] = {
                        { { 4, 5, 6, 7 } }, { { 1, 0, 3, 2 } }, { { 7, 6, 2, 3 } },
                        { { 0, 1, 5, 4 } }, { { 5, 1, 2, 6 } }, { { 0, 4, 7, 3 } }
                    };
                    for (auto& inst : selList) {
                        if (auto p = std::dynamic_pointer_cast<Part>(inst)) {
                            std::set<int> verts;
                            if (EditorLayout::instance().shadingMode == EditorShadingMode::Vertex) {
                                for (int v : selectedVertices) verts.insert(v);
                            } else if (EditorLayout::instance().shadingMode == EditorShadingMode::Edge) {
                                for (int e : selectedEdges) {
                                    if (e >= 0 && e < 12) { verts.insert(edges[e][0]); verts.insert(edges[e][1]); }
                                }
                            } else if (EditorLayout::instance().shadingMode == EditorShadingMode::Face) {
                                for (int f : selectedFaces) {
                                    if (f >= 0 && f < 6) { for (int v = 0; v < 4; ++v) verts.insert(s_boxFaces[f].idx[v]); }
                                }
                            }
                            for (int v : verts) {
                                Engine::Math::Vector3 worldV = p->getPosition() + p->getVertex(v);
                                worldV.x = std::round(worldV.x);
                                worldV.y = std::round(worldV.y);
                                worldV.z = std::round(worldV.z);
                                p->setVertex(v, worldV - p->getPosition());
                            }
                        }
                    }
                }
            };

            auto snapSelectionToCursor = [&]() {
                auto selList = SelectionManager::instance().getSelectionList();
                if (selList.empty()) {
                    if (auto single = SelectionManager::instance().getSelected()) selList.push_back(single);
                }
                if (EditorLayout::instance().shadingMode == EditorShadingMode::Object) {
                    for (auto& inst : selList) {
                        if (auto p = std::dynamic_pointer_cast<Part>(inst)) {
                            p->setPosition(EditorLayout::instance().cursor3DPosition);
                        }
                    }
                } else {
                    static const int edges[12][2] = {
                        {0, 1}, {1, 2}, {2, 3}, {3, 0},
                        {4, 5}, {5, 6}, {6, 7}, {7, 4},
                        {0, 4}, {1, 5}, {2, 6}, {3, 7}
                    };
                    struct FaceDef { int idx[4]; };
                    static const FaceDef s_boxFaces[6] = {
                        { { 4, 5, 6, 7 } }, { { 1, 0, 3, 2 } }, { { 7, 6, 2, 3 } },
                        { { 0, 1, 5, 4 } }, { { 5, 1, 2, 6 } }, { { 0, 4, 7, 3 } }
                    };
                    for (auto& inst : selList) {
                        if (auto p = std::dynamic_pointer_cast<Part>(inst)) {
                            std::set<int> verts;
                            if (EditorLayout::instance().shadingMode == EditorShadingMode::Vertex) {
                                for (int v : selectedVertices) verts.insert(v);
                            } else if (EditorLayout::instance().shadingMode == EditorShadingMode::Edge) {
                                for (int e : selectedEdges) {
                                    if (e >= 0 && e < 12) { verts.insert(edges[e][0]); verts.insert(edges[e][1]); }
                                }
                            } else if (EditorLayout::instance().shadingMode == EditorShadingMode::Face) {
                                for (int f : selectedFaces) {
                                    if (f >= 0 && f < 6) { for (int v = 0; v < 4; ++v) verts.insert(s_boxFaces[f].idx[v]); }
                                }
                            }
                            if (!verts.empty()) {
                                Engine::Math::Vector3 centroidLocal(0, 0, 0);
                                for (int v : verts) centroidLocal += p->getVertex(v);
                                centroidLocal = centroidLocal * (1.0f / (float)verts.size());
                                Engine::Math::Vector3 centroidWorld = p->getPosition() + centroidLocal;
                                Engine::Math::Vector3 delta = EditorLayout::instance().cursor3DPosition - centroidWorld;
                                for (int v : verts) {
                                    p->setVertex(v, p->getVertex(v) + delta);
                                }
                            }
                        }
                    }
                }
            };

            auto snapCursorToSelected = [&]() {
                auto selList = SelectionManager::instance().getSelectionList();
                if (selList.empty()) {
                    if (auto single = SelectionManager::instance().getSelected()) selList.push_back(single);
                }
                if (EditorLayout::instance().shadingMode == EditorShadingMode::Object) {
                    if (!selList.empty()) {
                        Engine::Math::Vector3 center(0, 0, 0);
                        int count = 0;
                        for (auto& inst : selList) {
                            if (auto p = std::dynamic_pointer_cast<Part>(inst)) {
                                center += p->getPosition();
                                count++;
                            }
                        }
                        if (count > 0) EditorLayout::instance().cursor3DPosition = center * (1.0f / (float)count);
                    }
                } else {
                    static const int edges[12][2] = {
                        {0, 1}, {1, 2}, {2, 3}, {3, 0},
                        {4, 5}, {5, 6}, {6, 7}, {7, 4},
                        {0, 4}, {1, 5}, {2, 6}, {3, 7}
                    };
                    struct FaceDef { int idx[4]; };
                    static const FaceDef s_boxFaces[6] = {
                        { { 4, 5, 6, 7 } }, { { 1, 0, 3, 2 } }, { { 7, 6, 2, 3 } },
                        { { 0, 1, 5, 4 } }, { { 5, 1, 2, 6 } }, { { 0, 4, 7, 3 } }
                    };
                    for (auto& inst : selList) {
                        if (auto p = std::dynamic_pointer_cast<Part>(inst)) {
                            std::set<int> verts;
                            if (EditorLayout::instance().shadingMode == EditorShadingMode::Vertex) {
                                for (int v : selectedVertices) verts.insert(v);
                            } else if (EditorLayout::instance().shadingMode == EditorShadingMode::Edge) {
                                for (int e : selectedEdges) {
                                    if (e >= 0 && e < 12) { verts.insert(edges[e][0]); verts.insert(edges[e][1]); }
                                }
                            } else if (EditorLayout::instance().shadingMode == EditorShadingMode::Face) {
                                for (int f : selectedFaces) {
                                    if (f >= 0 && f < 6) { for (int v = 0; v < 4; ++v) verts.insert(s_boxFaces[f].idx[v]); }
                                }
                            }
                            if (!verts.empty()) {
                                Engine::Math::Vector3 centroidLocal(0, 0, 0);
                                for (int v : verts) centroidLocal += p->getVertex(v);
                                centroidLocal = centroidLocal * (1.0f / (float)verts.size());
                                EditorLayout::instance().cursor3DPosition = p->getPosition() + centroidLocal;
                            }
                        }
                    }
                }
            };

            auto snapCursorToOrigin = [&]() {
                EditorLayout::instance().cursor3DPosition = Engine::Math::Vector3(0.0f, 0.0f, 0.0f);
            };

            auto snapCursorToGrid = [&]() {
                EditorLayout::instance().cursor3DPosition.x = std::round(EditorLayout::instance().cursor3DPosition.x);
                EditorLayout::instance().cursor3DPosition.y = std::round(EditorLayout::instance().cursor3DPosition.y);
                EditorLayout::instance().cursor3DPosition.z = std::round(EditorLayout::instance().cursor3DPosition.z);
            };

            auto snapCursorToActive = [&]() {
                if (auto active = SelectionManager::instance().getSelected()) {
                    if (auto p = std::dynamic_pointer_cast<Part>(active)) {
                        EditorLayout::instance().cursor3DPosition = p->getPosition();
                    }
                }
            };

            items = {
                { "Selection to Grid", "1", -90.0f, snapSelectionToGrid },
                { "Selection to Cursor", "2", 0.0f, snapSelectionToCursor },
                { "Cursor to Selected", "3", 90.0f, snapCursorToSelected },
                { "Cursor to World Origin", "4", 180.0f, snapCursorToOrigin },
                { "Cursor to Grid", "5", -145.0f, snapCursorToGrid },
                { "Cursor to Active", "6", -35.0f, snapCursorToActive }
            };
        } else if (showModesPieMenu) {
            items = {
                { "Object Mode", "1", -90.0f, [&]() {
                    EditorLayout::instance().shadingMode = EditorShadingMode::Object;
                    selectedVertices.clear(); selectedEdges.clear(); selectedFaces.clear();
                } },
                { "Face Mode", "2", 0.0f, [&]() {
                    EditorLayout::instance().shadingMode = EditorShadingMode::Face;
                } },
                { "Edge Mode", "3", 90.0f, [&]() {
                    EditorLayout::instance().shadingMode = EditorShadingMode::Edge;
                } },
                { "Vertex Mode", "4", 180.0f, [&]() {
                    EditorLayout::instance().shadingMode = EditorShadingMode::Vertex;
                } }
            };
        }

        // Draw and interact with items
        float radius = 105.0f;
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& item = items[i];
            float rad = item.angleDeg * 3.14159265f / 180.0f;
            ImVec2 itemCenter(pieMenuCenter.x + std::cos(rad) * radius,
                              pieMenuCenter.y + std::sin(rad) * radius);

            ImVec2 textSize = ImGui::CalcTextSize(item.label.c_str());
            float padX = 14.0f, padY = 8.0f;
            ImVec2 bMin(itemCenter.x - textSize.x * 0.5f - padX, itemCenter.y - textSize.y * 0.5f - padY);
            ImVec2 bMax(itemCenter.x + textSize.x * 0.5f + padX, itemCenter.y + textSize.y * 0.5f + padY);

            bool isItemHov = (mPos.x >= bMin.x && mPos.x <= bMax.x && mPos.y >= bMin.y && mPos.y <= bMax.y);
            
            bool keyPressed = false;
            if (item.key == "1" && ImGui::IsKeyPressed(ImGuiKey_1)) keyPressed = true;
            if (item.key == "2" && ImGui::IsKeyPressed(ImGuiKey_2)) keyPressed = true;
            if (item.key == "3" && ImGui::IsKeyPressed(ImGuiKey_3)) keyPressed = true;
            if (item.key == "4" && ImGui::IsKeyPressed(ImGuiKey_4)) keyPressed = true;
            if (item.key == "5" && ImGui::IsKeyPressed(ImGuiKey_5)) keyPressed = true;
            if (item.key == "6" && ImGui::IsKeyPressed(ImGuiKey_6)) keyPressed = true;

            if (isItemHov || keyPressed) {
                fgDl->AddLine(pieMenuCenter, itemCenter, COL(T.accent), 2.0f);
                fgDl->AddRectFilled(bMin, bMax, IM_COL32(30, 45, 60, 245), 6.0f);
                fgDl->AddRect(bMin, bMax, COL(T.accent), 6.0f, 0, 2.0f);
                fgDl->AddText(ImVec2(bMin.x + padX, bMin.y + padY), COL(T.accent), item.label.c_str());
            } else {
                fgDl->AddRectFilled(bMin, bMax, IM_COL32(20, 20, 24, 230), 6.0f);
                fgDl->AddRect(bMin, bMax, COL(T.border), 6.0f, 0, 1.2f);
                fgDl->AddText(ImVec2(bMin.x + padX, bMin.y + padY), COL(T.textPrimary), item.label.c_str());
            }

            // Key badge on top-right corner
            ImVec2 badgePos(bMax.x - 4.0f, bMin.y - 4.0f);
            fgDl->AddCircleFilled(badgePos, 7.5f, IM_COL32(35, 35, 42, 255));
            fgDl->AddCircle(badgePos, 7.5f, COL(T.border), 0, 1.0f);
            fgDl->AddText(ImVec2(badgePos.x - 3.5f, badgePos.y - 6.5f), COL(T.textSecondary), item.key.c_str());

            if ((isItemHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) || keyPressed) {
                item.action();
                showSnapPieMenu = false;
                showModesPieMenu = false;
            }
        }
    }

    // Render Floating Operator Panel (Adjust Last Operation)
    Editor::UI::ModelingOperatorPanel::render(screenPos, avail);

    ImGui::End();
    ImGui::PopStyleVar();
}

void ViewportPanel::handleGizmoInput(Engine::Renderer::Camera& camera) {
    auto& mCtx = Editor::Modeling::ModelingContext::instance();
    auto selectionList = SelectionManager::instance().getSelectionList();
    std::vector<std::shared_ptr<Part>> selectedParts;
    for (auto& inst : selectionList) {
        if (auto p = std::dynamic_pointer_cast<Part>(inst)) {
            selectedParts.push_back(p);
        }
    }
    if (selectedParts.empty()) {
        if (auto singlePart = std::dynamic_pointer_cast<Part>(SelectionManager::instance().getSelected())) {
            selectedParts.push_back(singlePart);
        }
    }
    if (selectedParts.empty()) {
        mCtx.clearSelection();
        return;
    }

    // ── Face Mode Multi-Gizmo ────────────────────────────────────────────────
    if (EditorLayout::instance().shadingMode == EditorShadingMode::Face) {
        if (selectedParts.size() == 1 && !mCtx.selectedFaces.empty()) {
            auto part = selectedParts[0];
            part->ensureEditableMesh();
            auto mesh = part->getEditableMesh();
            if (!mesh) return;

            // Collect all unique vertex indices from selected faces
            std::set<uint32_t> uniqueVerts;
            for (uint32_t fIdx : mCtx.selectedFaces) {
                if (fIdx < mesh->getFaces().size() && !mesh->getFaces()[fIdx].deleted) {
                    for (uint32_t v : mesh->getFaces()[fIdx].vertices) {
                        uniqueVerts.insert(v);
                    }
                }
            }

            if (!uniqueVerts.empty()) {
                Engine::Math::Vector3 centroidLocal(0, 0, 0);
                for (uint32_t vIdx : uniqueVerts) {
                    centroidLocal += part->getVertex(vIdx);
                }
                centroidLocal = centroidLocal * (1.0f / (float)uniqueVerts.size());
                Engine::Math::Vector3 centroidWorld = part->getPosition() + centroidLocal;

                ImGuizmo::SetDrawlist();
                ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y,
                                  ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

                Engine::Math::Matrix4 vTransform = Engine::Math::Matrix4::translation(centroidWorld);
                Engine::Math::Matrix4 view = camera.getViewMatrix();
                Engine::Math::Matrix4 proj = camera.getProjectionMatrix(
                    (float)currentWidth / (float)currentHeight);

                float snapValues[3] = { 1.0f, 1.0f, 1.0f };
                float* snapPtr = EditorLayout::instance().gridSnap ? snapValues : nullptr;

                static std::map<uint32_t, Engine::Math::Vector3> s_faceDragStartPositions;
                static Engine::Math::Vector3 s_faceDragStartCenter;

                ImGuizmo::Manipulate(
                    view.m.data(), proj.m.data(),
                    ImGuizmo::TRANSLATE,
                    ImGuizmo::WORLD,
                    vTransform.m.data(),
                    nullptr,
                    snapPtr
                );

                if (ImGuizmo::IsUsing()) {
                    if (!isDraggingGizmo) {
                        isDraggingGizmo = true;
                        s_faceDragStartCenter = centroidWorld;
                        s_faceDragStartPositions.clear();
                        for (uint32_t vIdx : uniqueVerts) {
                            s_faceDragStartPositions[vIdx] = part->getVertex(vIdx);
                        }
                    }
                    Engine::Math::Vector3 newWorldCenter(vTransform.m[12], vTransform.m[13], vTransform.m[14]);
                    Engine::Math::Vector3 delta = newWorldCenter - s_faceDragStartCenter;
                    for (uint32_t vIdx : uniqueVerts) {
                        part->setVertex(vIdx, s_faceDragStartPositions[vIdx] + delta);
                    }
                } else {
                    isDraggingGizmo = false;
                }
                return; // Face is selected and manipulated
            }
        }
        return; // In Face mode, do not draw whole-object gizmo
    }

    // ── Vertex Mode Multi-Gizmo ──────────────────────────────────────────
    if (EditorLayout::instance().shadingMode == EditorShadingMode::Vertex) {
        if (selectedParts.size() == 1 && !mCtx.selectedVertices.empty()) {
            auto part = selectedParts[0];
            part->ensureEditableMesh();

            // Calculate Centroid of all selected vertices
            Engine::Math::Vector3 centroidLocal(0, 0, 0);
            for (uint32_t vIdx : mCtx.selectedVertices) {
                centroidLocal += part->getVertex(vIdx);
            }
            centroidLocal = centroidLocal * (1.0f / (float)mCtx.selectedVertices.size());
            Engine::Math::Vector3 centroidWorld = part->getPosition() + centroidLocal;

            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y,
                              ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

            Engine::Math::Matrix4 vTransform = Engine::Math::Matrix4::translation(centroidWorld);
            Engine::Math::Matrix4 view = camera.getViewMatrix();
            Engine::Math::Matrix4 proj = camera.getProjectionMatrix(
                (float)currentWidth / (float)currentHeight);

            float snapValues[3] = { 1.0f, 1.0f, 1.0f };
            float* snapPtr = EditorLayout::instance().gridSnap ? snapValues : nullptr;

            static std::map<uint32_t, Engine::Math::Vector3> s_vertDragStartPositions;
            static Engine::Math::Vector3 s_vertDragStartCenter;

            ImGuizmo::Manipulate(
                view.m.data(), proj.m.data(),
                ImGuizmo::TRANSLATE,
                ImGuizmo::WORLD,
                vTransform.m.data(),
                nullptr,
                snapPtr
            );

            if (ImGuizmo::IsUsing()) {
                if (!isDraggingGizmo) {
                    isDraggingGizmo = true;
                    s_vertDragStartCenter = centroidWorld;
                    s_vertDragStartPositions.clear();
                    for (uint32_t vIdx : mCtx.selectedVertices) {
                        s_vertDragStartPositions[vIdx] = part->getVertex(vIdx);
                    }
                }
                Engine::Math::Vector3 newWorldCenter(vTransform.m[12], vTransform.m[13], vTransform.m[14]);
                Engine::Math::Vector3 delta = newWorldCenter - s_vertDragStartCenter;
                for (uint32_t vIdx : mCtx.selectedVertices) {
                    part->setVertex(vIdx, s_vertDragStartPositions[vIdx] + delta);
                }
            } else {
                isDraggingGizmo = false;
            }
        }
        return; // In Vertex mode, do not draw whole-object gizmo
    }

    // ── Edge Mode Multi-Gizmo ────────────────────────────────────────────
    if (EditorLayout::instance().shadingMode == EditorShadingMode::Edge) {
        if (selectedParts.size() == 1 && !mCtx.selectedEdges.empty()) {
            auto part = selectedParts[0];
            part->ensureEditableMesh();
            auto mesh = part->getEditableMesh();
            if (!mesh) return;

            // Collect all unique vertex indices from selected edges
            std::set<uint32_t> uniqueVerts;
            for (uint32_t eIdx : mCtx.selectedEdges) {
                if (eIdx < mesh->getEdges().size() && !mesh->getEdges()[eIdx].deleted) {
                    uniqueVerts.insert(mesh->getEdges()[eIdx].v0);
                    uniqueVerts.insert(mesh->getEdges()[eIdx].v1);
                }
            }

            if (!uniqueVerts.empty()) {
                Engine::Math::Vector3 centroidLocal(0, 0, 0);
                for (uint32_t vIdx : uniqueVerts) {
                    centroidLocal += part->getVertex(vIdx);
                }
                centroidLocal = centroidLocal * (1.0f / (float)uniqueVerts.size());
                Engine::Math::Vector3 centroidWorld = part->getPosition() + centroidLocal;

                ImGuizmo::SetDrawlist();
                ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y,
                                  ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

                Engine::Math::Matrix4 vTransform = Engine::Math::Matrix4::translation(centroidWorld);
                Engine::Math::Matrix4 view = camera.getViewMatrix();
                Engine::Math::Matrix4 proj = camera.getProjectionMatrix(
                    (float)currentWidth / (float)currentHeight);

                float snapValues[3] = { 1.0f, 1.0f, 1.0f };
                float* snapPtr = EditorLayout::instance().gridSnap ? snapValues : nullptr;

                static std::map<uint32_t, Engine::Math::Vector3> s_edgeDragStartPositions;
                static Engine::Math::Vector3 s_edgeDragStartCenter;

                ImGuizmo::Manipulate(
                    view.m.data(), proj.m.data(),
                    ImGuizmo::TRANSLATE,
                    ImGuizmo::WORLD,
                    vTransform.m.data(),
                    nullptr,
                    snapPtr
                );

                if (ImGuizmo::IsUsing()) {
                    if (!isDraggingGizmo) {
                        isDraggingGizmo = true;
                        s_edgeDragStartCenter = centroidWorld;
                        s_edgeDragStartPositions.clear();
                        for (uint32_t vIdx : uniqueVerts) {
                            s_edgeDragStartPositions[vIdx] = part->getVertex(vIdx);
                        }
                    }
                    Engine::Math::Vector3 newWorldCenter(vTransform.m[12], vTransform.m[13], vTransform.m[14]);
                    Engine::Math::Vector3 delta = newWorldCenter - s_edgeDragStartCenter;
                    for (uint32_t vIdx : uniqueVerts) {
                        part->setVertex(vIdx, s_edgeDragStartPositions[vIdx] + delta);
                    }
                } else {
                    isDraggingGizmo = false;
                }
            }
        }
        return; // In Edge mode, do not draw whole-object gizmo
    }

    // Calculate Bounding Centroid Center
    Engine::Math::Vector3 center(0.0f, 0.0f, 0.0f);
    for (const auto& p : selectedParts) {
        center += p->getPosition();
    }
    center = center * (1.0f / (float)selectedParts.size());

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y,
                      ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

    // Single part scale or unit scale for group
    Engine::Math::Vector3 gizmoScale = (selectedParts.size() == 1) ? selectedParts[0]->getSize() : Engine::Math::Vector3(1.0f, 1.0f, 1.0f);
    Engine::Math::Matrix4 transform = Engine::Math::Matrix4::fromPositionAndSize(center, gizmoScale);
    Engine::Math::Matrix4 view = camera.getViewMatrix();
    Engine::Math::Matrix4 proj = camera.getProjectionMatrix(
        (float)currentWidth / (float)currentHeight);

    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Q)) EditorLayout::instance().currentTool = EditorTool::Select;
        if (ImGui::IsKeyPressed(ImGuiKey_W)) EditorLayout::instance().currentTool = EditorTool::Move;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) EditorLayout::instance().currentTool = EditorTool::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) EditorLayout::instance().currentTool = EditorTool::Scale;
    }

    EditorTool curTool = EditorLayout::instance().currentTool;
    if (curTool == EditorTool::Select) {
        return; // No gizmo drawn in Select mode
    }

    ImGuizmo::OPERATION currentOp = ImGuizmo::TRANSLATE;
    if (curTool == EditorTool::Move) currentOp = ImGuizmo::TRANSLATE;
    else if (curTool == EditorTool::Rotate) currentOp = ImGuizmo::ROTATE;
    else if (curTool == EditorTool::Scale) currentOp = ImGuizmo::SCALE;

    ImGuizmo::MODE currentMode = s_worldSpace ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

    float snapValues[3] = { 1.0f, 1.0f, 1.0f };
    if (currentOp == ImGuizmo::ROTATE) {
        snapValues[0] = snapValues[1] = snapValues[2] = 15.0f; // 15 degrees
    }
    float* snapPtr = EditorLayout::instance().gridSnap ? snapValues : nullptr;

    static std::map<Part*, Engine::Math::Vector3> s_dragStartPositions;
    static std::map<Part*, Engine::Math::Vector3> s_dragStartSizes;
    static Engine::Math::Vector3 s_gizmoStartCenter(0, 0, 0);

    ImGuizmo::Manipulate(view.m.data(), proj.m.data(),
                         currentOp, currentMode, transform.m.data(), nullptr, snapPtr);

    if (ImGuizmo::IsUsing()) {
        if (!isDraggingGizmo) {
            isDraggingGizmo    = true;
            s_gizmoStartCenter = center;
            s_dragStartPositions.clear();
            s_dragStartSizes.clear();
            for (auto& p : selectedParts) {
                s_dragStartPositions[p.get()] = p->getPosition();
                s_dragStartSizes[p.get()] = p->getSize();
            }
        }

        Engine::Math::Vector3 newTrans, newScale;
        Engine::Math::Quaternion newRot;
        transform.decompose(newTrans, newRot, newScale);

        Engine::Math::Vector3 deltaPos = newTrans - s_gizmoStartCenter;

        if (currentOp == ImGuizmo::TRANSLATE) {
            for (auto& p : selectedParts) {
                p->setPosition(s_dragStartPositions[p.get()] + deltaPos);
            }
        } else if (currentOp == ImGuizmo::SCALE) {
            if (selectedParts.size() == 1) {
                selectedParts[0]->setSize(newScale);
                selectedParts[0]->setPosition(newTrans);
            } else {
                for (auto& p : selectedParts) {
                    Engine::Math::Vector3 origSz = s_dragStartSizes[p.get()];
                    p->setSize(Engine::Math::Vector3(origSz.x * newScale.x, origSz.y * newScale.y, origSz.z * newScale.z));
                    Engine::Math::Vector3 origPos = s_dragStartPositions[p.get()];
                    Engine::Math::Vector3 rel = origPos - s_gizmoStartCenter;
                    Engine::Math::Vector3 scaledRel(rel.x * newScale.x, rel.y * newScale.y, rel.z * newScale.z);
                    p->setPosition(s_gizmoStartCenter + scaledRel + deltaPos);
                }
            }
        } else if (currentOp == ImGuizmo::ROTATE) {
            if (selectedParts.size() == 1) {
                selectedParts[0]->setPosition(newTrans);
            } else {
                Engine::Math::Matrix4 rotMat = Engine::Math::Matrix4::fromRotation(newRot);
                for (auto& p : selectedParts) {
                    Engine::Math::Vector3 origPos = s_dragStartPositions[p.get()];
                    Engine::Math::Vector3 rel = origPos - s_gizmoStartCenter;
                    float rx = rel.x * rotMat.m[0] + rel.y * rotMat.m[4] + rel.z * rotMat.m[8];
                    float ry = rel.x * rotMat.m[1] + rel.y * rotMat.m[5] + rel.z * rotMat.m[9];
                    float rz = rel.x * rotMat.m[2] + rel.y * rotMat.m[6] + rel.z * rotMat.m[10];
                    p->setPosition(s_gizmoStartCenter + Engine::Math::Vector3(rx, ry, rz) + deltaPos);
                }
            }
        }
    } else {
        if (isDraggingGizmo) {
            for (auto& p : selectedParts) {
                UndoStack::instance().pushPropertyChangeCommand(
                    p, "Position", s_dragStartPositions[p.get()], p->getPosition());
            }
            isDraggingGizmo = false;
            s_dragStartPositions.clear();
            s_dragStartSizes.clear();
        }
    }
}

void ViewportPanel::handleCameraControls(Engine::Renderer::Camera& camera, bool isHovered) {
    static CameraViewMode s_prevCameraMode = (CameraViewMode)-1;
    static int s_prevDegree90Index = -1;
    static Engine::Math::Vector3 s_cameraPivot(0.0f, 0.0f, 0.0f);
    static float s_yaw = 0.0f;
    static float s_pitch = -0.2f;
    static bool s_isMouseLooking = false;
    static bool s_isPanning = false;
    static float s_dragAccumX = 0.0f;
    static float s_dragAccumY = 0.0f;
    static bool s_is90Swiping = false;
    static int s_topRotIndex = 0;
    static int s_bottomRotIndex = 0;

    // Sync camera projection mode with EditorLayout
    camera.isOrthographic = EditorLayout::instance().isOrthographic;

    // 1. Update Camera Pivot from active selection or keep previous
    auto selection = SelectionManager::instance().getSelectionList();
    if (!selection.empty() || SelectionManager::instance().getSelected()) {
        Engine::Math::Vector3 targetCenter(0, 0, 0);
        int count = 0;
        for (auto& inst : selection) {
            if (auto part = std::dynamic_pointer_cast<Part>(inst)) {
                targetCenter += part->getPosition();
                count++;
            }
        }
        if (count == 0) {
            if (auto singlePart = std::dynamic_pointer_cast<Part>(SelectionManager::instance().getSelected())) {
                targetCenter = singlePart->getPosition();
                count = 1;
            }
        }
        if (count > 0) {
            s_cameraPivot = targetCenter * (1.0f / (float)count);
        }
    }

    CameraViewMode curMode = EditorLayout::instance().cameraMode;
    int cur90Idx = EditorLayout::instance().degree90Index;

    // Helper lambda to calculate 90° camera position cleanly
    auto apply90DegreeView = [&](int idx) {
        Engine::Math::Vector3 toCam = camera.position - s_cameraPivot;
        float dist = toCam.length();
        if (dist < 5.0f || dist > 200.0f) dist = 25.0f;

        if (idx == 0) { // Front (+Z looking forward)
            camera.position = Engine::Math::Vector3(s_cameraPivot.x, s_cameraPivot.y, s_cameraPivot.z - dist);
            camera.forward = Engine::Math::Vector3(0.0f, 0.0f, 1.0f);
            camera.up = Engine::Math::Vector3(0.0f, 1.0f, 0.0f);
            s_yaw = 0.0f; s_pitch = 0.0f;
        } else if (idx == 1) { // Right (-X looking left)
            camera.position = Engine::Math::Vector3(s_cameraPivot.x + dist, s_cameraPivot.y, s_cameraPivot.z);
            camera.forward = Engine::Math::Vector3(-1.0f, 0.0f, 0.0f);
            camera.up = Engine::Math::Vector3(0.0f, 1.0f, 0.0f);
            s_yaw = 1.5707963f; s_pitch = 0.0f;
        } else if (idx == 2) { // Back (-Z looking backward)
            camera.position = Engine::Math::Vector3(s_cameraPivot.x, s_cameraPivot.y, s_cameraPivot.z + dist);
            camera.forward = Engine::Math::Vector3(0.0f, 0.0f, -1.0f);
            camera.up = Engine::Math::Vector3(0.0f, 1.0f, 0.0f);
            s_yaw = 3.14159265f; s_pitch = 0.0f;
        } else if (idx == 3) { // Left (+X looking right)
            camera.position = Engine::Math::Vector3(s_cameraPivot.x - dist, s_cameraPivot.y, s_cameraPivot.z);
            camera.forward = Engine::Math::Vector3(1.0f, 0.0f, 0.0f);
            camera.up = Engine::Math::Vector3(0.0f, 1.0f, 0.0f);
            s_yaw = -1.5707963f; s_pitch = 0.0f;
        } else if (idx == 4) { // Top (+Y looking down)
            camera.position = Engine::Math::Vector3(s_cameraPivot.x, s_cameraPivot.y + dist, s_cameraPivot.z);
            camera.forward = Engine::Math::Vector3(0.0f, -1.0f, 0.0f);
            
            // 4 compass orientations for top view
            if (s_topRotIndex == 0) camera.up = Engine::Math::Vector3(0.0f, 0.0f, 1.0f);
            else if (s_topRotIndex == 1) camera.up = Engine::Math::Vector3(1.0f, 0.0f, 0.0f);
            else if (s_topRotIndex == 2) camera.up = Engine::Math::Vector3(0.0f, 0.0f, -1.0f);
            else camera.up = Engine::Math::Vector3(-1.0f, 0.0f, 0.0f);
            
            s_yaw = 0.0f; s_pitch = -1.55f;
        } else if (idx == 5) { // Bottom (-Y looking up)
            camera.position = Engine::Math::Vector3(s_cameraPivot.x, s_cameraPivot.y - dist, s_cameraPivot.z);
            camera.forward = Engine::Math::Vector3(0.0f, 1.0f, 0.0f);
            
            // 4 compass orientations for bottom view
            if (s_bottomRotIndex == 0) camera.up = Engine::Math::Vector3(0.0f, 0.0f, -1.0f);
            else if (s_bottomRotIndex == 1) camera.up = Engine::Math::Vector3(1.0f, 0.0f, 0.0f);
            else if (s_bottomRotIndex == 2) camera.up = Engine::Math::Vector3(0.0f, 0.0f, 1.0f);
            else camera.up = Engine::Math::Vector3(-1.0f, 0.0f, 0.0f);
            
            s_yaw = 0.0f; s_pitch = 1.55f;
        }
    };

    // 2. Camera Mode Snapping when mode changes
    bool modeChanged = (curMode != s_prevCameraMode) || (curMode == CameraViewMode::Degree90 && cur90Idx != s_prevDegree90Index);
    if (modeChanged) {
        s_prevCameraMode = curMode;
        s_prevDegree90Index = cur90Idx;

        if (curMode == CameraViewMode::Degree90) {
            apply90DegreeView(cur90Idx);
        }
    }

    // 3. Right Mouse Button Drag Interaction
    if (curMode == CameraViewMode::Degree90) {
        // In 90 Degree Mode: Right-click swipe turns 90° around focus point
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && (isHovered || s_is90Swiping)) {
            s_is90Swiping = true;
            ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
            s_dragAccumX += mouseDelta.x;
            s_dragAccumY += mouseDelta.y;

            const float swipeThreshold = 55.0f;
            float absX = std::abs(s_dragAccumX);
            float absY = std::abs(s_dragAccumY);

            if (absX >= swipeThreshold && absX >= absY) {
                // Horizontal swipe
                if (cur90Idx == 4) {
                    // In Top view: rotate compass orientation
                    if (s_dragAccumX > 0.0f) s_topRotIndex = (s_topRotIndex + 1) % 4;
                    else s_topRotIndex = (s_topRotIndex + 3) % 4;
                } else if (cur90Idx == 5) {
                    // In Bottom view: rotate compass orientation
                    if (s_dragAccumX > 0.0f) s_bottomRotIndex = (s_bottomRotIndex + 1) % 4;
                    else s_bottomRotIndex = (s_bottomRotIndex + 3) % 4;
                } else {
                    // Horizontal 90° rotation
                    if (s_dragAccumX > 0.0f) cur90Idx = (cur90Idx + 1) % 4; // Right
                    else cur90Idx = (cur90Idx + 3) % 4; // Left
                    EditorLayout::instance().degree90Index = cur90Idx;
                    s_prevDegree90Index = cur90Idx;
                }
                s_dragAccumX = 0.0f;
                s_dragAccumY = 0.0f;
                apply90DegreeView(cur90Idx);
            } else if (absY >= swipeThreshold && absY > absX) {
                // Vertical swipe
                // Swiping DOWN (deltaY > 0) -> Top View (looking down from above)
                // Swiping UP (deltaY < 0) -> Bottom View (looking up from below)
                if (s_dragAccumY > 0.0f) {
                    // Swiped DOWN:
                    if (cur90Idx == 5) cur90Idx = 0; // return to front if was in bottom view
                    else cur90Idx = 4; // go to Top view
                } else {
                    // Swiped UP:
                    if (cur90Idx == 4) cur90Idx = 0; // return to front if was in top view
                    else cur90Idx = 5; // go to Bottom view
                }
                s_dragAccumX = 0.0f;
                s_dragAccumY = 0.0f;
                EditorLayout::instance().degree90Index = cur90Idx;
                s_prevDegree90Index = cur90Idx;
                apply90DegreeView(cur90Idx);
            }
        } else {
            s_is90Swiping = false;
            s_dragAccumX = 0.0f;
            s_dragAccumY = 0.0f;
        }
    } else {
        // Free View: Right-click drag rotates camera with smooth mouse look
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && (isHovered || s_isMouseLooking)) {
            s_isMouseLooking = true;
            ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
            if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
                float sensitivity = 0.0035f;
                s_yaw += mouseDelta.x * sensitivity;
                s_pitch -= mouseDelta.y * sensitivity;
                s_pitch = std::clamp(s_pitch, -1.55f, 1.55f);

                float cp = std::cos(s_pitch);
                camera.forward.x = -std::sin(s_yaw) * cp;
                camera.forward.y = std::sin(s_pitch);
                camera.forward.z = std::cos(s_yaw) * cp;
                camera.forward.normalize();
                camera.up = Engine::Math::Vector3(0.0f, 1.0f, 0.0f);

                if (EditorLayout::instance().cameraMode != CameraViewMode::Free) {
                    EditorLayout::instance().cameraMode = CameraViewMode::Free;
                    s_prevCameraMode = CameraViewMode::Free;
                }
            }
        } else {
            s_isMouseLooking = false;
        }
    }

    // 4. WASD + QE Keyboard Navigation
    bool canMoveKeys = (isHovered || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) && !ImGui::GetIO().WantTextInput;
    bool modifierHeld = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyAlt;
    if (canMoveKeys && !modifierHeld) {
        Engine::Math::Vector3 screenRight = camera.forward.cross(camera.up);
        screenRight.normalize();

        float speed = 18.0f * ImGui::GetIO().DeltaTime;
        if (ImGui::GetIO().KeyShift) speed *= 3.0f;

        if (camera.isOrthographic) {
            // In Orthographic mode: 2D Planar Pan navigation (keeps camera distance constant so shadows/LODs are unchanged)
            if (ImGui::IsKeyDown(ImGuiKey_W)) camera.position += camera.up * speed;
            if (ImGui::IsKeyDown(ImGuiKey_S)) camera.position -= camera.up * speed;
            if (ImGui::IsKeyDown(ImGuiKey_D)) camera.position += screenRight * speed;
            if (ImGui::IsKeyDown(ImGuiKey_A)) camera.position -= screenRight * speed;
            if (ImGui::IsKeyDown(ImGuiKey_E)) {
                camera.orthoSize -= speed * (camera.orthoSize * 0.08f + 0.2f);
                if (camera.orthoSize < 0.5f) camera.orthoSize = 0.5f;
            }
            if (ImGui::IsKeyDown(ImGuiKey_Q)) {
                camera.orthoSize += speed * (camera.orthoSize * 0.08f + 0.2f);
                if (camera.orthoSize > 500.0f) camera.orthoSize = 500.0f;
            }
        } else {
            // In Perspective mode: 3D Fly navigation
            if (ImGui::IsKeyDown(ImGuiKey_W)) camera.position += camera.forward * speed;
            if (ImGui::IsKeyDown(ImGuiKey_S)) camera.position -= camera.forward * speed;
            if (ImGui::IsKeyDown(ImGuiKey_D)) camera.position += screenRight * speed;
            if (ImGui::IsKeyDown(ImGuiKey_A)) camera.position -= screenRight * speed;
            if (ImGui::IsKeyDown(ImGuiKey_E)) camera.position += camera.up * speed;
            if (ImGui::IsKeyDown(ImGuiKey_Q)) camera.position -= camera.up * speed;
        }
    }

    // 5. Middle Mouse Button Pan
    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) && (isHovered || s_isPanning)) {
        s_isPanning = true;
        ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
        if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
            Engine::Math::Vector3 screenRight = camera.forward.cross(camera.up);
            screenRight.normalize();
            float panSpeed = 0.035f;
            camera.position -= screenRight * (mouseDelta.x * panSpeed);
            camera.position += camera.up * (mouseDelta.y * panSpeed);
        }
    } else {
        s_isPanning = false;
    }

    // 6. Mouse Wheel Zoom (Dolly in / out or Ortho scale)
    if (isHovered && ImGui::GetIO().MouseWheel != 0.0f) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (camera.isOrthographic) {
            camera.orthoSize -= wheel * (camera.orthoSize * 0.12f + 0.3f);
            if (camera.orthoSize < 0.5f) camera.orthoSize = 0.5f;
            if (camera.orthoSize > 500.0f) camera.orthoSize = 500.0f;
        } else {
            float zoomAmount = wheel * 3.0f;
            camera.position += camera.forward * zoomAmount;
        }
    }

    // 7. Focus Shortcut ('F' Key)
    if (isHovered && ImGui::IsKeyPressed(ImGuiKey_F) && !ImGui::GetIO().WantTextInput) {
        if (camera.isOrthographic) {
            camera.orthoSize = 15.0f;
            camera.position = s_cameraPivot - camera.forward * 25.0f;
        } else {
            camera.position = s_cameraPivot - camera.forward * 15.0f;
        }
    }
}
