#pragma once
#include <bgfx/bgfx.h>
#include "../../Core/Math/Vector3.h"

namespace Engine::Renderer {

struct MaterialData {
    Engine::Math::Vector3 albedo{1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float emissiveStrength = 0.0f;
    
    bgfx::TextureHandle albedoTexture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle normalTexture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle metallicTexture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle roughnessTexture = BGFX_INVALID_HANDLE;
    
    bgfx::ProgramHandle customShader = BGFX_INVALID_HANDLE;
};

} // namespace Engine::Renderer
