#pragma once
#include "Instance.h"
#include "Part.h"
#include <vector>
#include <map>
#include <memory>
#include <string>

// A snapshot of the DataModel tree to save state before Play and restore it on Stop
struct PartSnapshot {
    InstanceId id;
    std::string name;
    std::weak_ptr<Instance> parent;
    Engine::Math::Vector3 position;
    Engine::Math::Vector3 size;
    bool anchored;
    Engine::Math::Vector3 albedoColor;
    float metallic;
    float roughness;
    float emissiveStrength;
    std::string albedoTexturePath;
    std::string normalTexturePath;
    std::string metallicTexturePath;
    std::string roughnessTexturePath;
    // Pointers to active instances are saved, so they are not fully serialized
    // Instead we just restore their properties
};

class DataModelSnapshot {
public:
    void capture(const std::shared_ptr<Instance>& root) {
        m_parts.clear();
        m_activeInstances.clear();
        captureRecursive(root);
    }

    void restore(const std::shared_ptr<Instance>& root) {
        // Find all parts currently in the workspace that were not in the snapshot, and destroy them
        std::vector<std::shared_ptr<Instance>> currentInstances;
        gatherAllInstances(root, currentInstances);

        for (auto& inst : currentInstances) {
            if (m_activeInstances.find(inst->getInstanceId()) == m_activeInstances.end() && inst != root) {
                inst->destroy();
            }
        }

        // Restore properties for saved parts
        for (const auto& snap : m_parts) {
            auto parent = snap.parent.lock();
            if (!parent) continue; // Original parent was destroyed somehow

            // Find the instance if it still exists
            std::shared_ptr<Instance> found = nullptr;
            for (auto& inst : currentInstances) {
                if (inst->getInstanceId() == snap.id) {
                    found = inst;
                    break;
                }
            }

            if (!found) continue; // If the instance was fully deleted in play mode and collected, we don't recreate it here for simplicity of Phase 1

            if (auto part = std::dynamic_pointer_cast<Part>(found)) {
                part->name = snap.name;
                if (part->getParent() != parent) part->setParent(parent);
                part->setPosition(snap.position);
                part->setSize(snap.size);
                part->setAnchored(snap.anchored);
                part->setAlbedoColor(snap.albedoColor);
                part->setMetallic(snap.metallic);
                part->setRoughness(snap.roughness);
                part->setEmissiveStrength(snap.emissiveStrength);
                part->setAlbedoTexturePath(snap.albedoTexturePath);
                part->setNormalTexturePath(snap.normalTexturePath);
                part->setMetallicTexturePath(snap.metallicTexturePath);
                part->setRoughnessTexturePath(snap.roughnessTexturePath);
                
                part->resetPhysics(); // Reset velocity etc
            }
        }
    }

private:
    std::vector<PartSnapshot> m_parts;
    std::map<InstanceId, bool> m_activeInstances;

    void captureRecursive(const std::shared_ptr<Instance>& inst) {
        m_activeInstances[inst->getInstanceId()] = true;

        if (auto part = std::dynamic_pointer_cast<Part>(inst)) {
            PartSnapshot snap;
            snap.id = part->getInstanceId();
            snap.name = part->name;
            snap.parent = part->getParent();
            snap.position = part->getPosition();
            snap.size = part->getSize();
            snap.anchored = part->getAnchored();
            snap.albedoColor = part->getAlbedoColor();
            snap.metallic = part->getMetallic();
            snap.roughness = part->getRoughness();
            snap.emissiveStrength = part->getEmissiveStrength();
            snap.albedoTexturePath = part->getAlbedoTexturePath();
            snap.normalTexturePath = part->getNormalTexturePath();
            snap.metallicTexturePath = part->getMetallicTexturePath();
            snap.roughnessTexturePath = part->getRoughnessTexturePath();
            m_parts.push_back(snap);
        }

        for (const auto& child : inst->getChildren()) {
            captureRecursive(child);
        }
    }

    void gatherAllInstances(const std::shared_ptr<Instance>& inst, std::vector<std::shared_ptr<Instance>>& list) {
        list.push_back(inst);
        for (const auto& child : inst->getChildren()) {
            gatherAllInstances(child, list);
        }
    }
};
