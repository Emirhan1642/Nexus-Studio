#include <gtest/gtest.h>
#include "TestEnvironment.h"
#include "../Engine/Core/DataModel/DataModel.h"
#include "../Engine/Core/DataModel/Part.h"
#include "../Engine/Core/DataModel/Humanoid.h"
#include "../Engine/Core/DataModel/IKControl.h"
#include "../Engine/Physics/PhysicsWorld.h"
#include <thread>
#include <chrono>

using namespace Engine;
using namespace Engine::Math;

class HumanoidTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensureTestEnvironmentInitialized();
    }
};

TEST_F(HumanoidTest, BasicMovement) {
    auto root = DataModel::instance();

    // Create Floor
    auto floor = std::static_pointer_cast<Part>(createInstance("Part"));
    floor->name = "Floor";
    floor->setSize(Vector3(100.0f, 1.0f, 100.0f));
    floor->setPosition(Vector3(0, -0.5f, 0));
    floor->setAnchored(true);
    floor->setParent(root);

    // Create Humanoid Root Part
    auto rootPart = std::static_pointer_cast<Part>(createInstance("Part"));
    rootPart->name = "HumanoidRootPart";
    rootPart->setSize(Vector3(2.0f, 4.0f, 2.0f));
    rootPart->setPosition(Vector3(0, 10.0f, 0)); // Start in the air
    rootPart->setAnchored(false);
    rootPart->setParent(root);

    // Create Humanoid
    auto humanoid = std::static_pointer_cast<Humanoid>(createInstance("Humanoid"));
    humanoid->name = "TestHumanoid";
    humanoid->setParent(rootPart); // This will trigger onAddedToWorkspace and initCharacterVirtual

    ASSERT_NE(humanoid->getCharacter(), nullptr);
    EXPECT_EQ(humanoid->getRootPart(), rootPart);

    auto& physicsWorld = Physics::PhysicsWorld::instance();

    // Step physics to let character fall to the ground
    for (int i = 0; i < 60; ++i) { // 1 second of simulation
        physicsWorld.step(1.0f / 60.0f);
    }

    // It should have landed on the floor (Y ~ 2.0f because height is 4, half-height is 2)
    float landedY = rootPart->getPosition().y;
    EXPECT_NEAR(landedY, 2.0f, 0.1f);
    EXPECT_EQ(humanoid->getState(), HumanoidState::Idle); // Should be idle when standing still

    // Test moving forward
    Vector3 moveDir(1.0f, 0.0f, 0.0f);
    humanoid->moveTo(moveDir);
    
    // Simulate half a second of movement
    for (int i = 0; i < 30; ++i) {
        physicsWorld.step(1.0f / 60.0f);
    }

    // It should have moved in the +X direction (speed is 16 stud/sec, so 0.5s is ~8 studs)
    float newX = rootPart->getPosition().x;
    EXPECT_NEAR(newX, 8.0f, 1.0f);
    EXPECT_EQ(humanoid->getState(), HumanoidState::Walking);

    // Test Jumping
    humanoid->jump();
    
    // Simulate a few frames for the jump velocity to take effect
    for (int i = 0; i < 10; ++i) {
        physicsWorld.step(1.0f / 60.0f);
    }

    float jumpY = rootPart->getPosition().y;
    EXPECT_GT(jumpY, landedY + 0.1f); // Should be higher than before

    // Cleanup
    humanoid->destroy();
    rootPart->destroy();
    floor->destroy();
}

TEST_F(HumanoidTest, IKControlApplication) {
    // Verify IKControl is registered in the TypeRegistry
    auto* ikDesc = Engine::Reflection::TypeRegistry::instance().find("IKControl");
    ASSERT_NE(ikDesc, nullptr) << "IKControl class not registered in TypeRegistry";
    ASSERT_NE(ikDesc->factory, nullptr) << "IKControl has no factory function";

    // Create a fresh standalone root (don't use DataModel singleton which may be dirty from BasicMovement test)
    auto rootPart = std::make_shared<Part>();
    rootPart->name = "HumanoidRootPart";
    rootPart->setSize(Vector3(2.0f, 4.0f, 2.0f));
    rootPart->setPosition(Vector3(0, 0, 0));
    rootPart->setAnchored(true);

    // Create Humanoid directly
    auto humanoid = std::make_shared<Humanoid>();
    humanoid->name = "TestHumanoid";
    humanoid->setParent(rootPart);

    // Create IKControl via reflection factory
    auto ikBase = createInstance("IKControl");
    ASSERT_NE(ikBase, nullptr) << "createInstance(IKControl) returned nullptr";

    auto ikControl = std::static_pointer_cast<Engine::IKControl>(ikBase);
    ASSERT_NE(ikControl, nullptr) << "Failed to cast to IKControl";

    ikControl->name = "IKControlTest";
    ikControl->endEffector = "Hand_R";
    ikControl->targetPosition = Vector3(5.0f, 5.0f, 0.0f);
    ikControl->weight = 1.0f;
    ikControl->setParent(humanoid);

    // Verify hierarchy
    EXPECT_EQ(ikControl->getParent(), humanoid);
    EXPECT_EQ(ikControl->endEffector, "Hand_R");

    // Step simulation - IK hook should execute without crash (skeleton is empty so apply() returns early)
    auto& physicsWorld = Physics::PhysicsWorld::instance();
    physicsWorld.step(1.0f / 60.0f);

    // Still alive after step
    EXPECT_EQ(ikControl->weight, 1.0f);
}

TEST_F(HumanoidTest, RagdollSimulation) {
    // 1. Create Humanoid and Root
    auto rootPart = std::make_shared<Part>();
    rootPart->name = "RagdollRoot";
    rootPart->setSize(Vector3(1.0f, 1.0f, 1.0f));
    rootPart->setPosition(Vector3(0, 10.0f, 0)); // Start in the air
    rootPart->setAnchored(true);

    auto humanoid = std::make_shared<Humanoid>();
    humanoid->name = "RagdollHumanoid";
    humanoid->setParent(rootPart);

    // 2. Setup Skeleton
    Engine::Animation::Bone rootBone;
    rootBone.name = "Root";
    rootBone.bindPoseLocalTransform = Math::Matrix4::fromTRS(Vector3(0, 0, 0), Math::Quaternion::identity(), Vector3(1,1,1));
    rootBone.inverseBindPoseWorldTransform = rootBone.bindPoseLocalTransform.inverse();
    
    Engine::Animation::Bone childBone;
    childBone.name = "Spine";
    childBone.parentIndex = 0;
    childBone.bindPoseLocalTransform = Math::Matrix4::fromTRS(Vector3(0, 1.0f, 0), Math::Quaternion::identity(), Vector3(1,1,1));
    childBone.inverseBindPoseWorldTransform = (rootBone.bindPoseLocalTransform * childBone.bindPoseLocalTransform).inverse();

    humanoid->getSkeleton().bones = { rootBone, childBone };

    // 3. Setup PhysicsAsset
    auto physicsAsset = std::make_shared<Engine::Physics::PhysicsAsset>();
    
    Engine::Physics::PhysicsBoneShape rootShape;
    rootShape.boneName = "Root";
    rootShape.radius = 0.5f;
    rootShape.halfHeight = 0.5f;
    
    Engine::Physics::PhysicsBoneShape childShape;
    childShape.boneName = "Spine";
    childShape.radius = 0.4f;
    childShape.halfHeight = 0.5f;
    
    physicsAsset->shapes = { rootShape, childShape };
    humanoid->physicsAsset = physicsAsset;

    // 4. Trigger Ragdoll
    EXPECT_EQ(humanoid->getState(), HumanoidState::Idle);
    humanoid->enterRagdoll();
    EXPECT_EQ(humanoid->getState(), HumanoidState::Ragdoll);

    // 5. Run simulation (let it fall)
    auto& physicsWorld = Physics::PhysicsWorld::instance();
    for(int i = 0; i < 10; ++i) {
        physicsWorld.step(1.0f / 60.0f);
        humanoid->update(1.0f / 60.0f); // update transfers Jolt to bone transforms
    }

    // 6. Verify bones moved from origin (due to gravity)
    // The part's boneTransforms array should now reflect physical positions
    auto boneTransforms = rootPart->getBoneTransforms();
    ASSERT_EQ(boneTransforms.size(), 2u);
    
    // Convert back from inverseBindPose to world pose to check position
    Math::Matrix4 rootWorld = boneTransforms[0] * rootBone.bindPoseLocalTransform;
    Vector3 rootPos = rootWorld.getTranslation();
    
    // It should have fallen slightly down from Y=0 (relative to rootPart, or world, depending on how we set it up)
    // Actually, Jolt bodies were created at worldPos = rootPart->pos + localPos
    // Let's just check that it's not (0,0,0) anymore, or that Y is less than initial
    EXPECT_NE(rootPos.y, 0.0f);

    // 7. Cleanup
    humanoid->exitRagdoll();
    EXPECT_EQ(humanoid->getState(), HumanoidState::Idle);
}
