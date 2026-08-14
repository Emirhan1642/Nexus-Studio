#include "Part.h"
#include "../Reflection/ClassBuilder.h"
#include "../Reflection/EnumRegistry.h"
#include "../../Renderer/SceneGraph/RenderScene.h"
#include "../../Renderer/Renderer.h"
#include "../../Physics/PhysicsWorld.h"
#include "../../Physics/PhysicsConversions.h"
#include "../../Assets/AssetDependencyTracker.h"
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
    
    if (physicsBodyId != 0xFFFFFFFF) {
        JPH::BodyID bodyId(physicsBodyId);
        auto& bi = Engine::Physics::PhysicsWorld::instance().getBodyInterface();
        
        // Create new shape (half extents)
        JPH::RefConst<JPH::Shape> newShape = new JPH::BoxShape(Engine::Physics::toJoltVec3(size * 0.5f));
        
        // Set shape, update mass properties, and activate
        bi.SetShape(bodyId, newShape, true, JPH::EActivation::Activate);
    }
}

void Part::setMeshFromAsset(const Engine::Assets::AssetGuid& guid) {
    if (meshAssetGuid.isValid()) {
        Engine::Assets::AssetDependencyTracker::instance().unregisterUsage(meshAssetGuid, getInstanceId());
    }
    
    meshAssetGuid = guid;
    
    if (meshAssetGuid.isValid()) {
        Engine::Assets::AssetDependencyTracker::instance().registerUsage(meshAssetGuid, getInstanceId());
        
        // MVP: update texture if it's a texture asset, or just trigger render update
        const auto* meta = Engine::Assets::AssetDatabase::instance().find(guid);
        if (meta) {
            if (meta->importerType == "Texture") {
                setAlbedoTexturePath(Engine::Assets::AssetDatabase::instance().getAbsolutePath(meta->relativePath));
            }
        }
    }
}

void Part::setBoneTransforms(const std::vector<Engine::Math::Matrix4>& transforms) {
    currentBoneTransforms = transforms;
    markRenderDirty();
}

void Part::ensureCustomVertices() {
    if (customVertices.size() == 8) return;
    float hx = size.x;
    float hy = size.y;
    float hz = size.z;
    customVertices = {
        {-hx, -hy, -hz}, // 0
        { hx, -hy, -hz}, // 1
        { hx,  hy, -hz}, // 2
        {-hx,  hy, -hz}, // 3
        {-hx, -hy,  hz}, // 4
        { hx, -hy,  hz}, // 5
        { hx,  hy,  hz}, // 6
        {-hx,  hy,  hz}  // 7
    };
}

Engine::Math::Vector3 Part::getVertex(int index) const {
    if (index < 0 || index >= 8) return {0, 0, 0};
    if (customVertices.size() == 8) {
        return customVertices[index];
    }
    float hx = size.x;
    float hy = size.y;
    float hz = size.z;
    static const float signs[8][3] = {
        {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
        {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
    };
    return { signs[index][0] * hx, signs[index][1] * hy, signs[index][2] * hz };
}

void Part::setVertex(int index, const Engine::Math::Vector3& localPos) {
    if (index < 0 || index >= 8) return;
    ensureCustomVertices();
    customVertices[index] = localPos;
    rebuildProceduralMesh();
    markRenderDirty();
}

void Part::resetDeformation() {
    if (customMeshHandle != Engine::Renderer::InvalidHandle) {
        Engine::Renderer::RendererSystem::instance().destroyMesh(customMeshHandle);
        customMeshHandle = Engine::Renderer::InvalidHandle;
    }
    customVertices.clear();
    markRenderDirty();
}

void Part::rebuildProceduralMesh() {
    if (customVertices.size() < 8) return;
    if (customMeshHandle == Engine::Renderer::InvalidHandle) {
        customMeshHandle = Engine::Renderer::RendererSystem::instance().createDeformedCubeMesh(customVertices);
    } else {
        Engine::Renderer::RendererSystem::instance().updateDeformedCubeMesh(customMeshHandle, customVertices);
    }
}

void Part::markRenderDirty() {
    if (renderProxyIndex != Engine::Renderer::InvalidHandle) {
        Engine::Math::Matrix4 transform;
        if (customMeshHandle != Engine::Renderer::InvalidHandle) {
            transform = Engine::Math::Matrix4::translation(position);
        } else {
            transform = Engine::Math::Matrix4::fromPositionAndSize(position, size);
        }

        Engine::Renderer::RenderProxy proxy;
        proxy.worldTransform = transform;
        proxy.material.albedo = albedoColor;
        proxy.material.metallic = metallic;
        proxy.material.roughness = roughness;
        proxy.material.emissiveStrength = emissiveStrength;
        proxy.material.customShader = { customShaderHandle };
        proxy.boneTransforms = currentBoneTransforms;
        
        proxy.material.albedoTexture = Engine::Renderer::RendererSystem::instance().getTexture(albedoTexturePath);
        proxy.material.normalTexture = Engine::Renderer::RendererSystem::instance().getTexture(normalTexturePath);
        proxy.material.metallicTexture = Engine::Renderer::RendererSystem::instance().getTexture(metallicTexturePath);
        proxy.material.roughnessTexture = Engine::Renderer::RendererSystem::instance().getTexture(roughnessTexturePath);
        
        if (customMeshHandle != Engine::Renderer::InvalidHandle) {
            proxy.mesh = customMeshHandle;
        } else if (meshAssetGuid.isValid()) {
            proxy.mesh = Engine::Renderer::RendererSystem::instance().getMeshHandle(meshAssetGuid);
        }

        Engine::Renderer::RenderScene::instance().markDirty(renderProxyIndex, transform);
        Engine::Renderer::RenderScene::instance().updateProxy(renderProxyIndex, proxy);
    }
}

void Part::onAddedToWorkspace() {
    Engine::Math::Matrix4 transform;
    if (customMeshHandle != Engine::Renderer::InvalidHandle) {
        transform = Engine::Math::Matrix4::translation(position);
    } else {
        transform = Engine::Math::Matrix4::fromPositionAndSize(position, size);
    }

    Engine::Renderer::RenderProxy proxy;
    proxy.worldTransform = transform;
    proxy.material.albedo = albedoColor;
    proxy.material.metallic = metallic;
    proxy.material.roughness = roughness;
    proxy.material.emissiveStrength = emissiveStrength;
    proxy.material.customShader = { customShaderHandle };
    proxy.material.albedoTexture = Engine::Renderer::RendererSystem::instance().getTexture(albedoTexturePath);
    proxy.material.normalTexture = Engine::Renderer::RendererSystem::instance().getTexture(normalTexturePath);
    proxy.material.metallicTexture = Engine::Renderer::RendererSystem::instance().getTexture(metallicTexturePath);
    proxy.material.roughnessTexture = Engine::Renderer::RendererSystem::instance().getTexture(roughnessTexturePath);
    proxy.boneTransforms = currentBoneTransforms;
    
    if (customMeshHandle != Engine::Renderer::InvalidHandle) {
        proxy.mesh = customMeshHandle;
    } else if (meshAssetGuid.isValid()) {
        proxy.mesh = Engine::Renderer::RendererSystem::instance().getMeshHandle(meshAssetGuid);
    }
    
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
    if (customMeshHandle != Engine::Renderer::InvalidHandle) {
        Engine::Renderer::RendererSystem::instance().destroyMesh(customMeshHandle);
        customMeshHandle = Engine::Renderer::InvalidHandle;
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
        JPH::BodyID oldBodyId(physicsBodyId);
        auto& bi = Engine::Physics::PhysicsWorld::instance().getBodyInterface();
        bi.RemoveBody(oldBodyId);
        bi.DestroyBody(oldBodyId);
        physicsBodyId = 0xFFFFFFFF;

        JPH::BodyCreationSettings bodySettings(
            new JPH::BoxShape(Engine::Physics::toJoltVec3(size * 0.5f)),
            Engine::Physics::toJoltVec3(position),
            JPH::Quat::sIdentity(),
            anchored ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic,
            anchored ? Engine::Physics::Layers::NON_MOVING : Engine::Physics::Layers::MOVING
        );
        bodySettings.mUserData = (uint64_t)getInstanceId();
        
        JPH::BodyID newBodyId = bi.CreateAndAddBody(
            bodySettings, anchored ? JPH::EActivation::DontActivate : JPH::EActivation::Activate
        );
        physicsBodyId = newBodyId.GetIndexAndSequenceNumber();
    }
}

namespace {
    struct PartReflectionInit {
        PartReflectionInit() {
            using namespace Engine::Reflection;
            ClassBuilder<Part>("Part")
                .base("Instance")
                .propertyAccessor("Position", &Part::getPosition, &Part::setPosition).category("Transform")
                .propertyAccessor("Size", &Part::getSize, &Part::setSize).category("Transform")
                .property("Transparency", &Part::transparency).category("Appearance")
                .propertyAccessor("Anchored", &Part::getAnchored, &Part::setAnchored).category("Behavior")
                
                // Texture & Material Properties
                .propertyAccessor("AlbedoColor", &Part::getAlbedoColor, &Part::setAlbedoColor).category("Appearance")
                .propertyAccessor("Metallic", &Part::getMetallic, &Part::setMetallic).category("Appearance")
                .propertyAccessor("Roughness", &Part::getRoughness, &Part::setRoughness).category("Appearance")
                .propertyAccessor("EmissiveStrength", &Part::getEmissiveStrength, &Part::setEmissiveStrength).category("Appearance")
                
                .propertyAccessor("AlbedoTexture", &Part::getAlbedoTexturePath, &Part::setAlbedoTexturePath).category("Appearance")
                .propertyAccessor("NormalTexture", &Part::getNormalTexturePath, &Part::setNormalTexturePath).category("Appearance")
                .propertyAccessor("MetallicTexture", &Part::getMetallicTexturePath, &Part::setMetallicTexturePath).category("Appearance")
                .propertyAccessor("RoughnessTexture", &Part::getRoughnessTexturePath, &Part::setRoughnessTexturePath).category("Appearance")

                .signal("Touched", &Part::Touched);
        }
    } g_partReflectionInit;
}
