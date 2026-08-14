#pragma once
#include <bgfx/bgfx.h>
#include "Engine/Renderer/Camera.h"
#include <memory>

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
};
