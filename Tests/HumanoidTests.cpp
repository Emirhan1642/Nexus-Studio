#include <gtest/gtest.h>
#include "TestEnvironment.h"
#include "../Engine/Core/DataModel/DataModel.h"
#include "../Engine/Core/DataModel/Part.h"
#include "../Engine/Core/DataModel/Humanoid.h"
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
