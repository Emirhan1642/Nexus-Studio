#include "MeshTopologyCommand.h"
#include "Editor/Modeling/ModelingContext.h"
#include <iostream>

MeshTopologyCommand::MeshTopologyCommand(
    std::shared_ptr<Part> part,
    std::shared_ptr<Engine::Geometry::EditableMesh> beforeState,
    std::shared_ptr<Engine::Geometry::EditableMesh> afterState,
    const std::vector<uint32_t>& beforeSelVerts,
    const std::vector<uint32_t>& beforeSelEdges,
    const std::vector<uint32_t>& beforeSelFaces,
    const std::vector<uint32_t>& afterSelVerts,
    const std::vector<uint32_t>& afterSelEdges,
    const std::vector<uint32_t>& afterSelFaces
) : m_part(part),
    m_beforeState(beforeState),
    m_afterState(afterState),
    m_beforeSelVerts(beforeSelVerts),
    m_beforeSelEdges(beforeSelEdges),
    m_beforeSelFaces(beforeSelFaces),
    m_afterSelVerts(afterSelVerts),
    m_afterSelEdges(afterSelEdges),
    m_afterSelFaces(afterSelFaces) {}

void MeshTopologyCommand::execute() {
    if (m_part && m_afterState) {
        auto cloned = m_afterState->clone();
        m_part->setEditableMesh(cloned);
        auto& ctx = Editor::Modeling::ModelingContext::instance();
        ctx.selectedVertices = m_afterSelVerts;
        ctx.selectedEdges = m_afterSelEdges;
        ctx.selectedFaces = m_afterSelFaces;
    }
}

void MeshTopologyCommand::undo() {
    if (m_part && m_beforeState) {
        auto cloned = m_beforeState->clone();
        m_part->setEditableMesh(cloned);
        auto& ctx = Editor::Modeling::ModelingContext::instance();
        ctx.selectedVertices = m_beforeSelVerts;
        ctx.selectedEdges = m_beforeSelEdges;
        ctx.selectedFaces = m_beforeSelFaces;
    }
}
