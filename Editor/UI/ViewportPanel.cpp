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
#include <functional>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <set>
#include <vector>

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

    // ── Edge & Vertex Mode Overlays and Picking / Multi-Selection ────────────
    if (EditorLayout::instance().shadingMode == EditorShadingMode::Edge ||
        EditorLayout::instance().shadingMode == EditorShadingMode::Vertex) {
        
        bool isEdgeMode = (EditorLayout::instance().shadingMode == EditorShadingMode::Edge);
        bool isVertexMode = (EditorLayout::instance().shadingMode == EditorShadingMode::Vertex);

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

        ImVec2 mousePos = ImGui::GetIO().MousePos;
        bool shiftHeld = ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl;
        bool canPick = isHovered && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver() && !ImGui::IsMouseDown(ImGuiMouseButton_Right);

        int hoveredVertex = -1;
        float bestVertexDist = 14.0f;

        int hoveredEdge = -1;
        float bestEdgeDist = 12.0f;

        static const int edges[12][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Front quad
            {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Back quad
            {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Connecting edges
        };

        auto contains = [](const std::vector<int>& vec, int val) {
            return std::find(vec.begin(), vec.end(), val) != vec.end();
        };

        std::function<void(const std::shared_ptr<Instance>&)> drawDeformOverlay = [&](const std::shared_ptr<Instance>& inst) {
            if (auto part = std::dynamic_pointer_cast<Part>(inst)) {
                Engine::Math::Vector3 pos = part->getPosition();

                bool isSelected = (part == sel);
                if (!isSelected) {
                    for (auto& s : selList) { if (s == part) { isSelected = true; break; } }
                }

                Engine::Math::Vector3 corners[8];
                for (int i = 0; i < 8; ++i) {
                    corners[i] = pos + part->getVertex(i);
                }

                ImVec2 sPts[8];
                bool visible[8];
                for (int i = 0; i < 8; ++i) {
                    visible[i] = project(corners[i], sPts[i]);
                }

                // Check Picking for Selected Part
                if (isSelected && canPick && !isBoxSelecting) {
                    if (isVertexMode) {
                        for (int i = 0; i < 8; ++i) {
                            if (visible[i]) {
                                float dx = mousePos.x - sPts[i].x;
                                float dy = mousePos.y - sPts[i].y;
                                float dist = std::sqrt(dx * dx + dy * dy);
                                if (dist < bestVertexDist) {
                                    bestVertexDist = dist;
                                    hoveredVertex = i;
                                }
                            }
                        }
                    } else if (isEdgeMode) {
                        for (int e = 0; e < 12; ++e) {
                            int i0 = edges[e][0], i1 = edges[e][1];
                            if (visible[i0] && visible[i1]) {
                                float dist = distToSegment(mousePos, sPts[i0], sPts[i1]);
                                if (dist < bestEdgeDist) {
                                    bestEdgeDist = dist;
                                    hoveredEdge = e;
                                }
                            }
                        }
                    }
                }

                // 12 edges rendering
                for (int e = 0; e < 12; ++e) {
                    int i0 = edges[e][0], i1 = edges[e][1];
                    if (visible[i0] && visible[i1]) {
                        bool isHoveredE = (isSelected && isEdgeMode && hoveredEdge == e);
                        bool isCurrentE = (isSelected && isEdgeMode && contains(selectedEdges, e));

                        if (isCurrentE) {
                            // Selected Edge: Thick glowing cyan bar
                            dl->AddLine(sPts[i0], sPts[i1], IM_COL32(0, 220, 255, 255), 4.5f);
                            dl->AddLine(sPts[i0], sPts[i1], IM_COL32(255, 255, 255, 255), 2.0f);
                        } else if (isHoveredE) {
                            // Hovered Edge: Glowing yellow bar
                            dl->AddLine(sPts[i0], sPts[i1], IM_COL32(255, 230, 70, 255), 3.5f);
                        } else {
                            // Default Edge
                            ImU32 lineCol = isSelected ? IM_COL32(130, 217, 255, 220) : IM_COL32(180, 180, 180, 160);
                            dl->AddLine(sPts[i0], sPts[i1], lineCol, isSelected ? 2.5f : 1.8f);
                        }
                    }
                }

                // Vertex Corner dots (Rendered in Vertex Mode or for selected Edge endpoints)
                if (isVertexMode || isEdgeMode) {
                    for (int i = 0; i < 8; ++i) {
                        if (visible[i]) {
                            if (isVertexMode) {
                                bool isHoveredVert = (isSelected && hoveredVertex == i);
                                bool isCurrentVert = (isSelected && contains(selectedVertices, i));

                                if (isCurrentVert) {
                                    // Active selected vertex: Glowing cyan target with inner bright white dot
                                    dl->AddCircleFilled(sPts[i], 6.5f, IM_COL32(0, 210, 255, 255));
                                    dl->AddCircleFilled(sPts[i], 4.0f, IM_COL32(255, 255, 255, 255));
                                    dl->AddCircle(sPts[i], 8.5f, IM_COL32(0, 140, 255, 200), 0, 1.8f);
                                } else if (isHoveredVert) {
                                    // Hovered vertex: Yellow pulse ring
                                    dl->AddCircleFilled(sPts[i], 5.5f, IM_COL32(255, 240, 80, 255));
                                    dl->AddCircle(sPts[i], 7.5f, IM_COL32(255, 180, 0, 220), 0, 1.5f);
                                } else {
                                    // Unselected vertex
                                    ImU32 vCol = isSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 210, 40, 255);
                                    ImU32 borderCol = isSelected ? IM_COL32(0, 120, 215, 255) : IM_COL32(20, 20, 20, 240);
                                    dl->AddCircleFilled(sPts[i], 3.5f, vCol);
                                    dl->AddCircle(sPts[i], 4.0f, borderCol, 0, 1.2f);
                                }
                            } else if (isEdgeMode && isSelected) {
                                bool isEndpointOfSelectedEdge = false;
                                for (int selE : selectedEdges) {
                                    if (selE >= 0 && selE < 12) {
                                        if (i == edges[selE][0] || i == edges[selE][1]) {
                                            isEndpointOfSelectedEdge = true;
                                            break;
                                        }
                                    }
                                }
                                if (isEndpointOfSelectedEdge) {
                                    dl->AddCircleFilled(sPts[i], 4.5f, IM_COL32(0, 220, 255, 255));
                                    dl->AddCircle(sPts[i], 5.5f, IM_COL32(255, 255, 255, 255), 0, 1.2f);
                                }
                            }
                        }
                    }
                }
            }
            for (auto& child : inst->getChildren()) drawDeformOverlay(child);
        };
        drawDeformOverlay(DataModel::instance());

        // Handle Click Selection & Multi-Selection (Shift / Ctrl + Click)
        if (canPick && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (isVertexMode) {
                if (hoveredVertex != -1) {
                    if (shiftHeld) {
                        auto it = std::find(selectedVertices.begin(), selectedVertices.end(), hoveredVertex);
                        if (it != selectedVertices.end()) selectedVertices.erase(it);
                        else selectedVertices.push_back(hoveredVertex);
                    } else {
                        selectedVertices = { hoveredVertex };
                    }
                } else if (!shiftHeld && !ImGuizmo::IsOver()) {
                    isBoxSelecting = true;
                    boxSelectStart = mousePos;
                }
            } else if (isEdgeMode) {
                if (hoveredEdge != -1) {
                    if (shiftHeld) {
                        auto it = std::find(selectedEdges.begin(), selectedEdges.end(), hoveredEdge);
                        if (it != selectedEdges.end()) selectedEdges.erase(it);
                        else selectedEdges.push_back(hoveredEdge);
                    } else {
                        selectedEdges = { hoveredEdge };
                    }
                } else if (!shiftHeld && !ImGuizmo::IsOver()) {
                    isBoxSelecting = true;
                    boxSelectStart = mousePos;
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
                        selectedVertices.clear();
                        selectedEdges.clear();
                    }
                    if (auto part = std::dynamic_pointer_cast<Part>(sel)) {
                        Engine::Math::Vector3 pos = part->getPosition();
                        ImVec2 sPts[8];
                        bool visible[8];
                        for (int i = 0; i < 8; ++i) {
                            visible[i] = project(pos + part->getVertex(i), sPts[i]);
                        }

                        if (isVertexMode) {
                            for (int i = 0; i < 8; ++i) {
                                if (visible[i] && sPts[i].x >= bMin.x && sPts[i].x <= bMax.x &&
                                    sPts[i].y >= bMin.y && sPts[i].y <= bMax.y) {
                                    if (!contains(selectedVertices, i)) selectedVertices.push_back(i);
                                }
                            }
                        } else if (isEdgeMode) {
                            for (int e = 0; e < 12; ++e) {
                                int i0 = edges[e][0], i1 = edges[e][1];
                                if (visible[i0] && visible[i1]) {
                                    ImVec2 mid((sPts[i0].x + sPts[i1].x) * 0.5f, (sPts[i0].y + sPts[i1].y) * 0.5f);
                                    if ((sPts[i0].x >= bMin.x && sPts[i0].x <= bMax.x && sPts[i0].y >= bMin.y && sPts[i0].y <= bMax.y) ||
                                        (sPts[i1].x >= bMin.x && sPts[i1].x <= bMax.x && sPts[i1].y >= bMin.y && sPts[i1].y <= bMax.y) ||
                                        (mid.x >= bMin.x && mid.x <= bMax.x && mid.y >= bMin.y && mid.y <= bMax.y)) {
                                        if (!contains(selectedEdges, e)) selectedEdges.push_back(e);
                                    }
                                }
                            }
                        }
                    }
                } else if (!shiftHeld) {
                    selectedVertices.clear();
                    selectedEdges.clear();
                }
                isBoxSelecting = false;
            }
        }
    }

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

    ImGui::End();
    ImGui::PopStyleVar();
}

#include "Engine/Core/Math/Quaternion.h"
#include <map>
#include <vector>

void ViewportPanel::handleGizmoInput(Engine::Renderer::Camera& camera) {
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
        selectedVertices.clear();
        selectedEdges.clear();
        return;
    }

    // ── Vertex Mode Multi-Gizmo ──────────────────────────────────────────────
    if (EditorLayout::instance().shadingMode == EditorShadingMode::Vertex) {
        if (selectedParts.size() == 1 && !selectedVertices.empty()) {
            auto part = selectedParts[0];

            // Calculate Centroid of all selected vertices
            Engine::Math::Vector3 centroidLocal(0, 0, 0);
            for (int vIdx : selectedVertices) {
                centroidLocal += part->getVertex(vIdx);
            }
            centroidLocal = centroidLocal * (1.0f / (float)selectedVertices.size());
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

            static std::map<int, Engine::Math::Vector3> s_vertDragStartPositions;
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
                    for (int vIdx : selectedVertices) {
                        s_vertDragStartPositions[vIdx] = part->getVertex(vIdx);
                    }
                }
                Engine::Math::Vector3 newWorldCenter(vTransform.m[12], vTransform.m[13], vTransform.m[14]);
                Engine::Math::Vector3 delta = newWorldCenter - s_vertDragStartCenter;
                for (int vIdx : selectedVertices) {
                    part->setVertex(vIdx, s_vertDragStartPositions[vIdx] + delta);
                }
            } else {
                isDraggingGizmo = false;
            }
        }
        return; // In Vertex mode, do not draw whole-object gizmo
    }

    // ── Edge Mode Multi-Gizmo ────────────────────────────────────────────────
    if (EditorLayout::instance().shadingMode == EditorShadingMode::Edge) {
        static const int edges[12][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Front quad
            {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Back quad
            {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Connecting edges
        };

        if (selectedParts.size() == 1 && !selectedEdges.empty()) {
            auto part = selectedParts[0];

            // Collect all unique vertex indices from selected edges
            std::set<int> uniqueVerts;
            for (int eIdx : selectedEdges) {
                if (eIdx >= 0 && eIdx < 12) {
                    uniqueVerts.insert(edges[eIdx][0]);
                    uniqueVerts.insert(edges[eIdx][1]);
                }
            }

            if (!uniqueVerts.empty()) {
                Engine::Math::Vector3 centroidLocal(0, 0, 0);
                for (int vIdx : uniqueVerts) {
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

                static std::map<int, Engine::Math::Vector3> s_edgeDragStartPositions;
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
                        for (int vIdx : uniqueVerts) {
                            s_edgeDragStartPositions[vIdx] = part->getVertex(vIdx);
                        }
                    }
                    Engine::Math::Vector3 newWorldCenter(vTransform.m[12], vTransform.m[13], vTransform.m[14]);
                    Engine::Math::Vector3 delta = newWorldCenter - s_edgeDragStartCenter;
                    for (int vIdx : uniqueVerts) {
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
    if (canMoveKeys) {
        Engine::Math::Vector3 screenRight = camera.forward.cross(camera.up);
        screenRight.normalize();

        float speed = 18.0f * ImGui::GetIO().DeltaTime;
        if (ImGui::GetIO().KeyShift) speed *= 3.0f;
        if (ImGui::GetIO().KeyAlt) speed *= 0.3f;

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
