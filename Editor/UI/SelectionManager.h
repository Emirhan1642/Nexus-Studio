#pragma once
#include "Engine/Core/DataModel/Instance.h"
#include <memory>

class SelectionManager {
public:
    static SelectionManager& instance() {
        static SelectionManager s_instance;
        return s_instance;
    }

    void select(std::shared_ptr<Instance> inst) {
        m_selected = inst;
    }

    void clear() {
        m_selected.reset();
    }

    std::shared_ptr<Instance> getSelected() const {
        return m_selected;
    }

private:
    SelectionManager() = default;
    std::shared_ptr<Instance> m_selected;
};
