#pragma once
#include "Engine/Core/DataModel/Instance.h"
#include "Engine/Core/Reflection/TypeRegistry.h"
#include <any>
#include <string>

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
};

class PropertyChangeCommand : public ICommand {
public:
    PropertyChangeCommand(std::shared_ptr<Instance> inst, std::string propName, std::any oldVal, std::any newVal)
        : instance(inst), propertyName(propName), oldValue(oldVal), newValue(newVal) {}

    void execute() override {
        auto* prop = findProperty();
        if (prop) prop->setter(instance.get(), newValue);
    }
    void undo() override {
        auto* prop = findProperty();
        if (prop) prop->setter(instance.get(), oldValue);
    }

private:
    const Engine::Reflection::PropertyDescriptor* findProperty() {
        auto* classDesc = Engine::Reflection::TypeRegistry::instance().find(instance->getClassName());
        if (!classDesc) return nullptr;
        return classDesc->findProperty(propertyName);
    }

    std::shared_ptr<Instance> instance;
    std::string propertyName;
    std::any oldValue, newValue;
};

class CreateInstanceCommand : public ICommand {
public:
    CreateInstanceCommand(std::shared_ptr<Instance> newInst, std::shared_ptr<Instance> parent)
        : instance(newInst), targetParent(parent) {}

    void execute() override {
        instance->setParent(targetParent);
    }
    void undo() override {
        instance->setParent(nullptr);
    }

private:
    std::shared_ptr<Instance> instance;
    std::shared_ptr<Instance> targetParent;
};
