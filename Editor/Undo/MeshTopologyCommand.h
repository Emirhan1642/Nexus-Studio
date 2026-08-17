#pragma once
#include "Command.h"
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Core/Geometry/EditableMesh.h"
#include <memory>

class MeshTopologyCommand : public ICommand {
public:
    MeshTopologyCommand(
        std::shared_ptr<Part> part,
        std::shared_ptr<Engine::Geometry::EditableMesh> beforeState,
        std::shared_ptr<Engine::Geometry::EditableMesh> afterState
    ) : m_part(part), m_beforeState(beforeState), m_afterState(afterState) {}

    void execute() override {
        if (m_part && m_afterState) {
            m_part->setEditableMesh(m_afterState->clone());
        }
    }

    void undo() override {
        if (m_part && m_beforeState) {
            m_part->setEditableMesh(m_beforeState->clone());
        }
    }

private:
    std::shared_ptr<Part> m_part;
    std::shared_ptr<Engine::Geometry::EditableMesh> m_beforeState;
    std::shared_ptr<Engine::Geometry::EditableMesh> m_afterState;
};
