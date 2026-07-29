#include "Humanoid.h"
#include "../../Physics/PhysicsWorld.h"
#include "../../Physics/PhysicsConversions.h"
#include "../Reflection/ClassBuilder.h"
#include "../Reflection/EnumRegistry.h"

#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include "../../Animation/IK/TwoBoneIK.h"

using namespace Engine;

Humanoid::Humanoid() {
}

Humanoid::~Humanoid() {
    cleanupCharacterVirtual();
}

void Humanoid::onAddedToWorkspace() {
    Instance::onAddedToWorkspace();

    // If parent is a Part, treat it as our RootPart
    if (auto parentPart = dynamic_cast<Part*>(this->getParent().get())) {
        rootPart = std::static_pointer_cast<Part>(parentPart->shared_from_this());
        initCharacterVirtual();
    }
}

void Humanoid::onRemovedFromWorkspace() {
    Instance::onRemovedFromWorkspace();
    rootPart.reset();
    cleanupCharacterVirtual();
}

std::shared_ptr<AnimationTrack> Humanoid::getIdleTrack() {
    auto track = std::dynamic_pointer_cast<AnimationTrack>(findFirstChild("IdleTrack"));
    if (!track) {
        track = std::static_pointer_cast<AnimationTrack>(createInstance("AnimationTrack"));
        track->name = "IdleTrack";
        track->priority = 100; // base priority
        track->setParent(shared_from_this());
    }
    track->clip = idleAnimation;
    return track;
}

std::shared_ptr<AnimationTrack> Humanoid::getWalkTrack() {
    auto track = std::dynamic_pointer_cast<AnimationTrack>(findFirstChild("WalkTrack"));
    if (!track) {
        track = std::static_pointer_cast<AnimationTrack>(createInstance("AnimationTrack"));
        track->name = "WalkTrack";
        track->priority = 110;
        track->setParent(shared_from_this());
    }
    track->clip = walkAnimation;
    return track;
}

std::shared_ptr<AnimationTrack> Humanoid::getJumpTrack() {
    auto track = std::dynamic_pointer_cast<AnimationTrack>(findFirstChild("JumpTrack"));
    if (!track) {
        track = std::static_pointer_cast<AnimationTrack>(createInstance("AnimationTrack"));
        track->name = "JumpTrack";
        track->priority = 120;
        track->setParent(shared_from_this());
    }
    track->clip = jumpAnimation;
    return track;
}

std::shared_ptr<AnimationTrack> Humanoid::getFallTrack() {
    auto track = std::dynamic_pointer_cast<AnimationTrack>(findFirstChild("FallTrack"));
    if (!track) {
        track = std::static_pointer_cast<AnimationTrack>(createInstance("AnimationTrack"));
        track->name = "FallTrack";
        track->priority = 120;
        track->setParent(shared_from_this());
    }
    track->clip = fallAnimation;
    return track;
}

void Humanoid::initCharacterVirtual() {
    if (character) return;
    auto part = getRootPart();
    if (!part) return;

    auto& physicsWorld = Physics::PhysicsWorld::instance();

    // Create character shape using the part's size
    // Convert size to meters first!
    JPH::Vec3 joltSize = Physics::toJoltVec3(part->getSize());
    float radius = joltSize.GetX() * 0.5f;
    float halfHeight = (joltSize.GetY() * 0.5f) - radius;
    if (halfHeight < 0.05f) halfHeight = 0.05f; // Ensure minimum height

    JPH::RefConst<JPH::Shape> standingShape = new JPH::CapsuleShape(halfHeight, radius); 

    JPH::CharacterVirtualSettings settings;
    settings.mShape = standingShape;
    settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -radius); // Ground check plane
    settings.mUp = JPH::Vec3::sAxisY();

    character = new JPH::CharacterVirtual(&settings, Physics::toJoltVec3(part->getPosition()), JPH::Quat::sIdentity(), &physicsWorld.getPhysicsSystem());

    if (part->getPhysicsBodyId() != 0xFFFFFFFF) {
        physicsWorld.getBodyInterface().SetMotionType(JPH::BodyID(part->getPhysicsBodyId()), JPH::EMotionType::Kinematic, JPH::EActivation::Activate);
    }

    prePhysicsConnectionId = physicsWorld.prePhysicsUpdate.connect([this](const std::vector<std::any>& args) {
        if (args.size() > 0) {
            float dt = std::any_cast<float>(args[0]);
            this->update(dt);
        }
    });
}

void Humanoid::cleanupCharacterVirtual() {
    if (prePhysicsConnectionId != 0) {
        Physics::PhysicsWorld::instance().prePhysicsUpdate.disconnect(prePhysicsConnectionId);
        prePhysicsConnectionId = 0;
    }
    if (character) {
        character = nullptr;
    }
}

void Humanoid::moveTo(const Engine::Math::Vector3& direction) {
    currentMoveDirection = direction;
    float len = currentMoveDirection.length();
    if (len > 1.0f) {
        currentMoveDirection = currentMoveDirection * (1.0f / len);
    }
}

void Humanoid::jump() {
    jumpRequested = true;
}

void Humanoid::update(float deltaTime) {
    if (state == HumanoidState::Ragdoll || health <= 0) {
        if (state != HumanoidState::Ragdoll) enterRagdoll();
        return;
    }

    applyMovement(currentMoveDirection, deltaTime);
    
    // Jump handling
    if (jumpRequested && character && character->IsSupported()) {
        JPH::Vec3 currentVel = character->GetLinearVelocity();
        currentVel.SetY(jumpPower * 0.1f); // scaled down for Jolt units
        character->SetLinearVelocity(currentVel);
    }
    jumpRequested = false;

    stateMachine.update(*this, deltaTime);
}

void Humanoid::applyMovement(const Math::Vector3& moveDirection, float deltaTime) {
    if (!character) return;
    auto part = getRootPart();
    if (!part) return;

    auto& physicsWorld = Physics::PhysicsWorld::instance();

    // Apply gravity
    JPH::Vec3 linearVelocity = character->GetLinearVelocity();
    linearVelocity += JPH::Vec3(0, -9.81f, 0) * deltaTime;

    // Calculate desired velocity (studs to meters)
    JPH::Vec3 desiredVelocity = Physics::toJoltVec3(moveDirection * walkSpeed);
    
    // Overwrite horizontal velocity
    linearVelocity.SetX(desiredVelocity.GetX());
    linearVelocity.SetZ(desiredVelocity.GetZ());

    character->SetLinearVelocity(linearVelocity);

    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
    updateSettings.mStickToFloorStepDown = JPH::Vec3(0, -0.5f, 0);
    updateSettings.mWalkStairsStepUp = JPH::Vec3(0, 0.4f, 0);

    // Setup Jolt default filters using the Character's layer
    JPH::DefaultBroadPhaseLayerFilter broadphaseFilter(physicsWorld.getObjectVsBroadPhaseLayerFilter(), Physics::Layers::CHARACTER);
    JPH::DefaultObjectLayerFilter objectFilter(physicsWorld.getObjectLayerPairFilter(), Physics::Layers::CHARACTER);
    JPH::IgnoreSingleBodyFilter bodyFilter(JPH::BodyID(part->getPhysicsBodyId()));

    // Apply movement
    character->ExtendedUpdate(
        deltaTime,
        JPH::Vec3(0, -9.81f, 0),
        updateSettings,
        broadphaseFilter,
        objectFilter,
        bodyFilter,
        { }, // shape filter (ignored)
        *physicsWorld.getTempAllocator()
    );

    // Sync back to DataModel
    part->setPosition(Physics::fromJoltVec3(character->GetPosition()));
    
    // Evaluate skeletal animation
    if (!skeleton.bones.empty()) {
        std::vector<Math::Matrix4> localPose = animationPlayer.evaluate(skeleton, deltaTime);
        std::vector<Math::Matrix4> worldPose = skeleton.computeWorldTransforms(localPose);
        
        // Simple IK Integration hook
        if (ikEnabled) {
            bool ikApplied = false;
            for (auto& child : getChildren()) {
                if (auto ik = std::dynamic_pointer_cast<IKControl>(child)) {
                    ik->apply(skeleton, localPose, worldPose);
                    ikApplied = true;
                }
            }
            if (ikApplied) {
                // Recompute world pose if local pose changed
                worldPose = skeleton.computeWorldTransforms(localPose);
            }
        }
        
        std::vector<Math::Matrix4> finalBoneTransforms(skeleton.bones.size());
        for (size_t i = 0; i < skeleton.bones.size(); ++i) {
            finalBoneTransforms[i] = worldPose[i] * skeleton.bones[i].inverseBindPoseWorldTransform;
        }

        part->setBoneTransforms(finalBoneTransforms);
    }
}

void Humanoid::enterRagdoll() {
    if (state == HumanoidState::Ragdoll) return;
    
    cleanupCharacterVirtual();
    state = HumanoidState::Ragdoll;
    stateChangedSignal.fire({ (int)state });

    auto part = getRootPart();
    if (!part) return;

    auto& physicsWorld = Physics::PhysicsWorld::instance();
    auto& bodyInterface = physicsWorld.getBodyInterface();

    // In a real game, the limbs would already exist in the DataModel as children.
    // For this demonstration, we will spawn a Head, Torso, and Arms if they aren't there,
    // and attach them with Jolt Hinge/Spherical constraints.
    
    auto createLimb = [&](const std::string& name, const Engine::Math::Vector3& offset, const Engine::Math::Vector3& size) -> std::shared_ptr<Part> {
        auto limb = std::static_pointer_cast<Part>(createInstance("Part"));
        limb->name = name;
        limb->setSize(size);
        limb->setPosition(part->getPosition() + offset);
        limb->setAnchored(false);
        limb->setParent(this->getParent());
        return limb;
    };

    auto head = createLimb("Head", {0, 1.5f, 0}, {1.0f, 1.0f, 1.0f});
    auto torso = createLimb("Torso", {0, 0.0f, 0}, {2.0f, 2.0f, 1.0f});
    auto leftArm = createLimb("LeftArm", {-1.5f, 0.0f, 0}, {1.0f, 2.0f, 1.0f});
    auto rightArm = createLimb("RightArm", {1.5f, 0.0f, 0}, {1.0f, 2.0f, 1.0f});

    auto addLimbToPhysics = [&](std::shared_ptr<Part> limbPart) {
        JPH::BodyCreationSettings settings(
            new JPH::CapsuleShape(limbPart->getSize().y * 0.5f, limbPart->getSize().x * 0.5f), 
            Physics::toJoltVec3(limbPart->getPosition()), 
            JPH::Quat::sIdentity(), 
            JPH::EMotionType::Dynamic, 
            Physics::Layers::RAGDOLL
        );
        RagdollLimb limb;
        limb.part = limbPart;
        limb.bodyId = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate);
        ragdollLimbs.push_back(limb);
        return limb.bodyId;
    };

    JPH::BodyID headId = addLimbToPhysics(head);
    JPH::BodyID torsoId = addLimbToPhysics(torso);
    JPH::BodyID leftArmId = addLimbToPhysics(leftArm);
    JPH::BodyID rightArmId = addLimbToPhysics(rightArm);

    // Create constraints to link them together
    auto createJoint = [&](JPH::BodyID b1, JPH::BodyID b2, JPH::Vec3 pivot) {
        JPH::BodyLockWrite lock1(physicsWorld.getPhysicsSystem().GetBodyLockInterface(), b1);
        JPH::BodyLockWrite lock2(physicsWorld.getPhysicsSystem().GetBodyLockInterface(), b2);
        if (lock1.Succeeded() && lock2.Succeeded()) {
            JPH::PointConstraintSettings settings;
            settings.mPoint1 = pivot;
            settings.mPoint2 = pivot;
            JPH::Constraint* joint = settings.Create(lock1.GetBody(), lock2.GetBody());
            physicsWorld.addConstraint(joint);
            ragdollJoints.push_back(joint);
        }
    };

    createJoint(torsoId, headId, Physics::toJoltVec3(part->getPosition() + Math::Vector3(0, 1.0f, 0))); // Neck
    createJoint(torsoId, leftArmId, Physics::toJoltVec3(part->getPosition() + Math::Vector3(-1.0f, 0.5f, 0))); // Left Shoulder
    createJoint(torsoId, rightArmId, Physics::toJoltVec3(part->getPosition() + Math::Vector3(1.0f, 0.5f, 0))); // Right Shoulder
}

void Humanoid::exitRagdoll() {
    if (state != HumanoidState::Ragdoll) return;

    auto part = getRootPart();
    if (!part) return;

    auto& physicsWorld = Physics::PhysicsWorld::instance();
    auto& bodyInterface = physicsWorld.getBodyInterface();

    // Remove joints
    for (auto joint : ragdollJoints) {
        physicsWorld.getPhysicsSystem().RemoveConstraint(joint);
    }
    ragdollJoints.clear();

    // Remove limbs
    for (auto& limb : ragdollLimbs) {
        bodyInterface.RemoveBody(limb.bodyId);
        bodyInterface.DestroyBody(limb.bodyId);
        if (limb.part) {
            limb.part->setParent(nullptr);
        }
    }
    ragdollLimbs.clear();

    // Reinitialize virtual character
    initCharacterVirtual();

    state = HumanoidState::Idle;
    stateChangedSignal.fire({ (int)state });
}

// Reflection Registration
static void registerHumanoid() {
    Engine::Reflection::ClassBuilder<Humanoid>("Humanoid")
        .base("Instance")
        .property("WalkSpeed", &Humanoid::walkSpeed)
        .property("JumpPower", &Humanoid::jumpPower)
        .property("Health", &Humanoid::health)
        .property("MaxHealth", &Humanoid::maxHealth)
        .method("MoveTo", &Humanoid::moveTo)
        .method("Jump", &Humanoid::jump)
        .method("EnterRagdoll", &Humanoid::enterRagdoll)
        .method("ExitRagdoll", &Humanoid::exitRagdoll);
        
    auto& desc = Engine::Reflection::EnumRegistry::instance().registerEnum("HumanoidState");
    desc.values = {
        {"Idle", (int)HumanoidState::Idle},
        {"Walking", (int)HumanoidState::Walking},
        {"Jumping", (int)HumanoidState::Jumping},
        {"Falling", (int)HumanoidState::Falling},
        {"Landed", (int)HumanoidState::Landed},
        {"Climbing", (int)HumanoidState::Climbing},
        {"Ragdoll", (int)HumanoidState::Ragdoll}
    };

    // Register IKControl here to prevent linker from stripping its TU
    Engine::Reflection::ClassBuilder<IKControl>("IKControl")
        .base("Instance")
        .property("EndEffector", &IKControl::endEffector)
        .property("TargetPosition", &IKControl::targetPosition)
        .property("PoleVector", &IKControl::poleVector)
        .property("Weight", &IKControl::weight);
}
static struct HumanoidRegister { HumanoidRegister() { registerHumanoid(); } } s_register;
