#include "SpringConstraint.h"
#include "../../Physics/PhysicsWorld.h"
#include <Jolt/Physics/Constraints/DistanceConstraint.h>

void SpringConstraint::createJoltConstraint() {
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
        JPH::DistanceConstraintSettings settings;
        
        settings.mPoint1 = lock0.GetBody().GetPosition();
        settings.mPoint2 = lock1.GetBody().GetPosition();
        
        // Define Spring properties
        settings.mLimitsSpringSettings.mMode = JPH::ESpringMode::FrequencyAndDamping;
        settings.mLimitsSpringSettings.mFrequency = stiffness;
        settings.mLimitsSpringSettings.mDamping = damping;
        // Limits
        if (limitsEnabled) {
            settings.mMinDistance = minLength;
            settings.mMaxDistance = maxLength;
        } else {
            // MVP: min and max distance are the free length if no limits
            settings.mMinDistance = freeLength;
            settings.mMaxDistance = freeLength;
        }

        joltConstraint = settings.Create(lock0.GetBody(), lock1.GetBody());
        Engine::Physics::PhysicsWorld::instance().addConstraint(joltConstraint);
    }
}

#include "../Reflection/ClassBuilder.h"
namespace {
    struct SpringConstraintReflectionInit {
        SpringConstraintReflectionInit() {
            using namespace Engine::Reflection;
            ClassBuilder<SpringConstraint>("SpringConstraint")
                .base("Constraint")
                .propertyAccessor("FreeLength", &SpringConstraint::getFreeLength, &SpringConstraint::setFreeLength).category("Spring")
                .propertyAccessor("Stiffness", &SpringConstraint::getStiffness, &SpringConstraint::setStiffness).category("Spring")
                .propertyAccessor("Damping", &SpringConstraint::getDamping, &SpringConstraint::setDamping).category("Spring")
                .propertyAccessor("LimitsEnabled", &SpringConstraint::getLimitsEnabled, &SpringConstraint::setLimitsEnabled).category("Spring")
                .propertyAccessor("MinLength", &SpringConstraint::getMinLength, &SpringConstraint::setMinLength).category("Spring")
                .propertyAccessor("MaxLength", &SpringConstraint::getMaxLength, &SpringConstraint::setMaxLength).category("Spring");
        }
    } g_springConstraintReflectionInit;
}
