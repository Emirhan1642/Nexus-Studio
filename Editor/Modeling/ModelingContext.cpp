#include "ModelingContext.h"
#include "Engine/Core/Geometry/UVUnwrapper.h"
#include <cmath>
#include <set>
#include <map>
#include <algorithm>
#include <utility>

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
    if (!part) return false;
    if (selectedFaces.empty() && selectedEdges.empty() && selectedVertices.empty()) return false;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return false;

    activePart = part;
    preModalMesh = mesh->clone();
    baseSnapshotMesh = mesh->clone();
    opTargetFaces = selectedFaces;
    opTargetEdges = selectedEdges;
    opTargetVertices = selectedVertices;

    activeModal = ModalTool::Extrude;
    modalStartMouseX = 0.0f;
    modalStartMouseY = 0.0f;
    modalStarted = false;
    lastCalculatedParam = -999999.0f;
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
    modalStartMouseX = 0.0f;
    modalStartMouseY = 0.0f;
    modalStarted = false;
    lastCalculatedParam = -999999.0f;
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
    modalStartMouseX = 0.0f;
    modalStartMouseY = 0.0f;
    modalStarted = false;
    lastCalculatedParam = -999999.0f;
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
    modalStartMouseX = 0.0f;
    modalStartMouseY = 0.0f;
    modalStarted = false;
    lastCalculatedParam = -999999.0f;
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
    modalStarted = false;
    lastCalculatedParam = -999999.0f;
    modalNumericBuffer = "";
    modalHasNumericInput = false;
    activeModal = ModalTool::Knife;

    return true;
}

bool ModelingContext::startShrinkFatten(std::shared_ptr<Part> part) {
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

    std::set<uint32_t> verts;
    if (!selectedVertices.empty()) {
        verts.insert(selectedVertices.begin(), selectedVertices.end());
    }
    if (!selectedEdges.empty()) {
        const auto& edges = mesh->getEdges();
        for (uint32_t e : selectedEdges) {
            if (e < edges.size() && !edges[e].deleted) {
                verts.insert(edges[e].v0);
                verts.insert(edges[e].v1);
            }
        }
    }
    if (!selectedFaces.empty()) {
        const auto& faces = mesh->getFaces();
        for (uint32_t f : selectedFaces) {
            if (f < faces.size() && !faces[f].deleted) {
                verts.insert(faces[f].vertices.begin(), faces[f].vertices.end());
            }
        }
    }
    opTargetVertices.assign(verts.begin(), verts.end());

    activeModal = ModalTool::ShrinkFatten;
    modalStartMouseX = 0.0f;
    modalStartMouseY = 0.0f;
    modalStarted = false;
    lastCalculatedParam = -999999.0f;
    modalNumericBuffer = "";
    modalHasNumericInput = false;
    opDistance = 0.0f;
    lastOp = LastOpType::ShrinkFatten;

    return true;
}

bool ModelingContext::startEdgeSlide(std::shared_ptr<Part> part) {
    if (!part || (selectedVertices.empty() && selectedEdges.empty())) return false;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return false;

    activePart = part;
    preModalMesh = mesh->clone();
    baseSnapshotMesh = mesh->clone();
    opTargetEdges = selectedEdges;
    opTargetVertices = selectedVertices;

    if (opTargetVertices.empty() && !selectedEdges.empty()) {
        std::set<uint32_t> verts;
        const auto& edges = mesh->getEdges();
        for (uint32_t e : selectedEdges) {
            if (e < edges.size() && !edges[e].deleted) {
                verts.insert(edges[e].v0);
                verts.insert(edges[e].v1);
            }
        }
        opTargetVertices.assign(verts.begin(), verts.end());
    }

    activeModal = ModalTool::EdgeSlide;
    modalStartMouseX = 0.0f;
    modalStartMouseY = 0.0f;
    modalStarted = false;
    lastCalculatedParam = -999999.0f;
    modalNumericBuffer = "";
    modalHasNumericInput = false;
    opSlide = 0.0f;
    lastOp = LastOpType::EdgeSlide;

    return true;
}

void ModelingContext::updateModal(float mouseX, float mouseY, bool shiftHeld, bool ctrlHeld) {
    auto part = activePart.lock();
    if (!part || !baseSnapshotMesh || activeModal == ModalTool::None) return;

    if (!modalStarted) {
        modalStartMouseX = mouseX;
        modalStartMouseY = mouseY;
        modalStarted = true;
    }

    if (activeModal == ModalTool::Knife) {
        return; // Knife interacts via mouse clicks, not drag
    }

    float dx = (mouseX - modalStartMouseX);
    float dy = (mouseY - modalStartMouseY);
    float factor = shiftHeld ? 0.002f : 0.015f;

    if (activeModal == ModalTool::Extrude) {
        float newDistance = (dx - dy) * factor;
        if (ctrlHeld) newDistance = std::round(newDistance * 4.0f) / 4.0f; // Snap to 0.25 increments

        if (std::abs(newDistance - opDistance) < 1e-5f && lastCalculatedParam != -999999.0f) {
            return;
        }
        opDistance = newDistance;
        lastCalculatedParam = opDistance;

        auto workingMesh = baseSnapshotMesh->clone();
        if (!opTargetFaces.empty()) {
            Engine::Geometry::MeshOperators::extrudeFaces(*workingMesh, opTargetFaces, opDistance, {0, 0, 0}, opIndividual);
        } else if (!opTargetEdges.empty()) {
            Engine::Geometry::MeshOperators::extrudeEdges(*workingMesh, opTargetEdges, opDistance);
        } else if (!opTargetVertices.empty()) {
            Engine::Geometry::MeshOperators::extrudeVertices(*workingMesh, opTargetVertices, opDistance);
        }
        part->setEditableMesh(workingMesh);
    } else if (activeModal == ModalTool::Inset) {
        float newThickness = std::max(0.0f, std::min(0.95f, (dx - dy) * factor));
        if (std::abs(newThickness - opThickness) < 1e-5f && lastCalculatedParam != -999999.0f) {
            return;
        }
        opThickness = newThickness;
        lastCalculatedParam = opThickness;

        auto workingMesh = baseSnapshotMesh->clone();
        Engine::Geometry::MeshOperators::insetFaces(*workingMesh, opTargetFaces, opThickness, opDepth, opIndividual);
        part->setEditableMesh(workingMesh);
    } else if (activeModal == ModalTool::Bevel) {
        float newWidth = std::max(0.001f, std::abs(dx - dy) * factor);
        if (std::abs(newWidth - opWidth) < 1e-5f && lastCalculatedParam != -999999.0f) {
            return;
        }
        opWidth = newWidth;
        lastCalculatedParam = opWidth;

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
        float newSlide = std::max(-0.90f, std::min(0.90f, dx * factor));
        if (std::abs(newSlide - opSlide) < 1e-5f && lastCalculatedParam != -999999.0f) {
            return;
        }
        opSlide = newSlide;
        lastCalculatedParam = opSlide;

        if (!previewLoopEdges.empty()) {
            auto workingMesh = baseSnapshotMesh->clone();
            Engine::Geometry::MeshCutOperators::applyLoopCut(*workingMesh, previewLoopEdges, opSlide, opCuts);
            part->setEditableMesh(workingMesh);
        }
    } else if (activeModal == ModalTool::ShrinkFatten) {
        float newDistance = (dx - dy) * factor;
        if (std::abs(newDistance - opDistance) < 1e-5f && lastCalculatedParam != -999999.0f) {
            return;
        }
        opDistance = newDistance;
        lastCalculatedParam = opDistance;

        auto workingMesh = baseSnapshotMesh->clone();
        Engine::Geometry::MeshOperators::shrinkFatten(*workingMesh, opTargetVertices, opDistance);
        part->setEditableMesh(workingMesh);
    } else if (activeModal == ModalTool::EdgeSlide) {
        float newSlide = std::max(-1.0f, std::min(1.0f, dx * factor));
        if (std::abs(newSlide - opSlide) < 1e-5f && lastCalculatedParam != -999999.0f) {
            return;
        }
        opSlide = newSlide;
        lastCalculatedParam = opSlide;

        auto workingMesh = baseSnapshotMesh->clone();
        Engine::Geometry::MeshOperators::slideVertices(*workingMesh, opTargetVertices, opSlide);
        part->setEditableMesh(workingMesh);
    }
}

void ModelingContext::confirmModal() {
    auto part = activePart.lock();
    if (part && preModalMesh) {
        if (activeModal == ModalTool::Knife && knifePoints.size() >= 2) {
            auto workingMesh = preModalMesh->clone();
            Engine::Geometry::MeshCutOperators::cutMeshWithKnifePolyline(*workingMesh, knifePoints, knifeTargetFaces, opCutThrough);
            part->setEditableMesh(workingMesh);
            clearSelection();
        } else if (activeModal == ModalTool::LoopCut && !previewLoopEdges.empty()) {
            opTargetEdges = previewLoopEdges;
            auto workingMesh = baseSnapshotMesh ? baseSnapshotMesh->clone() : preModalMesh->clone();
            auto newEdges = Engine::Geometry::MeshCutOperators::applyLoopCut(*workingMesh, previewLoopEdges, opSlide, opCuts);
            part->setEditableMesh(workingMesh);
            selectedVertices.clear();
            selectedFaces.clear();
            selectedEdges = std::move(newEdges);
        } else if (activeModal == ModalTool::Extrude) {
            auto workingMesh = baseSnapshotMesh->clone();
            selectedVertices.clear();
            selectedEdges.clear();
            selectedFaces.clear();
            if (!opTargetFaces.empty()) {
                selectedFaces = Engine::Geometry::MeshOperators::extrudeFaces(*workingMesh, opTargetFaces, opDistance, {0, 0, 0}, opIndividual);
            } else if (!opTargetEdges.empty()) {
                selectedFaces = Engine::Geometry::MeshOperators::extrudeEdges(*workingMesh, opTargetEdges, opDistance);
            } else if (!opTargetVertices.empty()) {
                selectedVertices = Engine::Geometry::MeshOperators::extrudeVertices(*workingMesh, opTargetVertices, opDistance);
            }
            part->setEditableMesh(workingMesh);
        } else if (activeModal == ModalTool::Inset) {
            auto workingMesh = baseSnapshotMesh->clone();
            auto newFaces = Engine::Geometry::MeshOperators::insetFaces(*workingMesh, opTargetFaces, opThickness, opDepth, opIndividual);
            part->setEditableMesh(workingMesh);
            selectedVertices.clear();
            selectedEdges.clear();
            selectedFaces = std::move(newFaces);
        } else if (activeModal == ModalTool::Bevel) {
            auto workingMesh = baseSnapshotMesh->clone();
            selectedVertices.clear();
            selectedEdges.clear();
            selectedFaces.clear();
            if (!opTargetFaces.empty()) {
                selectedFaces = Engine::Geometry::MeshOperators::bevelFaces(
                    *workingMesh, opTargetFaces, opWidth, opSegments, opProfile, opDepth);
            } else if (!opTargetEdges.empty()) {
                selectedEdges = Engine::Geometry::MeshOperators::bevelEdges(
                    *workingMesh, opTargetEdges, opWidth, opSegments, opProfile);
            } else if (!opTargetVertices.empty()) {
                selectedVertices = Engine::Geometry::MeshOperators::bevelVertices(
                    *workingMesh, opTargetVertices, opWidth, opSegments);
            }
            part->setEditableMesh(workingMesh);
        } else if (activeModal == ModalTool::ShrinkFatten) {
            auto workingMesh = baseSnapshotMesh->clone();
            Engine::Geometry::MeshOperators::shrinkFatten(*workingMesh, opTargetVertices, opDistance);
            part->setEditableMesh(workingMesh);
        } else if (activeModal == ModalTool::EdgeSlide) {
            auto workingMesh = baseSnapshotMesh->clone();
            Engine::Geometry::MeshOperators::slideVertices(*workingMesh, opTargetVertices, opSlide);
            part->setEditableMesh(workingMesh);
        }

        if (part->getEditableMesh()) {
            part->rebuildProceduralMesh();
            auto cmd = std::make_unique<MeshTopologyCommand>(
                part, preModalMesh, part->getEditableMesh()->clone(),
                opTargetVertices, opTargetEdges, opTargetFaces,
                selectedVertices, selectedEdges, selectedFaces
            );
            UndoStack::instance().push(std::move(cmd));
        }
    }
    activeModal = ModalTool::None;
    modalStarted = false;
    lastCalculatedParam = -999999.0f;
    modalNumericBuffer = "";
    modalHasNumericInput = false;
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
    modalStarted = false;
    lastCalculatedParam = -999999.0f;
    modalNumericBuffer = "";
    modalHasNumericInput = false;
    preModalMesh = nullptr;
    previewLoopEdges.clear();
    knifeTargetFaces.clear();
}

void ModelingContext::reapplyLastOperation() {
    auto part = activePart.lock();
    if (!part || !baseSnapshotMesh) return;

    auto workingMesh = baseSnapshotMesh->clone();

    if (lastOp == LastOpType::Extrude) {
        selectedVertices.clear();
        selectedEdges.clear();
        selectedFaces.clear();
        if (!opTargetFaces.empty()) {
            selectedFaces = Engine::Geometry::MeshOperators::extrudeFaces(*workingMesh, opTargetFaces, opDistance, {0, 0, 0}, opIndividual);
        } else if (!opTargetEdges.empty()) {
            selectedFaces = Engine::Geometry::MeshOperators::extrudeEdges(*workingMesh, opTargetEdges, opDistance);
        } else if (!opTargetVertices.empty()) {
            selectedVertices = Engine::Geometry::MeshOperators::extrudeVertices(*workingMesh, opTargetVertices, opDistance);
        }
    } else if (lastOp == LastOpType::Inset) {
        opThickness = std::max(0.0f, opThickness);
        selectedVertices.clear();
        selectedEdges.clear();
        selectedFaces = Engine::Geometry::MeshOperators::insetFaces(*workingMesh, opTargetFaces, opThickness, opDepth, opIndividual);
    } else if (lastOp == LastOpType::Bevel) {
        opWidth = std::max(0.0001f, opWidth);
        opSegments = std::clamp(opSegments, 1, 8);
        opProfile = std::clamp(opProfile, 0.0f, 1.0f);
        selectedVertices.clear();
        selectedEdges.clear();
        selectedFaces.clear();
        if (!opTargetFaces.empty()) {
            selectedFaces = Engine::Geometry::MeshOperators::bevelFaces(*workingMesh, opTargetFaces, opWidth, opSegments, opProfile, opDepth);
        } else if (!opTargetEdges.empty()) {
            selectedEdges = Engine::Geometry::MeshOperators::bevelEdges(*workingMesh, opTargetEdges, opWidth, opSegments, opProfile);
        } else if (!opTargetVertices.empty()) {
            selectedVertices = Engine::Geometry::MeshOperators::bevelVertices(*workingMesh, opTargetVertices, opWidth, opSegments);
        }
    } else if (lastOp == LastOpType::LoopCut) {
        opSlide = std::clamp(opSlide, -0.90f, 0.90f);
        opCuts = std::clamp(opCuts, 1, 6);
        if (!opTargetEdges.empty()) {
            selectedVertices.clear();
            selectedFaces.clear();
            selectedEdges = Engine::Geometry::MeshCutOperators::applyLoopCut(*workingMesh, opTargetEdges, opSlide, opCuts);
        }
    } else if (lastOp == LastOpType::Subdivide) {
        opCuts = std::clamp(opCuts, 1, 4);
        opSmoothness = std::clamp(opSmoothness, 0.0f, 1.0f);
        selectedVertices.clear();
        selectedEdges.clear();
        selectedFaces = Engine::Geometry::MeshOperators::subdivideFaces(*workingMesh, opTargetFaces, opCuts, opSmoothness);
    } else if (lastOp == LastOpType::ShrinkFatten) {
        Engine::Geometry::MeshOperators::shrinkFatten(*workingMesh, opTargetVertices, opDistance);
    } else if (lastOp == LastOpType::EdgeSlide) {
        opSlide = std::clamp(opSlide, -1.0f, 1.0f);
        Engine::Geometry::MeshOperators::slideVertices(*workingMesh, opTargetVertices, opSlide);
    }

    part->setEditableMesh(workingMesh);
    part->rebuildProceduralMesh();

    auto cmd = dynamic_cast<MeshTopologyCommand*>(UndoStack::instance().getTopUndoCommand());
    if (cmd && cmd->targetsPart(part)) {
        cmd->updateAfterState(workingMesh->clone(), selectedVertices, selectedEdges, selectedFaces);
    }
}

void ModelingContext::executeSubdivide(std::shared_ptr<Part> part, int cuts, float smoothness) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    if (selectedFaces.empty()) {
        for (size_t f = 0; f < mesh->getFaces().size(); ++f) {
            if (!mesh->getFaces()[f].deleted && mesh->getFaces()[f].vertices.size() >= 3) {
                selectedFaces.push_back(static_cast<uint32_t>(f));
            }
        }
    }
    if (selectedFaces.empty()) return;

    auto before = mesh->clone();
    auto beforeVerts = selectedVertices;
    auto beforeEdges = selectedEdges;
    auto beforeFaces = selectedFaces;

    baseSnapshotMesh = mesh->clone();
    opTargetFaces = selectedFaces;
    opCuts = cuts;
    opSmoothness = smoothness;
    lastOp = LastOpType::Subdivide;
    activePart = part;

    selectedFaces = Engine::Geometry::MeshOperators::subdivideFaces(*mesh, selectedFaces, cuts, smoothness);
    part->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(
        part, before, mesh->clone(),
        beforeVerts, beforeEdges, beforeFaces,
        selectedVertices, selectedEdges, selectedFaces
    );
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeMerge(std::shared_ptr<Part> part, Engine::Geometry::MergeMode mode, const Engine::Math::Vector3& targetPos) {
    if (!part || selectedVertices.size() < 2) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    auto beforeVerts = selectedVertices;
    auto beforeEdges = selectedEdges;
    auto beforeFaces = selectedFaces;

    Engine::Geometry::MeshOperators::mergeVertices(*mesh, selectedVertices, mode, targetPos);
    part->rebuildProceduralMesh();
    clearSelection();

    auto cmd = std::make_unique<MeshTopologyCommand>(
        part, before, mesh->clone(),
        beforeVerts, beforeEdges, beforeFaces,
        selectedVertices, selectedEdges, selectedFaces
    );
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeBisect(std::shared_ptr<Part> part, const Engine::Math::Vector3& point, const Engine::Math::Vector3& normal, bool clearInner, bool clearOuter, bool fillCut) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    auto beforeVerts = selectedVertices;
    auto beforeEdges = selectedEdges;
    auto beforeFaces = selectedFaces;

    Engine::Geometry::MeshOperators::bisectPlane(*mesh, point, normal, clearInner, clearOuter, fillCut);
    part->rebuildProceduralMesh();
    clearSelection();

    auto cmd = std::make_unique<MeshTopologyCommand>(
        part, before, mesh->clone(),
        beforeVerts, beforeEdges, beforeFaces,
        selectedVertices, selectedEdges, selectedFaces
    );
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeDelete(std::shared_ptr<Part> part, Engine::Geometry::SubElementType type) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    auto beforeVerts = selectedVertices;
    auto beforeEdges = selectedEdges;
    auto beforeFaces = selectedFaces;

    if (type == Engine::Geometry::SubElementType::Face && !selectedFaces.empty()) {
        Engine::Geometry::MeshOperators::deleteElements(*mesh, selectedFaces, type);
    } else if (type == Engine::Geometry::SubElementType::Edge && !selectedEdges.empty()) {
        Engine::Geometry::MeshOperators::deleteElements(*mesh, selectedEdges, type);
    } else if (type == Engine::Geometry::SubElementType::Vertex && !selectedVertices.empty()) {
        Engine::Geometry::MeshOperators::deleteElements(*mesh, selectedVertices, type);
    }
    part->rebuildProceduralMesh();
    clearSelection();

    auto cmd = std::make_unique<MeshTopologyCommand>(
        part, before, mesh->clone(),
        beforeVerts, beforeEdges, beforeFaces,
        selectedVertices, selectedEdges, selectedFaces
    );
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeDissolve(std::shared_ptr<Part> part, Engine::Geometry::SubElementType type) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    auto beforeVerts = selectedVertices;
    auto beforeEdges = selectedEdges;
    auto beforeFaces = selectedFaces;

    if (type == Engine::Geometry::SubElementType::Edge && !selectedEdges.empty()) {
        Engine::Geometry::MeshOperators::dissolveEdges(*mesh, selectedEdges);
    } else if (type == Engine::Geometry::SubElementType::Vertex && !selectedVertices.empty()) {
        Engine::Geometry::MeshOperators::dissolveVertices(*mesh, selectedVertices);
    }
    part->rebuildProceduralMesh();
    clearSelection();

    auto cmd = std::make_unique<MeshTopologyCommand>(
        part, before, mesh->clone(),
        beforeVerts, beforeEdges, beforeFaces,
        selectedVertices, selectedEdges, selectedFaces
    );
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

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone(),
        selectedVertices, selectedEdges, selectedFaces,
        selectedVertices, selectedEdges, selectedFaces);
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

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone(),
        selectedVertices, selectedEdges, selectedFaces,
        selectedVertices, selectedEdges, selectedFaces);
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

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone(),
        selectedVertices, selectedEdges, selectedFaces,
        selectedVertices, selectedEdges, selectedFaces);
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

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone(),
        selectedVertices, selectedEdges, selectedFaces,
        selectedVertices, selectedEdges, selectedFaces);
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executePoke(std::shared_ptr<Part> part, float offset) {
    if (!part || selectedFaces.empty()) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    Engine::Geometry::MeshOperators::pokeFaces(*mesh, selectedFaces, offset);
    part->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone(),
        selectedVertices, selectedEdges, selectedFaces,
        selectedVertices, selectedEdges, selectedFaces);
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeTriangulate(std::shared_ptr<Part> part) {
    if (!part || selectedFaces.empty()) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    Engine::Geometry::MeshOperators::triangulateFaces(*mesh, selectedFaces);
    part->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone(),
        selectedVertices, selectedEdges, selectedFaces,
        selectedVertices, selectedEdges, selectedFaces);
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeTrisToQuads(std::shared_ptr<Part> part) {
    if (!part || selectedFaces.empty()) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    Engine::Geometry::MeshOperators::trisToQuads(*mesh, selectedFaces);
    part->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone(),
        selectedVertices, selectedEdges, selectedFaces,
        selectedVertices, selectedEdges, selectedFaces);
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeFlipNormals(std::shared_ptr<Part> part) {
    if (!part || selectedFaces.empty()) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    Engine::Geometry::MeshOperators::flipNormals(*mesh, selectedFaces);
    part->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone(),
        selectedVertices, selectedEdges, selectedFaces,
        selectedVertices, selectedEdges, selectedFaces);
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeEdgeSplit(std::shared_ptr<Part> part) {
    if (!part || selectedEdges.empty()) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    Engine::Geometry::MeshOperators::edgeSplit(*mesh, selectedEdges);
    part->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone(),
        selectedVertices, selectedEdges, selectedFaces,
        selectedVertices, selectedEdges, selectedFaces);
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeWeldByDistance(std::shared_ptr<Part> part, float threshold) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    Engine::Geometry::MeshOperators::weldVerticesByDistance(*mesh, threshold);
    part->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone(),
        selectedVertices, selectedEdges, selectedFaces,
        selectedVertices, selectedEdges, selectedFaces);
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::selectLinked(const std::shared_ptr<Part>& part, int mode) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto& faces = mesh->getFaces();
    auto& vertices = mesh->getVertices();
    std::set<uint32_t> visitedVerts;
    std::vector<uint32_t> queue;

    if (mode == 3 && !selectedFaces.empty()) {
        for (uint32_t f : selectedFaces) {
            if (f < faces.size()) {
                for (uint32_t v : faces[f].vertices) {
                    if (visitedVerts.insert(v).second) queue.push_back(v);
                }
            }
        }
    } else if (mode == 2 && !selectedEdges.empty()) {
        const auto& edges = mesh->getEdges();
        for (uint32_t e : selectedEdges) {
            if (e < edges.size()) {
                if (visitedVerts.insert(edges[e].v0).second) queue.push_back(edges[e].v0);
                if (visitedVerts.insert(edges[e].v1).second) queue.push_back(edges[e].v1);
            }
        }
    } else if (mode == 1 && !selectedVertices.empty()) {
        for (uint32_t v : selectedVertices) {
            if (visitedVerts.insert(v).second) queue.push_back(v);
        }
    }

    // BFS through connected vertices
    while (!queue.empty()) {
        uint32_t curV = queue.back();
        queue.pop_back();
        auto adj = mesh->getAdjacentVertices(curV);
        for (uint32_t nxt : adj) {
            if (nxt < vertices.size() && !vertices[nxt].deleted && visitedVerts.insert(nxt).second) {
                queue.push_back(nxt);
            }
        }
    }

    clearSelection();
    if (mode == 3) {
        for (size_t f = 0; f < faces.size(); ++f) {
            if (faces[f].deleted) continue;
            bool allIn = true;
            for (uint32_t v : faces[f].vertices) {
                if (!visitedVerts.count(v)) { allIn = false; break; }
            }
            if (allIn) selectedFaces.push_back(static_cast<uint32_t>(f));
        }
    } else if (mode == 2) {
        const auto& edges = mesh->getEdges();
        for (size_t e = 0; e < edges.size(); ++e) {
            if (!edges[e].deleted && visitedVerts.count(edges[e].v0) && visitedVerts.count(edges[e].v1)) {
                selectedEdges.push_back(static_cast<uint32_t>(e));
            }
        }
    } else {
        selectedVertices.assign(visitedVerts.begin(), visitedVerts.end());
    }
}

void ModelingContext::selectMore(const std::shared_ptr<Part>& part, int mode) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto& faces = mesh->getFaces();
    auto& vertices = mesh->getVertices();

    if (mode == 3) {
        std::set<uint32_t> selVerts;
        for (uint32_t f : selectedFaces) {
            if (f < faces.size()) {
                for (uint32_t v : faces[f].vertices) selVerts.insert(v);
            }
        }
        std::set<uint32_t> newFaces(selectedFaces.begin(), selectedFaces.end());
        for (size_t f = 0; f < faces.size(); ++f) {
            if (faces[f].deleted) continue;
            for (uint32_t v : faces[f].vertices) {
                if (selVerts.count(v)) { newFaces.insert(static_cast<uint32_t>(f)); break; }
            }
        }
        selectedFaces.assign(newFaces.begin(), newFaces.end());
    } else if (mode == 2) {
        const auto& edges = mesh->getEdges();
        std::set<uint32_t> selVerts;
        for (uint32_t e : selectedEdges) {
            if (e < edges.size() && !edges[e].deleted) {
                selVerts.insert(edges[e].v0);
                selVerts.insert(edges[e].v1);
            }
        }
        std::set<uint32_t> newEdges(selectedEdges.begin(), selectedEdges.end());
        for (size_t e = 0; e < edges.size(); ++e) {
            if (edges[e].deleted) continue;
            if (selVerts.count(edges[e].v0) || selVerts.count(edges[e].v1)) {
                newEdges.insert(static_cast<uint32_t>(e));
            }
        }
        selectedEdges.assign(newEdges.begin(), newEdges.end());
    } else if (mode == 1) {
        std::set<uint32_t> newVerts(selectedVertices.begin(), selectedVertices.end());
        for (uint32_t v : selectedVertices) {
            auto adj = mesh->getAdjacentVertices(v);
            for (uint32_t nxt : adj) {
                if (nxt < vertices.size() && !vertices[nxt].deleted) newVerts.insert(nxt);
            }
        }
        selectedVertices.assign(newVerts.begin(), newVerts.end());
    }
}

void ModelingContext::selectLess(const std::shared_ptr<Part>& part, int mode) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto& faces = mesh->getFaces();
    auto& vertices = mesh->getVertices();

    if (mode == 3) {
        std::set<uint32_t> selFaceSet(selectedFaces.begin(), selectedFaces.end());
        std::vector<uint32_t> keptFaces;
        for (uint32_t f : selectedFaces) {
            if (f >= faces.size() || faces[f].deleted) continue;
            bool isBoundary = false;
            for (uint32_t v : faces[f].vertices) {
                auto conn = mesh->getConnectedFaces(v);
                for (uint32_t cf : conn) {
                    if (!selFaceSet.count(cf)) { isBoundary = true; break; }
                }
                if (isBoundary) break;
            }
            if (!isBoundary) keptFaces.push_back(f);
        }
        selectedFaces = keptFaces;
    } else if (mode == 2) {
        const auto& edges = mesh->getEdges();
        std::set<uint32_t> selEdgeSet(selectedEdges.begin(), selectedEdges.end());
        std::vector<uint32_t> keptEdges;
        for (uint32_t e : selectedEdges) {
            if (e >= edges.size() || edges[e].deleted) continue;
            uint32_t v0 = edges[e].v0;
            uint32_t v1 = edges[e].v1;
            auto conn0 = mesh->getConnectedEdges(v0);
            auto conn1 = mesh->getConnectedEdges(v1);
            bool isBoundary = false;
            for (uint32_t ce : conn0) {
                if (!selEdgeSet.count(ce)) { isBoundary = true; break; }
            }
            if (!isBoundary) {
                for (uint32_t ce : conn1) {
                    if (!selEdgeSet.count(ce)) { isBoundary = true; break; }
                }
            }
            if (!isBoundary) keptEdges.push_back(e);
        }
        selectedEdges = keptEdges;
    } else if (mode == 1) {
        std::set<uint32_t> selVertSet(selectedVertices.begin(), selectedVertices.end());
        std::vector<uint32_t> keptVerts;
        for (uint32_t v : selectedVertices) {
            if (v >= vertices.size() || vertices[v].deleted) continue;
            auto adj = mesh->getAdjacentVertices(v);
            bool isBoundary = false;
            for (uint32_t nxt : adj) {
                if (!selVertSet.count(nxt)) { isBoundary = true; break; }
            }
            if (!isBoundary) keptVerts.push_back(v);
        }
        selectedVertices = keptVerts;
    }
}

void ModelingContext::selectInvert(const std::shared_ptr<Part>& part, int mode) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    if (mode == 3) {
        std::set<uint32_t> current(selectedFaces.begin(), selectedFaces.end());
        selectedFaces.clear();
        for (size_t f = 0; f < mesh->getFaces().size(); ++f) {
            if (!mesh->getFaces()[f].deleted && !current.count(static_cast<uint32_t>(f))) {
                selectedFaces.push_back(static_cast<uint32_t>(f));
            }
        }
    } else if (mode == 2) {
        std::set<uint32_t> current(selectedEdges.begin(), selectedEdges.end());
        selectedEdges.clear();
        for (size_t e = 0; e < mesh->getEdges().size(); ++e) {
            if (!mesh->getEdges()[e].deleted && !current.count(static_cast<uint32_t>(e))) {
                selectedEdges.push_back(static_cast<uint32_t>(e));
            }
        }
    } else if (mode == 1) {
        std::set<uint32_t> current(selectedVertices.begin(), selectedVertices.end());
        selectedVertices.clear();
        for (size_t v = 0; v < mesh->getVertices().size(); ++v) {
            if (!mesh->getVertices()[v].deleted && !current.count(static_cast<uint32_t>(v))) {
                selectedVertices.push_back(static_cast<uint32_t>(v));
            }
        }
    }
}

void ModelingContext::selectBoundaryLoop(const std::shared_ptr<Part>& part) {
    if (!part || selectedFaces.empty()) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    std::set<uint32_t> selFaceSet(selectedFaces.begin(), selectedFaces.end());
    std::map<std::pair<uint32_t, uint32_t>, int> edgeOccurrences;
    auto& faces = mesh->getFaces();

    for (uint32_t f : selectedFaces) {
        if (f >= faces.size() || faces[f].deleted) continue;
        const auto& fv = faces[f].vertices;
        size_t count = fv.size();
        for (size_t i = 0; i < count; ++i) {
            uint32_t v0 = fv[i];
            uint32_t v1 = fv[(i + 1) % count];
            auto key = std::minmax(v0, v1);
            edgeOccurrences[key]++;
        }
    }

    selectedEdges.clear();
    for (const auto& [edgeKey, count] : edgeOccurrences) {
        if (count == 1) {
            int e = mesh->findEdge(edgeKey.first, edgeKey.second);
            if (e >= 0) selectedEdges.push_back(static_cast<uint32_t>(e));
        }
    }
}

void ModelingContext::executeSolidify(std::shared_ptr<Part> part, float thickness, bool rimFill) {
    if (!part) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    Engine::Geometry::MeshOperators::solidify(*mesh, thickness, rimFill);
    part->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone(),
        selectedVertices, selectedEdges, selectedFaces,
        selectedVertices, selectedEdges, selectedFaces);
    UndoStack::instance().push(std::move(cmd));
}

void ModelingContext::executeSeparate(std::shared_ptr<Part> part) {
    if (!part || selectedFaces.empty()) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    auto newMesh = std::make_shared<Engine::Geometry::EditableMesh>();

    std::map<uint32_t, uint32_t> oldToNew;
    const auto& vertices = mesh->getVertices();
    const auto& faces = mesh->getFaces();

    for (uint32_t fIdx : selectedFaces) {
        if (fIdx >= faces.size() || faces[fIdx].deleted) continue;
        const auto& fv = faces[fIdx].vertices;
        std::vector<uint32_t> newFv;
        for (uint32_t v : fv) {
            if (oldToNew.find(v) == oldToNew.end()) {
                oldToNew[v] = newMesh->addVertex(vertices[v].position, vertices[v].u, vertices[v].v, vertices[v].normal);
            }
            newFv.push_back(oldToNew[v]);
        }
        newMesh->addFace(newFv);
        mesh->removeFace(fIdx);
    }

    newMesh->rebuildTopology();
    newMesh->recalculateAllNormals(false);

    mesh->packAndCompact();
    part->rebuildProceduralMesh();
    clearSelection();

    // Spawn separate part into workspace
    auto newPart = std::make_shared<Part>();
    newPart->name = part->name + "_Sep";
    newPart->setPosition(part->getPosition());
    newPart->setEditableMesh(newMesh);
    if (auto parent = part->getParent()) {
        newPart->setParent(parent);
    }

    // Keep the generated part alive in the command so Separate can be undone
    // and redone without losing its hierarchy membership.
    auto parent = part->getParent();
    UndoStack::instance().push(std::make_unique<InstanceHierarchyCommand>(
        std::vector<std::shared_ptr<Instance>>{newPart},
        std::vector<std::shared_ptr<Instance>>{nullptr},
        std::vector<std::shared_ptr<Instance>>{parent}
    ));
}

void ModelingContext::executeJoin(std::vector<std::shared_ptr<Instance>> selectedInstances) {
    std::vector<std::shared_ptr<Part>> parts;
    for (const auto& inst : selectedInstances) {
        if (auto p = std::dynamic_pointer_cast<Part>(inst)) {
            parts.push_back(p);
        }
    }
    if (parts.size() < 2) return;

    auto mainPart = parts[0];
    mainPart->ensureEditableMesh();
    auto mainMesh = mainPart->getEditableMesh();
    if (!mainMesh) return;

    auto before = mainMesh->clone();
    Engine::Math::Vector3 mainPos = mainPart->getPosition();

    std::vector<std::shared_ptr<Instance>> hierarchyInstances;
    std::vector<std::shared_ptr<Instance>> beforeParents;
    std::vector<std::shared_ptr<Instance>> afterParents;
    hierarchyInstances.reserve(parts.size());
    beforeParents.reserve(parts.size());
    afterParents.reserve(parts.size());

    hierarchyInstances.push_back(mainPart);
    beforeParents.push_back(mainPart->getParent());
    afterParents.push_back(mainPart->getParent());

    for (size_t i = 1; i < parts.size(); ++i) {
        auto otherPart = parts[i];
        otherPart->ensureEditableMesh();
        auto otherMesh = otherPart->getEditableMesh();
        if (!otherMesh) continue;

        hierarchyInstances.push_back(otherPart);
        beforeParents.push_back(otherPart->getParent());
        afterParents.push_back(nullptr);

        Engine::Math::Vector3 otherPos = otherPart->getPosition();
        Engine::Math::Vector3 offset = otherPos - mainPos;

        const auto& otherVerts = otherMesh->getVertices();
        const auto& otherFaces = otherMesh->getFaces();

        std::map<uint32_t, uint32_t> remap;
        for (size_t v = 0; v < otherVerts.size(); ++v) {
            if (otherVerts[v].deleted) continue;
            remap[static_cast<uint32_t>(v)] = mainMesh->addVertex(otherVerts[v].position + offset, otherVerts[v].u, otherVerts[v].v, otherVerts[v].normal);
        }

        for (const auto& f : otherFaces) {
            if (f.deleted) continue;
            std::vector<uint32_t> newFv;
            std::vector<std::pair<float, float>> newUVs;
            for (uint32_t v : f.vertices) {
                if (remap.count(v)) {
                    newFv.push_back(remap[v]);
                    const size_t corner = newFv.size() - 1;
                    newUVs.push_back(corner < f.uvs.size() ? f.uvs[corner] : std::make_pair(0.0f, 0.0f));
                }
            }
            if (newFv.size() >= 3) {
                mainMesh->addFaceWithUVs(newFv, newUVs, f.materialId);
            }
        }

        otherPart->setParent(nullptr);
    }

    mainMesh->rebuildTopology();
    mainMesh->recalculateAllNormals(false);
    mainPart->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(mainPart, before, mainMesh->clone());
    UndoStack::instance().push(std::move(cmd));

    UndoStack::instance().push(std::make_unique<InstanceHierarchyCommand>(
        std::move(hierarchyInstances), std::move(beforeParents), std::move(afterParents)));
}

} // namespace Editor::Modeling
