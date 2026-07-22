#define IMGUI_DEFINE_MATH_OPERATORS
#include "ViewportPanel.h"
#include <imgui/imgui.h>
#include <dear-imgui/imgui_internal.h>
#include <widgets/gizmo.h>
#include "SelectionManager.h"
#include "../Undo/UndoStack.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Core/DataModel/Part.h"

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
        // We MUST use the overloaded ImGui::Image that takes bgfx::TextureHandle provided by example-common's imgui.h
        ImGui::Image(colorTexture, availSize);

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
