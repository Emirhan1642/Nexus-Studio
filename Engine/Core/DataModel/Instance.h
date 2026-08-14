#pragma once
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include "../Reflection/ClassBuilder.h"

using InstanceId = uint64_t;

class Instance : public std::enable_shared_from_this<Instance> {
public:
    virtual ~Instance() = default;

    std::string name = "Instance";
    std::string customClassName = "";
    bool alwaysRelevant = false;

    virtual std::string getClassName() const {
        if (!customClassName.empty()) return customClassName;
        return "Instance";
    }

    InstanceId getInstanceId() const { return reinterpret_cast<InstanceId>(this); }

    void setParent(const std::shared_ptr<Instance>& newParent);
    std::shared_ptr<Instance> findFirstChild(const std::string& childName) const;
    const std::vector<std::shared_ptr<Instance>>& getChildren() const { return children; }
    std::shared_ptr<Instance> getParent() const { return parent.lock(); }

    void destroy();

    void notifyAddedToWorkspace();
    void notifyRemovedFromWorkspace();

    virtual void onAddedToWorkspace() {}
    virtual void onRemovedFromWorkspace() {}

protected:
    std::weak_ptr<Instance> parent;
    std::vector<std::shared_ptr<Instance>> children;
};

// Generic factory function using reflection
std::shared_ptr<Instance> createInstance(const std::string& className);
