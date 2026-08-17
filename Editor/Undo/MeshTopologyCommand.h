#pragma once
#include "Command.h"
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Core/Geometry/EditableMesh.h"
#include <memory>
#include <vector>

class MeshTopologyCommand : public ICommand {
public:
    MeshTopologyCommand(
        std::shared_ptr<Part> part,
        std::shared_ptr<Engine::Geometry::EditableMesh> beforeState,
        std::shared_ptr<Engine::Geometry::EditableMesh> afterState,
        const std::vector<uint32_t>& beforeSelVerts = {},
        const std::vector<uint32_t>& beforeSelEdges = {},
        const std::vector<uint32_t>& beforeSelFaces = {},
        const std::vector<uint32_t>& afterSelVerts = {},
        const std::vector<uint32_t>& afterSelEdges = {},
        const std::vector<uint32_t>& afterSelFaces = {}
    );

    void execute() override;
    void undo() override;

    void updateAfterState(
        std::shared_ptr<Engine::Geometry::EditableMesh> afterState,
        const std::vector<uint32_t>& afterSelVerts = {},
        const std::vector<uint32_t>& afterSelEdges = {},
        const std::vector<uint32_t>& afterSelFaces = {}
    );

    bool targetsPart(const std::shared_ptr<Part>& part) const { return m_part == part; }

private:
    std::shared_ptr<Part> m_part;
    std::shared_ptr<Engine::Geometry::EditableMesh> m_beforeState;
    std::shared_ptr<Engine::Geometry::EditableMesh> m_afterState;
    std::vector<uint32_t> m_beforeSelVerts;
    std::vector<uint32_t> m_beforeSelEdges;
    std::vector<uint32_t> m_beforeSelFaces;
    std::vector<uint32_t> m_afterSelVerts;
    std::vector<uint32_t> m_afterSelEdges;
    std::vector<uint32_t> m_afterSelFaces;
};
