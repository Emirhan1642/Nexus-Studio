#pragma once
#include "TypeRegistry.h"
#include <typeinfo>
#include <tuple>

namespace Engine::Reflection {

// Helper to expand arguments
template<typename T, typename Ret, typename... Args, size_t... Is>
std::any invokeWithUnpackedArgs(T* obj, Ret (T::*fn)(Args...), const std::vector<std::any>& args, std::index_sequence<Is...>) {
    if constexpr (std::is_void_v<Ret>) {
        (obj->*fn)(std::any_cast<Args>(args[Is])...);
        return std::any{};
    } else {
        return (obj->*fn)(std::any_cast<Args>(args[Is])...);
    }
}

template<typename T>
class ClassBuilder {
public:
    explicit ClassBuilder(const std::string& name) {
        descriptor = &TypeRegistry::instance().registerClass(name);
        descriptor->factory = []() -> void* { return new T(); };
    }

    ClassBuilder& base(const std::string& baseName) {
        TypeRegistry::instance().deferBaseClass(descriptor->className, baseName);
        return *this;
    }

    template<typename MemberT>
    ClassBuilder& property(const std::string& name, MemberT T::* member) {
        PropertyDescriptor desc;
        desc.name = name;
        desc.kind = PropertyDescriptor::Kind::Primitive;
        desc.typeName = typeid(MemberT).name(); // Simplified for now

        desc.getter = [member](void* instance) -> std::any {
            T* obj = static_cast<T*>(instance);
            return obj->*member;
        };
        desc.setter = [member](void* instance, const std::any& value) {
            T* obj = static_cast<T*>(instance);
            obj->*member = std::any_cast<MemberT>(value);
        };

        descriptor->properties.push_back(std::move(desc));
        return *this;
    }

    template<typename MemberT>
    ClassBuilder& propertyAccessor(const std::string& name, MemberT (T::*getter)() const, void (T::*setter)(const MemberT&)) {
        PropertyDescriptor desc;
        desc.name = name;
        desc.kind = PropertyDescriptor::Kind::Primitive;
        desc.typeName = typeid(MemberT).name(); // Simplified for now

        desc.getter = [getter](void* instance) -> std::any {
            T* obj = static_cast<T*>(instance);
            return (obj->*getter)();
        };
        desc.setter = [setter](void* instance, const std::any& value) {
            T* obj = static_cast<T*>(instance);
            (obj->*setter)(std::any_cast<MemberT>(value));
        };

        descriptor->properties.push_back(std::move(desc));
        return *this;
    }

    template<typename EnumT>
    ClassBuilder& enumProperty(const std::string& name, EnumT T::* member, const std::string& enumTypeName) {
        PropertyDescriptor desc;
        desc.name = name;
        desc.kind = PropertyDescriptor::Kind::Enum;
        desc.enumTypeName = enumTypeName;

        desc.getter = [member](void* instance) -> std::any {
            return static_cast<int>(static_cast<T*>(instance)->*member);
        };
        desc.setter = [member](void* instance, const std::any& value) {
            static_cast<T*>(instance)->*member = static_cast<EnumT>(std::any_cast<int>(value));
        };

        descriptor->properties.push_back(std::move(desc));
        return *this;
    }

    template<typename ElemT>
    ClassBuilder& arrayProperty(const std::string& name, std::vector<ElemT> T::* member) {
        PropertyDescriptor desc;
        desc.name = name;
        desc.kind = PropertyDescriptor::Kind::Array;

        desc.arraySize = [member](void* instance) -> size_t {
            return (static_cast<T*>(instance)->*member).size();
        };
        desc.arrayGet = [member](void* instance, size_t i) -> std::any {
            return (static_cast<T*>(instance)->*member)[i];
        };
        desc.arraySet = [member](void* instance, size_t i, const std::any& value) {
            (static_cast<T*>(instance)->*member)[i] = std::any_cast<ElemT>(value);
        };

        descriptor->properties.push_back(std::move(desc));
        return *this;
    }

    ClassBuilder& objectProperty(const std::string& name, std::weak_ptr<Instance> T::* member) {
        PropertyDescriptor desc;
        desc.name = name;
        desc.kind = PropertyDescriptor::Kind::ObjectRef;

        desc.objectGetter = [member](void* instance) -> std::shared_ptr<Instance> {
            return (static_cast<T*>(instance)->*member).lock(); 
        };
        desc.objectSetter = [member](void* instance, std::shared_ptr<Instance> value) {
            (static_cast<T*>(instance)->*member) = value;
        };

        descriptor->properties.push_back(std::move(desc));
        return *this;
    }

    ClassBuilder& category(const std::string& cat) {
        if (!descriptor->properties.empty())
            descriptor->properties.back().category = cat;
        return *this;
    }
    
    ClassBuilder& readOnly() {
        if (!descriptor->properties.empty())
            descriptor->properties.back().readOnly = true;
        return *this;
    }

    template<typename Ret, typename... Args>
    ClassBuilder& method(const std::string& name, Ret (T::*fn)(Args...)) {
        MethodDescriptor desc;
        desc.name = name;
        desc.invoke = [fn](void* instance, std::vector<std::any> args) -> std::any {
            T* obj = static_cast<T*>(instance);
            return invokeWithUnpackedArgs(obj, fn, args, std::index_sequence_for<Args...>{});
        };
        descriptor->methods.push_back(std::move(desc));
        return *this;
    }

private:
    ClassDescriptor* descriptor;
};

} // namespace Engine::Reflection
