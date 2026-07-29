#include "Quaternion.h"
#include "Matrix4.h"
#include <algorithm>

namespace Engine {
namespace Math {

Quaternion Quaternion::fromRotationMatrix(const Matrix4& m) {
    float tr = m.m[0] + m.m[5] + m.m[10];
    Quaternion q;
    if (tr > 0.0f) {
        float S = std::sqrt(tr + 1.0f) * 2.0f; 
        q.w = 0.25f * S;
        q.x = (m.m[6] - m.m[9]) / S;
        q.y = (m.m[8] - m.m[2]) / S; 
        q.z = (m.m[1] - m.m[4]) / S; 
    } else if ((m.m[0] > m.m[5]) && (m.m[0] > m.m[10])) { 
        float S = std::sqrt(1.0f + m.m[0] - m.m[5] - m.m[10]) * 2.0f; 
        q.w = (m.m[6] - m.m[9]) / S;
        q.x = 0.25f * S;
        q.y = (m.m[1] + m.m[4]) / S; 
        q.z = (m.m[8] + m.m[2]) / S; 
    } else if (m.m[5] > m.m[10]) { 
        float S = std::sqrt(1.0f + m.m[5] - m.m[0] - m.m[10]) * 2.0f; 
        q.w = (m.m[8] - m.m[2]) / S;
        q.x = (m.m[1] + m.m[4]) / S; 
        q.y = 0.25f * S;
        q.z = (m.m[6] + m.m[9]) / S; 
    } else { 
        float S = std::sqrt(1.0f + m.m[10] - m.m[0] - m.m[5]) * 2.0f; 
        q.w = (m.m[1] - m.m[4]) / S;
        q.x = (m.m[8] + m.m[2]) / S;
        q.y = (m.m[6] + m.m[9]) / S;
        q.z = 0.25f * S;
    }
    q.normalize();
    return q;
}

Quaternion Quaternion::fromAxisAngle(const Vector3& axis, float angle) {
    float halfAngle = angle * 0.5f;
    float s = std::sin(halfAngle);
    Quaternion q;
    q.x = axis.x * s;
    q.y = axis.y * s;
    q.z = axis.z * s;
    q.w = std::cos(halfAngle);
    q.normalize();
    return q;
}

Quaternion Quaternion::slerp(const Quaternion& target, float t) const {
    float cosTheta = dot(target);
    Quaternion tq = target;
    
    if (cosTheta < 0.0f) {
        tq.x = -tq.x;
        tq.y = -tq.y;
        tq.z = -tq.z;
        tq.w = -tq.w;
        cosTheta = -cosTheta;
    }
    
    if (cosTheta > 0.9995f) {
        // Linear interpolation for small angles
        Quaternion result(
            x + t * (tq.x - x),
            y + t * (tq.y - y),
            z + t * (tq.z - z),
            w + t * (tq.w - w)
        );
        result.normalize();
        return result;
    }
    
    float theta = std::acos(std::clamp(cosTheta, -1.0f, 1.0f));
    float sinTheta = std::sin(theta);
    
    float w1 = std::sin((1.0f - t) * theta) / sinTheta;
    float w2 = std::sin(t * theta) / sinTheta;
    
    return Quaternion(
        x * w1 + tq.x * w2,
        y * w1 + tq.y * w2,
        z * w1 + tq.z * w2,
        w * w1 + tq.w * w2
    );
}

Quaternion Quaternion::operator*(const Quaternion& q) const {
    return Quaternion(
        w * q.x + x * q.w + y * q.z - z * q.y,
        w * q.y - x * q.z + y * q.w + z * q.x,
        w * q.z + x * q.y - y * q.x + z * q.w,
        w * q.w - x * q.x - y * q.y - z * q.z
    );
}

} // namespace Math
} // namespace Engine
