#include "Part.h"
#include "../Reflection/ClassBuilder.h"
#include "../Reflection/EnumRegistry.h"
#include "../../Renderer/SceneGraph/RenderScene.h"
#include "../../Physics/PhysicsWorld.h"
#include "../../Physics/PhysicsConversions.h"
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

void Part::setPosition(const Engine::Math::Vector3& newPos) {
    position = newPos;
    markRenderDirty();
    
    if (physicsBodyId != 0xFFFFFFFF) {
        JPH::BodyID bodyId(physicsBodyId);
        Engine::Physics::PhysicsWorld::instance().getBodyInterface().SetPosition(
            bodyId, Engine::Physics::toJoltVec3(newPos), JPH::EActivation::Activate
        );
    }
}

void Part::setSize(const Engine::Math::Vector3& newSize) {
    size = newSize;
    markRenderDirty();
    // Note: Changing size of a physics body in Jolt requires recreating the shape.
    // For MVP, we might skip dynamic resizing of physics bodies.
}

void Part::markRenderDirty() {
    if (renderProxyIndex != Engine::Renderer::InvalidHandle) {
        Engine::Math::Matrix4 transform = Engine::Math::Matrix4::fromPositionAndSize(position, size);
        Engine::Renderer::RenderScene::instance().markDirty(renderProxyIndex, transform);
    }
}

void Part::onAddedToWorkspace() {
    Engine::Renderer::RenderProxy proxy;
    proxy.worldTransform = Engine::Math::Matrix4::fromPositionAndSize(position, size);
    renderProxyIndex = Engine::Renderer::RenderScene::instance().registerProxy(getInstanceId(), proxy);
    
    JPH::BodyCreationSettings bodySettings(
        new JPH::BoxShape(Engine::Physics::toJoltVec3(size * 0.5f)),
        Engine::Physics::toJoltVec3(position),
        JPH::Quat::sIdentity(),
        anchored ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic,
        anchored ? Engine::Physics::Layers::NON_MOVING : Engine::Physics::Layers::MOVING
    );
    bodySettings.mUserData = (uint64_t)getInstanceId();
    
    JPH::BodyID bodyId = Engine::Physics::PhysicsWorld::instance().getBodyInterface().CreateAndAddBody(
        bodySettings, JPH::EActivation::Activate
    );
    
    physicsBodyId = bodyId.GetIndexAndSequenceNumber();
}

void Part::onRemovedFromWorkspace() {
    if (renderProxyIndex != Engine::Renderer::InvalidHandle) {
        Engine::Renderer::RenderScene::instance().unregisterProxy(renderProxyIndex);
        renderProxyIndex = Engine::Renderer::InvalidHandle;
    }
    if (physicsBodyId != 0xFFFFFFFF) {
        JPH::BodyID bodyId(physicsBodyId);
        Engine::Physics::PhysicsWorld::instance().getBodyInterface().RemoveBody(bodyId);
        Engine::Physics::PhysicsWorld::instance().getBodyInterface().DestroyBody(bodyId);
        physicsBodyId = 0xFFFFFFFF;
    }
}

void Part::resetPhysics() {
    if (physicsBodyId != 0xFFFFFFFF) {
        JPH::BodyID bodyId(physicsBodyId);
        auto& bi = Engine::Physics::PhysicsWorld::instance().getBodyInterface();
        bi.SetLinearVelocity(bodyId, JPH::Vec3::sZero());
        bi.SetAngularVelocity(bodyId, JPH::Vec3::sZero());
    }
}

void Part::setAnchored(const bool& value) {
    anchored = value;
    if (physicsBodyId != 0xFFFFFFFF) {
        JPH::BodyID bodyId(physicsBodyId);
        auto& bi = Engine::Physics::PhysicsWorld::instance().getBodyInterface();
        bi.SetMotionType(bodyId, value ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
        bi.SetObjectLayer(bodyId, value ? Engine::Physics::Layers::NON_MOVING : Engine::Physics::Layers::MOVING);
    }
}

namespace {
    struct MaterialEnumInit {
        MaterialEnumInit() {
            using namespace Engine::Reflection;
            auto& e = EnumRegistry::instance().registerEnum("Material");
            e.values = {
                {"Wood", (int)Material::Wood},
                {"Metal", (int)Material::Metal},
                {"Plastic", (int)Material::Plastic},
                {"Concrete", (int)Material::Concrete}
            };
        }
    } g_materialEnumInit;

    struct PartReflectionInit {
        PartReflectionInit() {
            using namespace Engine::Reflection;
            ClassBuilder<Part>("Part")
                .base("Instance")
                .propertyAccessor("Position", &Part::getPosition, &Part::setPosition).category("Data")
                .propertyAccessor("Size", &Part::getSize, &Part::setSize).category("Data")
                .property("Transparency", &Part::transparency).category("Appearance")
                .propertyAccessor("Anchored", &Part::getAnchored, &Part::setAnchored).category("Behavior")
                .enumProperty("Material", &Part::material, "Material").category("Appearance");
        }
    } g_partReflectionInit;
}
