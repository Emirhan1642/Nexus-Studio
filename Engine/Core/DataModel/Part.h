#pragma once
#include "Instance.h"
#include "../Math/Vector3.h"
#include "../../Assets/AssetDatabase.h"
#include "../Geometry/EditableMesh.h"
#include <cstdint>
#include <memory>

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

    uint16_t customShaderHandle = 65535; // 65535 == bgfx::kInvalidHandle
    void setCustomShader(uint16_t handle) { customShaderHandle = handle; markRenderDirty(); }
    uint16_t getCustomShader() const { return customShaderHandle; }

    void setBoneTransforms(const std::vector<Engine::Math::Matrix4>& transforms);
    const std::vector<Engine::Math::Matrix4>& getBoneTransforms() const { return currentBoneTransforms; }

    // Vertex / Mesh Deformation & EditableMesh (Blender Edit Mode Style)
    bool isDeformed() const { return m_editableMesh != nullptr || !customVertices.empty(); }
    Engine::Math::Vector3 getVertex(int index) const;
    void setVertex(int index, const Engine::Math::Vector3& localPos);
    void resetDeformation();
    const std::vector<Engine::Math::Vector3>& getCustomVertices() const { return customVertices; }

    std::shared_ptr<Engine::Geometry::EditableMesh> getEditableMesh();
    void setEditableMesh(std::shared_ptr<Engine::Geometry::EditableMesh> mesh);
    void ensureEditableMesh();
    void ensureCustomVertices();
    void rebuildProceduralMesh();

private:
    std::shared_ptr<Engine::Geometry::EditableMesh> m_editableMesh;
    std::vector<Engine::Math::Vector3> customVertices;
    uint32_t customMeshHandle = 0xFFFFFFFF; // InvalidHandle
    std::vector<Engine::Math::Matrix4> currentBoneTransforms;
    uint32_t renderProxyIndex = 0xFFFFFFFF; // InvalidHandle
    uint32_t physicsBodyId = 0xFFFFFFFF; // JPH::BodyID::cInvalidBodyID
    bool anchored = false;
};
