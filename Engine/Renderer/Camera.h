#pragma once
#include "../Core/Math/Vector3.h"
#include "../Core/Math/Matrix4.h"

namespace Engine::Renderer {

class Camera {
public:
    Engine::Math::Vector3 position{0.0f, 5.0f, -10.0f};
    Engine::Math::Vector3 forward{0.0f, 0.0f, 1.0f};
    Engine::Math::Vector3 up{0.0f, 1.0f, 0.0f};
    float fovDegrees = 70.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    Engine::Math::Matrix4 getViewMatrix() const {
        Engine::Math::Vector3 target = { position.x + forward.x, position.y + forward.y, position.z + forward.z };
        return Engine::Math::Matrix4::lookAt(position, target, up);
    }

    Engine::Math::Matrix4 getProjectionMatrix(float aspectRatio) const {
        return Engine::Math::Matrix4::perspective(fovDegrees, aspectRatio, nearPlane, farPlane);
    }
};

} // namespace Engine::Renderer
