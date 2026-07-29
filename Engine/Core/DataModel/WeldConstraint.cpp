#include "WeldConstraint.h"
#include "../../Physics/PhysicsWorld.h"
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>

void WeldConstraint::createJoltConstraint() {
    auto p0 = std::dynamic_pointer_cast<Part>(part0.lock());
    auto p1 = std::dynamic_pointer_cast<Part>(part1.lock());
    if (!p0 || !p1) return;

    JPH::BodyID id0(p0->getPhysicsBodyId());
    JPH::BodyID id1(p1->getPhysicsBodyId());

    if (id0.IsInvalid() || id1.IsInvalid()) return;

    auto& physicsSystem = Engine::Physics::PhysicsWorld::instance().getPhysicsSystem();
    auto& bodyInterface = physicsSystem.GetBodyInterface();

    const JPH::BodyID ids[2] = { id0, id1 };
    JPH::BodyLockMultiWrite locks(physicsSystem.GetBodyLockInterface(), ids, 2);
    JPH::Body* b0 = locks.GetBody(0);
    JPH::Body* b1 = locks.GetBody(1);
    
    if (b0 && b1) {
        JPH::FixedConstraintSettings settings;
        settings.mAutoDetectPoint = true; // Use current relative positions

        joltConstraint = settings.Create(*b0, *b1);
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
