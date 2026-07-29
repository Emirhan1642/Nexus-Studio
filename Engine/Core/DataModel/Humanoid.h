#pragma once

#include "Instance.h"
#include "Part.h"
#include "HumanoidStateMachine.h"
#include "../Signal.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Constraints/Constraint.h>
#include <Jolt/Physics/Body/BodyID.h>

#include "../../Animation/AnimationPlayer.h"
#include "../../Animation/Skeleton.h"

namespace JPH {
    class CharacterVirtual;
}

namespace Engine {

class Humanoid : public Instance {
public:
    Humanoid();
    virtual ~Humanoid() override;

    // Instance overrides
    virtual std::string getClassName() const override { return "Humanoid"; }

    // Properties
    float walkSpeed = 16.0f;
    float jumpPower = 50.0f;
    float health = 100.0f;
    float maxHealth = 100.0f;

    HumanoidState getState() const { return state; }
    void changeState(HumanoidState newState);

    // Methods
    void moveTo(const Math::Vector3& direction);
    void jump();
    void enterRagdoll();
    void exitRagdoll();

    // Signal for state changes
    Signal stateChangedSignal;

    // Getters for internal mechanics
    Math::Vector3 getCurrentMoveDirection() const { return currentMoveDirection; }
    std::shared_ptr<Part> getRootPart() const { return rootPart.lock(); }
    JPH::CharacterVirtual* getCharacter() const { return character.GetPtr(); }
    
    // Animation API
    Animation::AnimationPlayer& getAnimationPlayer() { return animationPlayer; }
    Animation::Skeleton& getSkeleton() { return skeleton; }
    bool ikEnabled = true;

    // Internal updates
    void update(float deltaTime);
    void applyMovement(const Math::Vector3& moveDirection, float deltaTime);

    virtual void onAddedToWorkspace() override;
    virtual void onRemovedFromWorkspace() override;

private:
    void initCharacterVirtual();
    void cleanupCharacterVirtual();

    Engine::Math::Vector3 currentMoveDirection = {0.0f, 0.0f, 0.0f};
    bool jumpRequested = false;
    HumanoidState state = HumanoidState::Idle;

    friend class HumanoidStateMachine;

    JPH::Ref<JPH::CharacterVirtual> character;
    uint32_t prePhysicsConnectionId = 0;
    std::weak_ptr<Part> rootPart;
    HumanoidStateMachine stateMachine;

    // Ragdoll tracking
    struct RagdollLimb {
        std::shared_ptr<Part> part;
        JPH::BodyID bodyId;
    };
    std::vector<RagdollLimb> ragdollLimbs;
    std::vector<JPH::Constraint*> ragdollJoints;
    
    // Skeletal Animation
    Animation::AnimationPlayer animationPlayer;
    Animation::Skeleton skeleton;
};

} // namespace Engine
