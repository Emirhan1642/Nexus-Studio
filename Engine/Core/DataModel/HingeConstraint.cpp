#include "HingeConstraint.h"
#include "../../Physics/PhysicsWorld.h"
#include <Jolt/Physics/Constraints/HingeConstraint.h>

void HingeConstraint::createJoltConstraint() {
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
        JPH::HingeConstraintSettings settings;
        
        // MVP: Set the pivot point at the center of Part0, and rotate around Y axis
        JPH::RVec3 pivot = lock0.GetBody().GetPosition();
        JPH::Vec3 axis = JPH::Vec3::sAxisY();

        settings.mPoint1 = pivot;
        settings.mPoint2 = pivot;
        settings.mHingeAxis1 = axis;
        settings.mHingeAxis2 = axis;
        settings.mNormalAxis1 = JPH::Vec3::sAxisX();
        settings.mNormalAxis2 = JPH::Vec3::sAxisX();

        joltConstraint = settings.Create(lock0.GetBody(), lock1.GetBody());
        Engine::Physics::PhysicsWorld::instance().addConstraint(joltConstraint);
    }
}

#include "../Reflection/ClassBuilder.h"
namespace {
    struct HingeConstraintReflectionInit {
        HingeConstraintReflectionInit() {
            using namespace Engine::Reflection;
            ClassBuilder<HingeConstraint>("HingeConstraint")
                .base("Constraint");
        }
    } g_hingeConstraintReflectionInit;
}
