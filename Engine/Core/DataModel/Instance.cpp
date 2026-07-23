#include "Instance.h"
#include "InstanceRegistry.h"

void Instance::setParent(const std::shared_ptr<Instance>& newParent) {
    if (auto oldParent = parent.lock()) {
        auto& siblings = oldParent->children;
        siblings.erase(
            std::remove(siblings.begin(), siblings.end(), shared_from_this()),
            siblings.end()
        );
        onRemovedFromWorkspace();
    }

    parent = newParent;
    if (newParent) {
        newParent->children.push_back(shared_from_this());
        InstanceRegistry::instance().registerInstance(shared_from_this());
        onAddedToWorkspace();
    }
}

std::shared_ptr<Instance> Instance::findFirstChild(const std::string& childName) const {
    for (const auto& child : children) {
        if (child->name == childName) return child;
    }
    return nullptr;
}

void Instance::destroy() {
    InstanceRegistry::instance().unregisterInstance(getInstanceId());
    setParent(nullptr);
    // Let children handle their own cleanup if needed, but clearing parent
    // effectively removes this instance from the active tree.
    
    // To fully match Roblox behavior, we would also recursively call destroy on children
    // and lock properties, but for Phase 1 this is sufficient.
}

std::shared_ptr<Instance> createInstance(const std::string& className) {
    auto* classDesc = Engine::Reflection::TypeRegistry::instance().find(className);
    if (!classDesc || !classDesc->factory) return nullptr;

    void* raw = classDesc->factory();
    return std::shared_ptr<Instance>(static_cast<Instance*>(raw));
}

// Reflection Registration
namespace {
    struct InstanceReflectionInit {
        InstanceReflectionInit() {
            using namespace Engine::Reflection;
            ClassBuilder<Instance>("Instance")
                .property("Name", &Instance::name).category("Data")
                .method("Destroy", &Instance::destroy);
        }
    } g_instanceReflectionInit;
}
