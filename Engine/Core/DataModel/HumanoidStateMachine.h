#pragma once

#include "../Math/Vector3.h"

namespace Engine {

enum class HumanoidState {
    Idle,
    Walking,
    Jumping,
    Falling,
    Landed,
    Climbing,
    Ragdoll
};

class Humanoid;

class HumanoidStateMachine {
public:
    void update(Humanoid& humanoid, float deltaTime);

private:
    HumanoidState computeNextState(const Humanoid& humanoid);
    void onStateEnter(HumanoidState state, Humanoid& humanoid);
    void onStateExit(HumanoidState state, Humanoid& humanoid);
};

} // namespace Engine
