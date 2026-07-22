#pragma once
#include <cstdint>

namespace Engine::Physics {

using PhysicsBodyHandle = uint32_t;
constexpr PhysicsBodyHandle InvalidPhysicsHandle = 0xFFFFFFFF;

} // namespace Engine::Physics
