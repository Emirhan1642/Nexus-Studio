#pragma once
#include "../Core/Math/Vector3.h"
#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>

namespace Engine::Physics {

constexpr float STUDS_PER_METER = 3.57f;

inline JPH::Vec3 toJoltVec3(const Engine::Math::Vector3& studs) {
    return JPH::Vec3(studs.x / STUDS_PER_METER, studs.y / STUDS_PER_METER, studs.z / STUDS_PER_METER);
}

inline Engine::Math::Vector3 fromJoltVec3(const JPH::RVec3& meters) {
    return Engine::Math::Vector3(meters.GetX() * STUDS_PER_METER, meters.GetY() * STUDS_PER_METER, meters.GetZ() * STUDS_PER_METER);
}

} // namespace Engine::Physics
