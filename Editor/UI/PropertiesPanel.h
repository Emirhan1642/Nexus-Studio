#pragma once
#include <memory>
class Instance;
namespace Engine::Reflection {
    struct PropertyDescriptor;
}

class PropertiesPanel {
public:
    void draw();

private:
    void drawPropertyEditor(const std::shared_ptr<Instance>& inst, const Engine::Reflection::PropertyDescriptor* prop);
};
