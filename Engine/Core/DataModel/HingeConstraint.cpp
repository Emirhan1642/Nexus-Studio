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
        
        // Apply Pivot and Axis (Pivot is relative to world or Part0? Let's assume it's world or local to Part0. 
        // Typically it's relative to Part0 in Roblox, but for MVP let's assume world space offset from Part0 for simplicity)
        JPH::RVec3 pivotJolt = lock0.GetBody().GetPosition() + JPH::Vec3(pivot.x, pivot.y, pivot.z);
        JPH::Vec3 axisJolt(axis.x, axis.y, axis.z);
        axisJolt = axisJolt.Normalized();

        settings.mPoint1 = pivotJolt;
        settings.mPoint2 = pivotJolt;
        settings.mHingeAxis1 = axisJolt;
        settings.mHingeAxis2 = axisJolt;
        
        // Find a normal axis (perpendicular to hinge axis)
        JPH::Vec3 normalAxis = JPH::Vec3::sAxisX();
        if (abs(axisJolt.Dot(normalAxis)) > 0.99f) {
            normalAxis = JPH::Vec3::sAxisZ();
        }
        
        settings.mNormalAxis1 = normalAxis;
        settings.mNormalAxis2 = normalAxis;

        if (limitsEnabled) {
            settings.mLimitsMin = lowerLimit;
            settings.mLimitsMax = upperLimit;
        }

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
                .base("Constraint")
                .propertyAccessor("Pivot", &HingeConstraint::getPivot, &HingeConstraint::setPivot).category("Hinge")
                .propertyAccessor("Axis", &HingeConstraint::getAxis, &HingeConstraint::setAxis).category("Hinge")
                .propertyAccessor("LimitsEnabled", &HingeConstraint::getLimitsEnabled, &HingeConstraint::setLimitsEnabled).category("Hinge")
                .propertyAccessor("LowerLimit", &HingeConstraint::getLowerLimit, &HingeConstraint::setLowerLimit).category("Hinge")
                .propertyAccessor("UpperLimit", &HingeConstraint::getUpperLimit, &HingeConstraint::setUpperLimit).category("Hinge");
        }
    } g_hingeConstraintReflectionInit;
}
