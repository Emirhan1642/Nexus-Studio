#pragma once
#include "Engine/Core/DataModel/Instance.h"
#include <memory>
#include <vector>
#include <algorithm>

class SelectionManager {
public:
    static SelectionManager& instance() {
        static SelectionManager s_instance;
        return s_instance;
    }

    void select(std::shared_ptr<Instance> inst) {
        m_selected = inst;
        m_selectedList.clear();
        if (inst) m_selectedList.push_back(inst);
    }

    void toggleSelect(std::shared_ptr<Instance> inst) {
        if (!inst) return;
        auto it = std::find(m_selectedList.begin(), m_selectedList.end(), inst);
        if (it != m_selectedList.end()) {
            m_selectedList.erase(it);
            m_selected = m_selectedList.empty() ? nullptr : m_selectedList.back();
        } else {
            m_selectedList.push_back(inst);
            m_selected = inst;
        }
    }

    void addToSelection(std::shared_ptr<Instance> inst) {
        if (!inst) return;
        if (!isSelected(inst)) {
            m_selectedList.push_back(inst);
            m_selected = inst;
        }
    }

    bool isSelected(const std::shared_ptr<Instance>& inst) const {
        if (!inst) return false;
        if (m_selected == inst) return true;
        return std::find(m_selectedList.begin(), m_selectedList.end(), inst) != m_selectedList.end();
    }

    void clear() {
        m_selected.reset();
        m_selectedList.clear();
    }

    std::shared_ptr<Instance> getSelected() const {
        return m_selected;
    }

    const std::vector<std::shared_ptr<Instance>>& getSelectionList() const {
        return m_selectedList;
    }

private:
    SelectionManager() = default;
    std::shared_ptr<Instance> m_selected;
    std::vector<std::shared_ptr<Instance>> m_selectedList;
};
