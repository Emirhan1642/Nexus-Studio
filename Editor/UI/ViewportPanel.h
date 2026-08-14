#pragma once
#include <bgfx/bgfx.h>
#include <imgui.h>
#include <vector>
#include <memory>
#include "Engine/Renderer/Camera.h"

class ViewportPanel {
public:
    void resize(uint16_t width, uint16_t height);
    void draw(Engine::Renderer::Camera& camera);

private:
    void handleCameraControls(Engine::Renderer::Camera& camera, bool isHovered);
    void handleGizmoInput(Engine::Renderer::Camera& camera);

    bgfx::TextureHandle colorTexture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle depthTexture = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle frameBuffer = BGFX_INVALID_HANDLE;
    uint16_t currentWidth = 0, currentHeight = 0;

    bool isDraggingGizmo = false;
    Engine::Math::Vector3 dragStartPosition;
    std::vector<int> selectedVertices;
    std::vector<int> selectedEdges;
    std::vector<int> selectedFaces;
    bool isBoxSelecting = false;
    ImVec2 boxSelectStart{0, 0};

    // Pie Menu states
    bool showSnapPieMenu = false;
    bool showModesPieMenu = false;
    ImVec2 pieMenuCenter{0, 0};
};
