#include "Quaternion.h"
#include <algorithm>

namespace Engine {
namespace Math {

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

} // namespace Math
} // namespace Engine
