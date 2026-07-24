#include "HumanoidStateMachine.h"
#include "Humanoid.h"
#include <Jolt/Physics/Character/CharacterVirtual.h>

namespace Engine {

void HumanoidStateMachine::update(Humanoid& humanoid, float deltaTime) {
    HumanoidState newState = computeNextState(humanoid);

    if (newState != humanoid.state) {
        onStateExit(humanoid.state, humanoid);
        onStateEnter(newState, humanoid);
        humanoid.state = newState;
    }
}

HumanoidState HumanoidStateMachine::computeNextState(const Humanoid& humanoid) {
    if (humanoid.state == HumanoidState::Ragdoll) {
        return HumanoidState::Ragdoll; // Cannot escape ragdoll in this basic implementation
    }

    JPH::CharacterVirtual* character = humanoid.getCharacter();
    if (!character) return HumanoidState::Idle;

    JPH::CharacterVirtual::EGroundState groundState = character->GetGroundState();

    if (groundState == JPH::CharacterVirtual::EGroundState::InAir) {
        return character->GetLinearVelocity().GetY() > 0.0f
            ? HumanoidState::Jumping
            : HumanoidState::Falling;
    }

    if (humanoid.state == HumanoidState::Jumping || humanoid.state == HumanoidState::Falling) {
        return HumanoidState::Landed; // One frame transition state
    }

    bool hasMoveInput = humanoid.getCurrentMoveDirection().length() > 0.01f;
    return hasMoveInput ? HumanoidState::Walking : HumanoidState::Idle;
}

void HumanoidStateMachine::onStateEnter(HumanoidState state, Humanoid& humanoid) {
    humanoid.stateChangedSignal.fire({ (int)state });
}

void HumanoidStateMachine::onStateExit(HumanoidState state, Humanoid& humanoid) {
    // Optional: Cleanup state specific data
}

} // namespace Engine
