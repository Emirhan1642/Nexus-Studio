#pragma once
#include "Instance.h"
#include "../Math/Vector3.h"
#include "../../Assets/AssetDatabase.h"

class Part : public Instance {
public:
    std::string getClassName() const override { return "Part"; }

    Engine::Signal Touched;

    Engine::Math::Vector3 position;
    Engine::Math::Vector3 size{4.0f, 1.0f, 2.0f};
    float transparency = 0.0f;

    Engine::Math::Vector3 albedoColor{1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float emissiveStrength = 0.0f;

    std::string albedoTexturePath = "";
    std::string normalTexturePath = "";
    std::string metallicTexturePath = "";
    std::string roughnessTexturePath = "";

    void setAlbedoColor(const Engine::Math::Vector3& color) { albedoColor = color; markRenderDirty(); }
    void setMetallic(const float& m) { metallic = m; markRenderDirty(); }
    void setRoughness(const float& r) { roughness = r; markRenderDirty(); }
    void setEmissiveStrength(const float& e) { emissiveStrength = e; markRenderDirty(); }

    Engine::Math::Vector3 getAlbedoColor() const { return albedoColor; }
    float getMetallic() const { return metallic; }
    float getRoughness() const { return roughness; }
    float getEmissiveStrength() const { return emissiveStrength; }

    void setAlbedoTexturePath(const std::string& path) { albedoTexturePath = path; markRenderDirty(); }
    void setNormalTexturePath(const std::string& path) { normalTexturePath = path; markRenderDirty(); }
    void setMetallicTexturePath(const std::string& path) { metallicTexturePath = path; markRenderDirty(); }
    void setRoughnessTexturePath(const std::string& path) { roughnessTexturePath = path; markRenderDirty(); }

    std::string getAlbedoTexturePath() const { return albedoTexturePath; }
    std::string getNormalTexturePath() const { return normalTexturePath; }
    std::string getMetallicTexturePath() const { return metallicTexturePath; }
    std::string getRoughnessTexturePath() const { return roughnessTexturePath; }

    void setPosition(const Engine::Math::Vector3& newPos);
    Engine::Math::Vector3 getPosition() const { return position; }

    void setSize(const Engine::Math::Vector3& newSize);
    Engine::Math::Vector3 getSize() const { return size; }

    void setAnchored(const bool& value);
    bool getAnchored() const { return anchored; }

    void onAddedToWorkspace() override;
    void onRemovedFromWorkspace() override;
    void markRenderDirty();
    
    void resetPhysics();
    uint32_t getPhysicsBodyId() const { return physicsBodyId; }

    Engine::Assets::AssetGuid meshAssetGuid;
    void setMeshFromAsset(const Engine::Assets::AssetGuid& guid);

    void setBoneTransforms(const std::vector<Engine::Math::Matrix4>& transforms);

private:
    std::vector<Engine::Math::Matrix4> currentBoneTransforms;
    uint32_t renderProxyIndex = 0xFFFFFFFF; // InvalidHandle
    uint32_t physicsBodyId = 0xFFFFFFFF; // JPH::BodyID::cInvalidBodyID
    bool anchored = false;
};
