#pragma once

#include "Command.h"
#include <memory>
#include <vector>
#include <algorithm>

// Restores parent relationships while retaining the affected instances. This
// makes Separate and Join reversible instead of only restoring the main mesh.
class InstanceHierarchyCommand final : public ICommand {
public:
    InstanceHierarchyCommand(std::vector<std::shared_ptr<Instance>> instances,
                             std::vector<std::shared_ptr<Instance>> beforeParents,
                             std::vector<std::shared_ptr<Instance>> afterParents)
        : m_instances(std::move(instances)),
          m_beforeParents(std::move(beforeParents)),
          m_afterParents(std::move(afterParents)) {}

    void execute() override { apply(m_afterParents); }
    void undo() override { apply(m_beforeParents); }

private:
    void apply(const std::vector<std::shared_ptr<Instance>>& parents) {
        const size_t count = std::min(m_instances.size(), parents.size());
        for (size_t i = 0; i < count; ++i) {
            if (m_instances[i]) m_instances[i]->setParent(parents[i]);
        }
    }

    std::vector<std::shared_ptr<Instance>> m_instances;
    std::vector<std::shared_ptr<Instance>> m_beforeParents;
    std::vector<std::shared_ptr<Instance>> m_afterParents;
};
