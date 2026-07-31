#pragma once
#include <string>
#include <vector>
#include "../Core/Math/Vector3.h"

namespace Engine {
namespace Physics {

struct PhysicsBoneShape {
    std::string boneName;
    float radius = 0.5f;
    float halfHeight = 1.0f;
    Math::Vector3 localOffset = {0, 0, 0};
};

class PhysicsAsset {
public:
    std::vector<PhysicsBoneShape> shapes;

    const PhysicsBoneShape* findShape(const std::string& boneName) const {
        for (const auto& shape : shapes) {
            if (shape.boneName == boneName) {
                return &shape;
            }
        }
        return nullptr;
    }
};

} // namespace Physics
} // namespace Engine
