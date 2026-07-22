#pragma once
#include "Instance.h"
#include "../Math/Vector3.h"

enum class Material {
    Wood,
    Metal,
    Plastic,
    Concrete
};

class Part : public Instance {
public:
    std::string getClassName() const override { return "Part"; }

    Engine::Math::Vector3 position;
    Engine::Math::Vector3 size{4.0f, 1.0f, 2.0f};
    float transparency = 0.0f;
    Material material = Material::Plastic;

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

private:
    uint32_t renderProxyIndex = 0xFFFFFFFF; // InvalidHandle
    uint32_t physicsBodyId = 0xFFFFFFFF; // JPH::BodyID::cInvalidBodyID
    bool anchored = false;
};
