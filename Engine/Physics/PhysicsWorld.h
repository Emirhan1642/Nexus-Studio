#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>

#include "ContactListenerImpl.h"
#include "../Core/Signal.h"

namespace Engine::Physics {

struct PrePhysicsUpdateEvent {
    float deltaTime;
};

struct PostPhysicsUpdateEvent {
    float deltaTime;
};

// Layer definitions
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer CHARACTER = 2;
    static constexpr JPH::ObjectLayer RAGDOLL = 3;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 4;
}

namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr uint32_t NUM_LAYERS(2);
}

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::CHARACTER] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::RAGDOLL] = BroadPhaseLayers::MOVING;
    }

    virtual uint32_t GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        return mObjectToBroadPhase[inLayer];
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char * GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:     return "MOVING";
        default:                                                       return "INVALID";
        }
    }
#endif
private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
        case Layers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
        case Layers::CHARACTER:
        case Layers::RAGDOLL:
            return true;
        default:
            return false;
        }
    }
};

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
        switch (inObject1) {
        case Layers::NON_MOVING:
            return inObject2 == Layers::MOVING || inObject2 == Layers::CHARACTER || inObject2 == Layers::RAGDOLL;
        case Layers::MOVING:
        case Layers::CHARACTER:
        case Layers::RAGDOLL:
            return true;
        default:
            return false;
        }
    }
};

class PhysicsWorld {
public:
    static PhysicsWorld& instance() {
        static PhysicsWorld pw;
        return pw;
    }

    static void initJolt();
    void initialize();
    void shutdown();
    void step(float deltaTime);

    void addConstraint(JPH::Constraint* constraint);
    void removeConstraint(JPH::Constraint* constraint);

    Engine::Signal prePhysicsUpdate;
    Engine::Signal postPhysicsUpdate;

    JPH::BodyInterface& getBodyInterface() { return physicsSystem.GetBodyInterface(); }
    JPH::PhysicsSystem& getPhysicsSystem() { return physicsSystem; }

    JPH::TempAllocatorImpl* getTempAllocator() { return tempAllocator; }
    BPLayerInterfaceImpl& getBroadPhaseLayerInterface() { return broadPhaseLayerInterface; }
    ObjectVsBroadPhaseLayerFilterImpl& getObjectVsBroadPhaseLayerFilter() { return objectVsBroadPhaseFilter; }
    ObjectLayerPairFilterImpl& getObjectLayerPairFilter() { return objectLayerPairFilter; }

private:
    PhysicsWorld() = default;

    JPH::PhysicsSystem physicsSystem;
    float accumulator = 0.0f;
    
    ContactListenerImpl contactListener;
    BPLayerInterfaceImpl broadPhaseLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseFilter;
    ObjectLayerPairFilterImpl objectLayerPairFilter;

    JPH::TempAllocatorImpl* tempAllocator = nullptr;
    JPH::JobSystemThreadPool* jobSystem = nullptr;
};

} // namespace Engine::Physics
