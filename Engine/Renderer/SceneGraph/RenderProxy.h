#pragma once
#include <cstdint>
#include "../../Core/Math/Matrix4.h"

#include "../Materials/Material.h"

namespace Engine::Renderer {

using MeshHandle = uint32_t;

constexpr uint32_t InvalidHandle = 0xFFFFFFFF;

struct RenderProxy {
    Engine::Math::Matrix4 worldTransform;
    MeshHandle mesh = InvalidHandle;
    MaterialData material;
    bool visible = true;
    bool castsShadow = true;

    // Culling için önceden hesaplanmış bounding sphere
    Engine::Math::Vector3 boundsCenter;
    float boundsRadius = 0.0f;
};

} // namespace Engine::Renderer
