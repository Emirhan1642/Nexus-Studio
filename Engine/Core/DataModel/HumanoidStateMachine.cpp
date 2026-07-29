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
        // If we are in ragdoll state but character is valid, it means we exited ragdoll manually
        if (humanoid.getCharacter() != nullptr) {
            return HumanoidState::Idle; 
        }
        return HumanoidState::Ragdoll; 
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

static std::shared_ptr<AnimationTrack> getTrackForState(HumanoidState state, Humanoid& humanoid) {
    switch (state) {
        case HumanoidState::Idle: return humanoid.getIdleTrack();
        case HumanoidState::Walking: return humanoid.getWalkTrack();
        case HumanoidState::Jumping: return humanoid.getJumpTrack();
        case HumanoidState::Falling: return humanoid.getFallTrack();
        default: return nullptr;
    }
}

void HumanoidStateMachine::onStateEnter(HumanoidState state, Humanoid& humanoid) {
    if (auto track = getTrackForState(state, humanoid)) {
        track->play(0.2f); // 0.2s crossfade
    }
    humanoid.stateChangedSignal.fire({ (int)state });
}

void HumanoidStateMachine::onStateExit(HumanoidState state, Humanoid& humanoid) {
    if (auto track = getTrackForState(state, humanoid)) {
        track->stop(0.2f); // 0.2s fade out
    }
}

} // namespace Engine
