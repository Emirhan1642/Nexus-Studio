#include "Instance.h"
#include "InstanceRegistry.h"

void Instance::notifyAddedToWorkspace() {
    onAddedToWorkspace();
    for (auto& child : children) {
        if (child) child->notifyAddedToWorkspace();
    }
}

void Instance::notifyRemovedFromWorkspace() {
    onRemovedFromWorkspace();
    for (auto& child : children) {
        if (child) child->notifyRemovedFromWorkspace();
    }
}

void Instance::setParent(const std::shared_ptr<Instance>& newParent) {
    if (auto oldParent = parent.lock()) {
        auto& siblings = oldParent->children;
        siblings.erase(
            std::remove(siblings.begin(), siblings.end(), shared_from_this()),
            siblings.end()
        );
        notifyRemovedFromWorkspace();
    }

    parent = newParent;
    if (newParent) {
        newParent->children.push_back(shared_from_this());
        InstanceRegistry::instance().registerInstance(shared_from_this());
        notifyAddedToWorkspace();
    }
}

std::shared_ptr<Instance> Instance::findFirstChild(const std::string& childName) const {
    for (const auto& child : children) {
        if (child && child->name == childName) return child;
    }
    return nullptr;
}

void Instance::destroy() {
    InstanceRegistry::instance().unregisterInstance(getInstanceId());
    // Children detach themselves from this vector during destroy(). Iterating
    // the live vector here invalidates the iterator and can crash during
    // teardown (notably when a Part owns a Humanoid).
    auto childSnapshot = children;
    for (auto& child : childSnapshot) {
        if (child) child->destroy();
    }
    children.clear();
    setParent(nullptr);
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
