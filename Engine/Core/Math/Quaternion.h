#pragma once

#include <cmath>

namespace Engine {
namespace Math {

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    Quaternion() = default;
    Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    static Quaternion identity() { return Quaternion(0.0f, 0.0f, 0.0f, 1.0f); }

    float length() const {
        return std::sqrt(x * x + y * y + z * z + w * w);
    }

    void normalize() {
        float len = length();
        if (len > 0.0f) {
            float invLen = 1.0f / len;
            x *= invLen;
            y *= invLen;
            z *= invLen;
            w *= invLen;
        }
    }
    
    float dot(const Quaternion& q) const {
        return x * q.x + y * q.y + z * q.z + w * q.w;
    }

    Quaternion slerp(const Quaternion& target, float t) const;
};

} // namespace Math
} // namespace Engine
