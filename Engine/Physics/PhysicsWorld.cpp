#include "PhysicsWorld.h"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include "PhysicsConversions.h"
#include <cstdarg>
#include <cstdio>
#include <iostream>

// Forward declaration for DataModel and Part inclusion
#include "../Core/DataModel/Instance.h"
#include "../Core/DataModel/Part.h"
#include "../Core/DataModel/DataModel.h"

namespace Engine::Physics {

static void TraceImpl(const char* inFMT, ...) {
    // Basic formatting for trace
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint32_t inLine) {
    return true; 
}
#endif

void PhysicsWorld::initJolt() {
    std::cout << "  [JOLT] RegisterDefaultAllocator" << std::endl;
    JPH::RegisterDefaultAllocator();

    JPH::Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

    std::cout << "  [JOLT] Factory create" << std::endl;
    JPH::Factory::sInstance = new JPH::Factory();
    std::cout << "  [JOLT] RegisterTypes" << std::endl;
    JPH::RegisterTypes();
}

void PhysicsWorld::initialize() {
    std::cout << "  [JOLT] Allocating tempAllocator" << std::endl;
    tempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
    std::cout << "  [JOLT] Allocating jobSystem" << std::endl;
    jobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

    const uint32_t cMaxBodies = 10240;
    const uint32_t cNumBodyMutexes = 0; // default
    const uint32_t cMaxBodyPairs = 10240;
    const uint32_t cMaxContactConstraints = 10240;

    std::cout << "  [JOLT] Init physicsSystem" << std::endl;
    physicsSystem.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
        broadPhaseLayerInterface, objectVsBroadPhaseFilter, objectLayerPairFilter);

    std::cout << "  [JOLT] SetContactListener" << std::endl;
    physicsSystem.SetContactListener(&contactListener);
    
    // Roblox studs per meter is handled in Conversions, but gravity is 9.81 * STUDS_PER_METER
    physicsSystem.SetGravity(JPH::Vec3(0, -9.81f * STUDS_PER_METER, 0));
    std::cout << "  [JOLT] Initialize complete" << std::endl;
}

void PhysicsWorld::shutdown() {
    delete jobSystem;
    delete tempAllocator;
    
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

void PhysicsWorld::step(float deltaTime) {
    constexpr float fixedTimeStep = 1.0f / 60.0f;
    accumulator += deltaTime;

    bool stepped = false;
    while (accumulator >= fixedTimeStep) {
        physicsSystem.Update(fixedTimeStep, 1, tempAllocator, jobSystem);
        accumulator -= fixedTimeStep;
        stepped = true;
    }

    if (stepped) {
        // Helper function to find a part by its instance ID
        auto findPartById = [&](uint64_t id) -> std::shared_ptr<Part> {
            std::shared_ptr<Part> foundPart;
            auto search = [&](auto& self, const std::shared_ptr<Instance>& inst) -> void {
                if (inst->getInstanceId() == id) {
                    foundPart = std::dynamic_pointer_cast<Part>(inst);
                    return;
                }
                for (const auto& child : inst->getChildren()) {
                    self(self, child);
                    if (foundPart) return;
                }
            };
            search(search, DataModel::instance());
            return foundPart;
        };

        // Sync back active bodies to DataModel
        JPH::BodyIDVector activeBodies;
        physicsSystem.GetActiveBodies(JPH::EBodyType::RigidBody, activeBodies);
        
        for (JPH::BodyID bodyId : activeBodies) {
            uint64_t ownerId = physicsSystem.GetBodyInterface().GetUserData(bodyId);
            if (ownerId == 0) continue;
            
            std::shared_ptr<Part> foundPart = findPartById(ownerId);

            if (foundPart) {
                JPH::RVec3 joltPos = physicsSystem.GetBodyInterface().GetPosition(bodyId);
                // Directly write to fields to avoid calling setter which would feedback into Jolt
                foundPart->position = fromJoltVec3(joltPos);
                
                // Currently Part doesn't have rotation field in MVP, but if it did, we'd update it here
                
                foundPart->markRenderDirty();
            }
        }

        // Process Contact Events
        auto contactEvents = PendingContactEvents::instance().drainAll();
        for (const auto& ce : contactEvents) {
            auto part1 = findPartById(ce.id1);
            auto part2 = findPartById(ce.id2);
            if (part1 && part2) {
                std::shared_ptr<Instance> inst1 = std::static_pointer_cast<Instance>(part1);
                std::shared_ptr<Instance> inst2 = std::static_pointer_cast<Instance>(part2);
                part1->Touched.fire({inst2});
                part2->Touched.fire({inst1});
            }
        }
    }
}

void PhysicsWorld::addConstraint(JPH::Constraint* constraint) {
    if (constraint) {
        physicsSystem.AddConstraint(constraint);
    }
}

void PhysicsWorld::removeConstraint(JPH::Constraint* constraint) {
    if (constraint) {
        physicsSystem.RemoveConstraint(constraint);
    }
}

} // namespace Engine::Physics
