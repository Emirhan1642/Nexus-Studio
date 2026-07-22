#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <any>
#include <vector>
#include <stdexcept>
#include <memory>

// Forward declaration of Instance for object references
class Instance;

namespace Engine::Reflection {

struct PropertyDescriptor {
    std::string name;
    std::function<std::any(void* instance)> getter;
    std::function<void(void* instance, const std::any& value)> setter;
    
    enum class Kind { Primitive, Enum, Array, ObjectRef } kind = Kind::Primitive;
    std::string typeName;       // e.g. "float", "Vector3"
    std::string enumTypeName;   // e.g. "Material" if Kind::Enum
    
    // For Kind::Array
    std::function<size_t(void* instance)> arraySize;
    std::function<std::any(void* instance, size_t index)> arrayGet;
    std::function<void(void* instance, size_t index, const std::any&)> arraySet;
    
    // For Kind::ObjectRef
    std::function<std::shared_ptr<Instance>(void*)> objectGetter;
    std::function<void(void*, std::shared_ptr<Instance>)> objectSetter;

    // Editor metadata
    std::string category = "Data";
    bool readOnly = false;
    std::string tooltip;
};

struct MethodDescriptor {
    std::string name;
    std::function<std::any(void* instance, std::vector<std::any> args)> invoke;
};

class ClassDescriptor {
public:
    std::string className;
    ClassDescriptor* baseClass = nullptr;
    std::vector<PropertyDescriptor> properties;
    std::vector<MethodDescriptor> methods;
    std::function<void*()> factory;

    const PropertyDescriptor* findProperty(const std::string& name) const {
        for (const auto& p : properties)
            if (p.name == name) return &p;
        if (baseClass) return baseClass->findProperty(name);
        return nullptr;
    }

    bool isA(const std::string& targetClassName) const {
        const ClassDescriptor* current = this;
        while (current) {
            if (current->className == targetClassName) return true;
            current = current->baseClass;
        }
        return false;
    }
};

struct PendingClassInfo {
    std::string className;
    std::string baseClassName;
};

class TypeRegistry {
public:
    static TypeRegistry& instance();

    ClassDescriptor& registerClass(const std::string& name);
    void deferBaseClass(const std::string& className, const std::string& baseName);
    void finalize();
    
    ClassDescriptor* find(const std::string& name);

private:
    TypeRegistry() = default;
    
    std::unordered_map<std::string, ClassDescriptor> classes;
    std::vector<PendingClassInfo> pending;
};

} // namespace Engine::Reflection
