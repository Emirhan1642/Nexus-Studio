#define IMGUI_DEFINE_MATH_OPERATORS
#include "ViewportPanel.h"
#include <imgui/imgui.h>
#include <dear-imgui/imgui_internal.h>
#include <widgets/gizmo.h>
#include "SelectionManager.h"
#include "../Undo/UndoStack.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Core/DataModel/DataModel.h"
#include "Engine/Core/DataModel/Constraint.h"
#include "Engine/Assets/AssetDatabase.h"
#include <functional>

void ViewportPanel::resize(uint16_t width, uint16_t height) {
    if (width == currentWidth && height == currentHeight) return;
    if (width == 0 || height == 0) return;

    if (bgfx::isValid(frameBuffer)) {
        // FrameBuffer created with destroyTextures=true will automatically destroy attached textures!
        bgfx::destroy(frameBuffer);
        frameBuffer = BGFX_INVALID_HANDLE;
        colorTexture = BGFX_INVALID_HANDLE;
        depthTexture = BGFX_INVALID_HANDLE;
    }

    colorTexture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT);
    depthTexture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT);

    bgfx::TextureHandle attachments[] = { colorTexture, depthTexture };
    frameBuffer = bgfx::createFrameBuffer(2, attachments, true);

    currentWidth = width;
    currentHeight = height;
}

void ViewportPanel::draw(Engine::Renderer::Camera& camera) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 workPos = viewport->WorkPos;
    ImVec2 workSize = viewport->WorkSize;

    ImGui::SetNextWindowPos(workPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(workSize.x * 0.75f, workSize.y), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImVec2 availSize = ImGui::GetContentRegionAvail();
    if (availSize.x > 0 && availSize.y > 0) {
        resize((uint16_t)availSize.x, (uint16_t)availSize.y);

        // Render the scene to the frame buffer
        Engine::Renderer::RendererSystem::instance().renderFrame(camera, currentWidth, currentHeight, frameBuffer);

        // Display the frame buffer in ImGui
        ImVec2 screenPos = ImGui::GetCursorScreenPos();
        ImGui::Image(colorTexture, availSize);

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_GUID")) {
                Engine::Assets::AssetGuid droppedGuid = *(Engine::Assets::AssetGuid*)payload->Data;
                
                auto part = std::make_shared<Part>();
                part->name = "ImportedAsset";
                part->setMeshFromAsset(droppedGuid);
                
                // Place it slightly in front of the camera
                part->setPosition(camera.position + camera.forward * 5.0f);
                
                part->setParent(DataModel::instance());
                UndoStack::instance().pushCreateCommand(part, DataModel::instance());
            }
            ImGui::EndDragDropTarget();
        }

        static bool showConstraints = true;

        if (showConstraints) {
            auto drawList = ImGui::GetWindowDrawList();
            Engine::Math::Matrix4 view = camera.getViewMatrix();
            Engine::Math::Matrix4 proj = camera.getProjectionMatrix((float)currentWidth / (float)currentHeight);
            Engine::Math::Matrix4 viewProj = proj * view;

            auto projectToScreen = [&](const Engine::Math::Vector3& pos, ImVec2& outScreen) -> bool {
                float x = pos.x * viewProj.m[0] + pos.y * viewProj.m[4] + pos.z * viewProj.m[8] + viewProj.m[12];
                float y = pos.x * viewProj.m[1] + pos.y * viewProj.m[5] + pos.z * viewProj.m[9] + viewProj.m[13];
                float w = pos.x * viewProj.m[3] + pos.y * viewProj.m[7] + pos.z * viewProj.m[11] + viewProj.m[15];

                if (w < 0.001f) return false;

                x /= w;
                y /= w;

                // Simple NDC to Screen (bgfx OpenGL style / standard)
                outScreen.x = screenPos.x + (x * 0.5f + 0.5f) * availSize.x;
                outScreen.y = screenPos.y + (1.0f - (y * 0.5f + 0.5f)) * availSize.y;
                return true;
            };

            std::function<void(const std::shared_ptr<Instance>&)> drawConstraintsRecursive = [&](const std::shared_ptr<Instance>& inst) {
                if (auto constraint = std::dynamic_pointer_cast<Constraint>(inst)) {
                    if (constraint->getVisible() && constraint->getEnabled()) {
                        auto p0 = std::dynamic_pointer_cast<Part>(constraint->getPart0());
                        auto p1 = std::dynamic_pointer_cast<Part>(constraint->getPart1());
                        if (p0 && p1) {
                            ImVec2 s0, s1;
                            if (projectToScreen(p0->getPosition(), s0) && projectToScreen(p1->getPosition(), s1)) {
                                drawList->AddLine(s0, s1, IM_COL32(0, 255, 0, 255), 3.0f);
                                drawList->AddCircleFilled(s0, 5.0f, IM_COL32(255, 255, 0, 255));
                                drawList->AddCircleFilled(s1, 5.0f, IM_COL32(255, 255, 0, 255));
                            }
                        }
                    }
                }
                for (auto& child : inst->getChildren()) {
                    drawConstraintsRecursive(child);
                }
            };
            drawConstraintsRecursive(DataModel::instance());
        }

        // Checkbox at the top left
        ImGui::SetCursorPos(ImVec2(10, 10));
        ImGui::Checkbox("Show Constraints", &showConstraints);

        handleGizmoInput(camera);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void ViewportPanel::handleGizmoInput(Engine::Renderer::Camera& camera) {
    auto selected = SelectionManager::instance().getSelected();
    auto part = std::dynamic_pointer_cast<Part>(selected);
    if (!part) return;

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

    Engine::Math::Matrix4 transform = Engine::Math::Matrix4::fromPositionAndSize(part->getPosition(), part->getSize());
    Engine::Math::Matrix4 view = camera.getViewMatrix();
    Engine::Math::Matrix4 proj = camera.getProjectionMatrix((float)currentWidth / (float)currentHeight);

    static ImGuizmo::OPERATION currentOp = ImGuizmo::TRANSLATE;
    
    // Shortcuts for Gizmo
    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) currentOp = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) currentOp = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) currentOp = ImGuizmo::SCALE;
    }

    bool manipulated = ImGuizmo::Manipulate(view.m.data(), proj.m.data(), currentOp, ImGuizmo::WORLD, transform.m.data());

    if (ImGuizmo::IsUsing()) {
        if (!isDraggingGizmo) {
            isDraggingGizmo = true;
            dragStartPosition = part->getPosition();
        }
        Engine::Math::Vector3 newPos = transform.getTranslation();
        part->setPosition(newPos);
    } else {
        if (isDraggingGizmo) {
            UndoStack::instance().pushPropertyChangeCommand(part, "Position", dragStartPosition, part->getPosition());
            isDraggingGizmo = false;
        }
    }
}
