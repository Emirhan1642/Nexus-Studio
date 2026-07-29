#pragma once
#include <cmath>

namespace Engine::Math {

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3() = default;
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vector3 operator*(float scalar) const {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }
    
    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }
    
    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }
    
    float length() const {
        return std::sqrt(x * x + y * y + z * z);
    }
    
    void normalize() {
        float len = length();
        if (len > 0.0f) {
            float invLen = 1.0f / len;
            x *= invLen;
            y *= invLen;
            z *= invLen;
        }
    }

    float dot(const Vector3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    Vector3 cross(const Vector3& other) const {
        return Vector3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }
};

} // namespace Engine::Math
