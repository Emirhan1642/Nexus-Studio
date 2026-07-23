#include "WeldConstraint.h"
#include "../../Physics/PhysicsWorld.h"
#include <Jolt/Physics/Constraints/FixedConstraint.h>

void WeldConstraint::createJoltConstraint() {
    auto p0 = std::dynamic_pointer_cast<Part>(part0.lock());
    auto p1 = std::dynamic_pointer_cast<Part>(part1.lock());
    if (!p0 || !p1) return;

    JPH::BodyID id0(p0->getPhysicsBodyId());
    JPH::BodyID id1(p1->getPhysicsBodyId());

    if (id0.IsInvalid() || id1.IsInvalid()) return;

    auto& physicsSystem = Engine::Physics::PhysicsWorld::instance().getPhysicsSystem();
    auto& bodyInterface = physicsSystem.GetBodyInterface();

    JPH::BodyLockWrite lock0(physicsSystem.GetBodyLockInterface(), id0);
    JPH::BodyLockWrite lock1(physicsSystem.GetBodyLockInterface(), id1);
    
    if (lock0.Succeeded() && lock1.Succeeded()) {
        JPH::FixedConstraintSettings settings;
        settings.mAutoDetectPoint = true; // Use current relative positions

        joltConstraint = settings.Create(lock0.GetBody(), lock1.GetBody());
        Engine::Physics::PhysicsWorld::instance().addConstraint(joltConstraint);
    }
}

#include "../Reflection/ClassBuilder.h"
namespace {
    struct WeldConstraintReflectionInit {
        WeldConstraintReflectionInit() {
            using namespace Engine::Reflection;
            ClassBuilder<WeldConstraint>("WeldConstraint")
                .base("Constraint");
        }
    } g_weldConstraintReflectionInit;
}
