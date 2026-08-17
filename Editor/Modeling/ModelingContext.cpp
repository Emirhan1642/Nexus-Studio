#include "ModelingContext.h"
#include "Engine/Core/Geometry/UVUnwrapper.h"
#include <cmath>
#include <set>
#include <map>
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

void ModelingContext::executePoke(std::shared_ptr<Part> part, float offset) {
    if (!part || selectedFaces.empty()) return;
    part->ensureEditableMesh();
    auto mesh = part->getEditableMesh();
    if (!mesh) return;

    auto before = mesh->clone();
    Engine::Geometry::MeshOperators::pokeFaces(*mesh, selectedFaces, offset);
    part->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone());
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

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone());
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

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone());
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

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone());
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

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone());
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

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone());
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
    std::map<std::pair<uint32_t, uint32_t>, int> edgeCount;
    auto& faces = mesh->getFaces();

    for (uint32_t f : selectedFaces) {
        if (f >= faces.size() || faces[f].deleted) continue;
        const auto& fv = faces[f].vertices;
        size_t count = fv.size();
        for (size_t i = 0; i < count; ++i) {
            uint32_t v0 = fv[i];
            uint32_t v1 = fv[(i + 1) % count];
            edgeCount[{v0, v1}]++;
        }
    }

    selectedEdges.clear();
    for (const auto& [edge, count] : edgeCount) {
        if (count == 1 && edgeCount.find({edge.second, edge.first}) == edgeCount.end()) {
            int e = mesh->findEdge(edge.first, edge.second);
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

    auto cmd = std::make_unique<MeshTopologyCommand>(part, before, mesh->clone());
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

    for (size_t i = 1; i < parts.size(); ++i) {
        auto otherPart = parts[i];
        otherPart->ensureEditableMesh();
        auto otherMesh = otherPart->getEditableMesh();
        if (!otherMesh) continue;

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
            for (uint32_t v : f.vertices) {
                if (remap.count(v)) newFv.push_back(remap[v]);
            }
            if (newFv.size() >= 3) mainMesh->addFace(newFv);
        }

        otherPart->destroy();
    }

    mainMesh->rebuildTopology();
    mainMesh->recalculateAllNormals(false);
    mainPart->rebuildProceduralMesh();

    auto cmd = std::make_unique<MeshTopologyCommand>(mainPart, before, mainMesh->clone());
    UndoStack::instance().push(std::move(cmd));
}

} // namespace Editor::Modeling
