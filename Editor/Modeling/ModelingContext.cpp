#include "ModelingContext.h"
#include "Engine/Core/Geometry/UVUnwrapper.h"
#include <cmath>
#include <set>
#include <algorithm>

namespace Editor::Modeling {

void ModelingContext::clearSelection() {
    selectedVertices.clear();
    selectedEdges.clear();
    selectedFaces.clear();
}

void ModelingContext::selectAll(const std::shared_ptr<Part>& part, int mode) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    clearSelection();
    if (mode == 3) { // Face Mode
        for (size_t i = 0; i < mesh->getFaces().size(); ++i) {
            if (!mesh->getFaces()[i].deleted) selectedFaces.push_back(static_cast<uint32_t>(i));
        }
    } else if (mode == 2) { // Edge Mode
        for (size_t i = 0; i < mesh->getEdges().size(); ++i) {
            if (!mesh->getEdges()[i].deleted) selectedEdges.push_back(static_cast<uint32_t>(i));
        }
    } else if (mode == 1) { // Vertex Mode
        for (size_t i = 0; i < mesh->getVertices().size(); ++i) {
            if (!mesh->getVertices()[i].deleted) selectedVertices.push_back(static_cast<uint32_t>(i));
        }
    }
}

bool ModelingContext::startExtrude(std::shared_ptr<Part> part) {
    if (!part || selectedFaces.empty()) return false;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return false;

    activePart = part;
    preModalMesh = mesh->clone();
    baseSnapshotMesh = mesh->clone();
    opTargetFaces = selectedFaces;

    activeModal = ModalTool::Extrude;
    modalStartMouse = ImGui::GetMousePos();
    opDistance = 0.0f;
    lastOp = LastOpType::Extrude;

    return true;
}

bool ModelingContext::startInset(std::shared_ptr<Part> part) {
    if (!part || selectedFaces.empty()) return false;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return false;

    activePart = part;
    preModalMesh = mesh->clone();
    baseSnapshotMesh = mesh->clone();
    opTargetFaces = selectedFaces;

    activeModal = ModalTool::Inset;
    modalStartMouse = ImGui::GetMousePos();
    opThickness = 0.0f;
    opDepth = 0.0f;
    lastOp = LastOpType::Inset;

    return true;
}

bool ModelingContext::startBevel(std::shared_ptr<Part> part) {
    if (!part) return false;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return false;

    if (selectedFaces.empty() && selectedEdges.empty() && selectedVertices.empty()) return false;

    activePart = part;
    preModalMesh = mesh->clone();
    baseSnapshotMesh = mesh->clone();
    opTargetFaces = selectedFaces;
    opTargetEdges = selectedEdges;
    opTargetVertices = selectedVertices;

    activeModal = ModalTool::Bevel;
    modalStartMouse = ImGui::GetMousePos();
    opWidth = 0.05f;
    opSegments = 1;
    opProfile = 0.5f;
    opDepth = 0.0f;
    lastOp = LastOpType::Bevel;

    return true;
}

bool ModelingContext::startLoopCut(std::shared_ptr<Part> part) {
    if (!part) return false;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return false;

    activePart = part;
    preModalMesh = mesh->clone();
    baseSnapshotMesh = mesh->clone();
    previewLoopEdges.clear();

    activeModal = ModalTool::LoopCut;
    modalStartMouse = ImGui::GetMousePos();
    opSlide = 0.0f;
    opCuts = 1;
    lastOp = LastOpType::LoopCut;

    return true;
}

bool ModelingContext::startKnife(std::shared_ptr<Part> part) {
    if (!part) return false;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return false;

    activePart = part;
    preModalMesh = mesh->clone();
    baseSnapshotMesh = mesh->clone();
    knifePoints.clear();
    activeModal = ModalTool::Knife;

    return true;
}

void ModelingContext::updateModal(const ImVec2& currentMousePos, bool shiftHeld, bool ctrlHeld) {
    auto part = activePart.lock();
    if (!part || !baseSnapshotMesh || activeModal == ModalTool::None) return;

    float dx = (currentMousePos.x - modalStartMouse.x);
    float dy = (currentMousePos.y - modalStartMouse.y);
    float factor = shiftHeld ? 0.002f : 0.015f;

    if (activeModal == ModalTool::Extrude) {
        opDistance = (dx - dy) * factor;
        if (ctrlHeld) opDistance = std::round(opDistance * 4.0f) / 4.0f; // Snap to 0.25 increments

        auto workingMesh = baseSnapshotMesh->clone();
        auto newFaces = Engine::Geometry::MeshOperators::extrudeFaces(*workingMesh, opTargetFaces, opDistance);
        part->setEditableMesh(workingMesh);
    } else if (activeModal == ModalTool::Inset) {
        opThickness = std::max(0.0f, std::min(0.95f, (dx - dy) * factor));
        auto workingMesh = baseSnapshotMesh->clone();
        Engine::Geometry::MeshOperators::insetFaces(*workingMesh, opTargetFaces, opThickness, opDepth);
        part->setEditableMesh(workingMesh);
    } else if (activeModal == ModalTool::Bevel) {
        opWidth = std::max(0.001f, std::abs(dx - dy) * factor);
        auto workingMesh = baseSnapshotMesh->clone();
        if (!opTargetFaces.empty()) {
            Engine::Geometry::MeshOperators::bevelFaces(*workingMesh, opTargetFaces, opWidth, opSegments, opProfile, opDepth);
        } else if (!opTargetEdges.empty()) {
            Engine::Geometry::MeshOperators::bevelEdges(*workingMesh, opTargetEdges, opWidth, opSegments, opProfile);
        } else if (!opTargetVertices.empty()) {
            Engine::Geometry::MeshOperators::bevelVertices(*workingMesh, opTargetVertices, opWidth, opSegments);
        }
        part->setEditableMesh(workingMesh);
    } else if (activeModal == ModalTool::LoopCut) {
        opSlide = std::max(-0.90f, std::min(0.90f, dx * factor));
        if (!previewLoopEdges.empty()) {
            auto workingMesh = baseSnapshotMesh->clone();
            Engine::Geometry::MeshCutOperators::applyLoopCut(*workingMesh, previewLoopEdges, opSlide, opCuts);
            part->setEditableMesh(workingMesh);
        }
    }
}

void ModelingContext::confirmModal() {
    auto part = activePart.lock();
    if (part && preModalMesh) {
        if (activeModal == ModalTool::Knife && knifePoints.size() >= 2) {
            auto workingMesh = preModalMesh->clone();
            Engine::Geometry::MeshCutOperators::cutMeshWithKnifePolyline(*workingMesh, knifePoints, knifeTargetFaces, opCutThrough);
            part->setEditableMesh(workingMesh);
        } else if (activeModal == ModalTool::LoopCut && !previewLoopEdges.empty()) {
            opTargetEdges = previewLoopEdges;
            auto workingMesh = baseSnapshotMesh ? baseSnapshotMesh->clone() : preModalMesh->clone();
            Engine::Geometry::MeshCutOperators::applyLoopCut(*workingMesh, previewLoopEdges, opSlide, opCuts);
            part->setEditableMesh(workingMesh);
        }

        if (part->getEditableMesh()) {
            part->rebuildProceduralMesh();
            auto cmd = std::make_unique<MeshTopologyCommand>(part, preModalMesh, part->getEditableMesh()->clone());
            UndoStack::instance().push(std::move(cmd));
        }
    }
    activeModal = ModalTool::None;
    preModalMesh = nullptr;
    previewLoopEdges.clear();
    knifeTargetFaces.clear();
}

void ModelingContext::cancelModal() {
    auto part = activePart.lock();
    if (part && preModalMesh) {
        part->setEditableMesh(preModalMesh);
        part->rebuildProceduralMesh();
    }
    activeModal = ModalTool::None;
    preModalMesh = nullptr;
    previewLoopEdges.clear();
    knifeTargetFaces.clear();
}

void ModelingContext::reapplyLastOperation() {
    auto part = activePart.lock();
    if (!part || !baseSnapshotMesh) return;

    auto workingMesh = baseSnapshotMesh->clone();

    if (lastOp == LastOpType::Extrude) {
        Engine::Geometry::MeshOperators::extrudeFaces(*workingMesh, opTargetFaces, opDistance);
    } else if (lastOp == LastOpType::Inset) {
        Engine::Geometry::MeshOperators::insetFaces(*workingMesh, opTargetFaces, opThickness, opDepth);
    } else if (lastOp == LastOpType::Bevel) {
        if (!opTargetFaces.empty()) {
            Engine::Geometry::MeshOperators::bevelFaces(*workingMesh, opTargetFaces, opWidth, opSegments, opProfile, opDepth);
        } else if (!opTargetEdges.empty()) {
            Engine::Geometry::MeshOperators::bevelEdges(*workingMesh, opTargetEdges, opWidth, opSegments, opProfile);
        } else if (!opTargetVertices.empty()) {
            Engine::Geometry::MeshOperators::bevelVertices(*workingMesh, opTargetVertices, opWidth, opSegments);
        }
    } else if (lastOp == LastOpType::LoopCut) {
        if (!opTargetEdges.empty()) {
            Engine::Geometry::MeshCutOperators::applyLoopCut(*workingMesh, opTargetEdges, opSlide, opCuts);
        }
    } else if (lastOp == LastOpType::Subdivide) {
        Engine::Geometry::MeshOperators::subdivideFaces(*workingMesh, opTargetFaces, opCuts, opSmoothness);
    }

    part->setEditableMesh(workingMesh);
    part->rebuildProceduralMesh();
}

void ModelingContext::executeSubdivide(std::shared_ptr<Part> part, int cuts, float smoothness) {
    if (!part || selectedFaces.empty()) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    baseSnapshotMesh = mesh->clone();
    opTargetFaces = selectedFaces;
    opCuts = cuts;
    opSmoothness = smoothness;
    lastOp = LastOpType::Subdivide;
    activePart = part;

    Engine::Geometry::MeshOperators::subdivideFaces(*mesh, selectedFaces, cuts, smoothness);
    part->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone());
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeMerge(std::shared_ptr<Part> part, Engine::Geometry::MergeMode mode) {
    if (!part || selectedVertices.size() < 2) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    Engine::Geometry::MeshOperators::mergeVertices(*mesh, selectedVertices, mode);
    part->rebuildProceduralMesh();

    clearSelection();
    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone());
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeDelete(std::shared_ptr<Part> part, Engine::Geometry::SubElementType type) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    if (type == Engine::Geometry::SubElementType::Face && !selectedFaces.empty()) {
        Engine::Geometry::MeshOperators::deleteElements(*mesh, selectedFaces, type);
    } else if (type == Engine::Geometry::SubElementType::Edge && !selectedEdges.empty()) {
        Engine::Geometry::MeshOperators::deleteElements(*mesh, selectedEdges, type);
    } else if (type == Engine::Geometry::SubElementType::Vertex && !selectedVertices.empty()) {
        Engine::Geometry::MeshOperators::deleteElements(*mesh, selectedVertices, type);
    }
    part->rebuildProceduralMesh();

    clearSelection();
    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone());
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeDissolve(std::shared_ptr<Part> part, Engine::Geometry::SubElementType type) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    if (type == Engine::Geometry::SubElementType::Edge && !selectedEdges.empty()) {
        Engine::Geometry::MeshOperators::dissolveEdges(*mesh, selectedEdges);
    } else if (type == Engine::Geometry::SubElementType::Vertex && !selectedVertices.empty()) {
        Engine::Geometry::MeshOperators::dissolveVertices(*mesh, selectedVertices);
    }
    part->rebuildProceduralMesh();

    clearSelection();
    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone());
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeFill(std::shared_ptr<Part> part) {
    if (!part || selectedVertices.size() < 3) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    Engine::Geometry::MeshOperators::fillFace(*mesh, selectedVertices);
    part->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone());
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeMirror(std::shared_ptr<Part> part, Engine::Geometry::MirrorAxis axis) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    Engine::Geometry::MeshCutOperators::applyMirror(*mesh, axis, true, 0.01f);
    part->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone());
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeSmartUV(std::shared_ptr<Part> part) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    Engine::Geometry::UVUnwrapper::smartUVProject(*mesh);
    part->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone());
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeBoxUV(std::shared_ptr<Part> part) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    Engine::Geometry::UVUnwrapper::boxProject(*mesh, 1.0f);
    part->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone());
    UndoStack::instance().push(std::move(cmd));
}

} // namespace Editor::Modeling
