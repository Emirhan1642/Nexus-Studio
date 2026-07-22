#pragma once
#include "Vector3.h"
#include <array>

namespace Engine::Math {

struct Matrix4 {
    std::array<float, 16> m;

    Matrix4();

    // Identity matrix
    static Matrix4 identity();

    // Create a translation matrix
    static Matrix4 translation(const Vector3& t);

    // Create a scaling matrix
    static Matrix4 scale(const Vector3& s);

    // Look at matrix for camera
    static Matrix4 lookAt(const Vector3& eye, const Vector3& at, const Vector3& up);

    // Perspective projection matrix (Right handed, Z from 0 to 1 for bgfx/d3d, or -1 to 1 for GL - bgfx handles differences if we use specific projection or we just provide standard ones. We will provide a standard D3D style or let bgfx handle it. Let's provide a standard perspective projection)
    static Matrix4 perspective(float fovDegrees, float aspect, float nearPlane, float farPlane);
    
    // Orthographic projection matrix
    static Matrix4 orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane);

    // Create from position and scale (for simple transform)
    static Matrix4 fromPositionAndSize(const Vector3& pos, const Vector3& size);

    Vector3 getTranslation() const;

    // Multiply two matrices
    Matrix4 operator*(const Matrix4& rhs) const;
};

} // namespace Engine::Math
