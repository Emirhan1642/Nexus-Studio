#include "MeshCutOperators.h"
#include <cmath>
#include <algorithm>
#include <set>
#include <map>

namespace Engine::Geometry {

// ── Loop Cut & Slide ────────────────────────────────────────────────────────
std::vector<uint32_t> MeshCutOperators::findEdgeLoop(
    const EditableMesh& mesh,
    uint32_t startEdgeIdx
) {
    std::vector<uint32_t> loop;
    const auto& edges = mesh.getEdges();
    const auto& faces = mesh.getFaces();

    if (startEdgeIdx >= edges.size() || edges[startEdgeIdx].deleted) return loop;

    std::set<uint32_t> visitedEdges;
    loop.push_back(startEdgeIdx);
    visitedEdges.insert(startEdgeIdx);

    auto getOppositeEdge = [&](uint32_t fIdx, uint32_t inEdgeIdx) -> int {
        if (fIdx >= faces.size() || faces[fIdx].deleted) return -1;
        const auto& fVerts = faces[fIdx].vertices;
        if (fVerts.size() != 4) return -1; // Must be a quad

        uint32_t ev0 = edges[inEdgeIdx].v0;
        uint32_t ev1 = edges[inEdgeIdx].v1;

        // Find edge in quad
        int edgePos = -1;
        for (int i = 0; i < 4; ++i) {
            uint32_t vA = fVerts[i];
            uint32_t vB = fVerts[(i + 1) % 4];
            if ((vA == ev0 && vB == ev1) || (vA == ev1 && vB == ev0)) {
                edgePos = i;
                break;
            }
        }
        if (edgePos == -1) return -1;

        // Opposite edge in quad is at index (edgePos + 2) % 4
        int oppPos = (edgePos + 2) % 4;
        uint32_t oppV0 = fVerts[oppPos];
        uint32_t oppV1 = fVerts[(oppPos + 1) % 4];

        return mesh.findEdge(oppV0, oppV1);
    };

    // Traverse in both directions along connected quads
    for (int dir = 0; dir < 2; ++dir) {
        uint32_t curEdge = startEdgeIdx;
        while (true) {
            auto edgeFaces = mesh.getEdgeFaces(curEdge);
            if (edgeFaces.empty()) break;

            bool advanced = false;
            for (uint32_t fIdx : edgeFaces) {
                int oppEdge = getOppositeEdge(fIdx, curEdge);
                if (oppEdge >= 0 && !visitedEdges.count(static_cast<uint32_t>(oppEdge))) {
                    visitedEdges.insert(static_cast<uint32_t>(oppEdge));
                    loop.push_back(static_cast<uint32_t>(oppEdge));
                    curEdge = static_cast<uint32_t>(oppEdge);
                    advanced = true;
                    break;
                }
            }
            if (!advanced) break;
        }
    }

    return loop;
}

std::vector<uint32_t> MeshCutOperators::applyLoopCut(
    EditableMesh& mesh,
    const std::vector<uint32_t>& edgeLoop,
    float slideFactor,
    int numCuts
) {
    std::vector<uint32_t> newEdges;
    if (edgeLoop.empty()) return newEdges;

    auto& edges = mesh.getEdges();
    auto& vertices = mesh.getVertices();
    auto& faces = mesh.getFaces();

    numCuts = std::max(1, std::min(6, numCuts));
    slideFactor = std::max(-0.90f, std::min(0.90f, slideFactor));

    // 1. Pre-generate shared split vertices for every edge in edgeLoop
    // Map: edgeIdx -> vector of split vertex IDs (ordered from edge.v0 to edge.v1)
    std::map<uint32_t, std::vector<uint32_t>> edgeSplitMap;
    for (uint32_t eIdx : edgeLoop) {
        if (eIdx >= edges.size() || edges[eIdx].deleted) continue;
        uint32_t v0 = edges[eIdx].v0;
        uint32_t v1 = edges[eIdx].v1;
        if (v0 >= vertices.size() || v1 >= vertices.size()) continue;

        std::vector<uint32_t> splits(numCuts);
        for (int k = 0; k < numCuts; ++k) {
            float baseT = (float)(k + 1) / (float)(numCuts + 1);
            float t = std::clamp(baseT + slideFactor * (0.40f / (float)(numCuts + 1)), 0.02f, 0.98f);

            Engine::Math::Vector3 p = vertices[v0].position + (vertices[v1].position - vertices[v0].position) * t;
            splits[k] = mesh.addVertex(p, vertices[v0].u, vertices[v0].v);
        }
        edgeSplitMap[eIdx] = splits;
    }

    // Identify all affected quad faces
    std::set<uint32_t> affectedFaces;
    for (uint32_t eIdx : edgeLoop) {
        auto edgeFaces = mesh.getEdgeFaces(eIdx);
        for (uint32_t f : edgeFaces) affectedFaces.insert(f);
    }

    std::set<uint32_t> loopEdgeSet(edgeLoop.begin(), edgeLoop.end());

    auto getEdgeSplitsOriented = [&](uint32_t eIdx, uint32_t startV) -> std::vector<uint32_t> {
        auto splits = edgeSplitMap[eIdx];
        if (edges[eIdx].v0 != startV) {
            std::reverse(splits.begin(), splits.end());
        }
        return splits;
    };

    for (uint32_t fIdx : affectedFaces) {
        if (fIdx >= faces.size() || faces[fIdx].deleted) continue;
        auto face = faces[fIdx];
        if (face.vertices.size() != 4) continue;

        uint32_t v0 = face.vertices[0];
        uint32_t v1 = face.vertices[1];
        uint32_t v2 = face.vertices[2];
        uint32_t v3 = face.vertices[3];

        int e0 = mesh.findEdge(v0, v1);
        int e1 = mesh.findEdge(v1, v2);
        int e2 = mesh.findEdge(v2, v3);
        int e3 = mesh.findEdge(v3, v0);

        bool has0 = (e0 >= 0 && loopEdgeSet.count((uint32_t)e0));
        bool has2 = (e2 >= 0 && loopEdgeSet.count((uint32_t)e2));

        bool has1 = (e1 >= 0 && loopEdgeSet.count((uint32_t)e1));
        bool has3 = (e3 >= 0 && loopEdgeSet.count((uint32_t)e3));

        if (has0 || has2) {
            std::vector<uint32_t> sv0 = (has0) ? getEdgeSplitsOriented((uint32_t)e0, v0) : std::vector<uint32_t>(numCuts);
            std::vector<uint32_t> sv2 = (has2) ? getEdgeSplitsOriented((uint32_t)e2, v3) : std::vector<uint32_t>(numCuts);

            if (!has0 || !has2) {
                // Fallback for boundary quad with single loop edge
                for (int k = 0; k < numCuts; ++k) {
                    float baseT = (float)(k + 1) / (float)(numCuts + 1);
                    float t = std::clamp(baseT + slideFactor * (0.40f / (float)(numCuts + 1)), 0.02f, 0.98f);
                    if (!has0) {
                        Engine::Math::Vector3 pA = vertices[v0].position + (vertices[v1].position - vertices[v0].position) * t;
                        sv0[k] = mesh.addVertex(pA, vertices[v0].u, vertices[v0].v);
                    }
                    if (!has2) {
                        Engine::Math::Vector3 pB = vertices[v3].position + (vertices[v2].position - vertices[v3].position) * t;
                        sv2[k] = mesh.addVertex(pB, vertices[v3].u, vertices[v3].v);
                    }
                }
            }

            mesh.removeFace(fIdx);
            mesh.addFace({ v0, sv0[0], sv2[0], v3 });
            for (int k = 0; k + 1 < numCuts; ++k) {
                mesh.addFace({ sv0[k], sv0[k + 1], sv2[k + 1], sv2[k] });
            }
            mesh.addFace({ sv0[numCuts - 1], v1, v2, sv2[numCuts - 1] });

            for (int k = 0; k < numCuts; ++k) {
                int ne = mesh.addEdge(sv0[k], sv2[k]);
                if (ne >= 0) newEdges.push_back((uint32_t)ne);
            }
        } else if (has1 || has3) {
            std::vector<uint32_t> sv1 = (has1) ? getEdgeSplitsOriented((uint32_t)e1, v1) : std::vector<uint32_t>(numCuts);
            std::vector<uint32_t> sv3 = (has3) ? getEdgeSplitsOriented((uint32_t)e3, v0) : std::vector<uint32_t>(numCuts);

            if (!has1 || !has3) {
                // Fallback for boundary quad with single loop edge
                for (int k = 0; k < numCuts; ++k) {
                    float baseT = (float)(k + 1) / (float)(numCuts + 1);
                    float t = std::clamp(baseT + slideFactor * (0.40f / (float)(numCuts + 1)), 0.02f, 0.98f);
                    if (!has1) {
                        Engine::Math::Vector3 pA = vertices[v1].position + (vertices[v2].position - vertices[v1].position) * t;
                        sv1[k] = mesh.addVertex(pA, vertices[v1].u, vertices[v1].v);
                    }
                    if (!has3) {
                        Engine::Math::Vector3 pB = vertices[v0].position + (vertices[v3].position - vertices[v0].position) * t;
                        sv3[k] = mesh.addVertex(pB, vertices[v0].u, vertices[v0].v);
                    }
                }
            }

            mesh.removeFace(fIdx);
            mesh.addFace({ v0, v1, sv1[0], sv3[0] });
            for (int k = 0; k + 1 < numCuts; ++k) {
                mesh.addFace({ sv3[k], sv1[k], sv1[k + 1], sv3[k + 1] });
            }
            mesh.addFace({ sv3[numCuts - 1], sv1[numCuts - 1], v2, v3 });

            for (int k = 0; k < numCuts; ++k) {
                int ne = mesh.addEdge(sv3[k], sv1[k]);
                if (ne >= 0) newEdges.push_back((uint32_t)ne);
            }
        }
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
    return newEdges;
}

// ── Knife Tool ──────────────────────────────────────────────────────────────
bool MeshCutOperators::cutMeshWithKnifePolyline(
    EditableMesh& mesh,
    const std::vector<Engine::Math::Vector3>& localPoints,
    const std::vector<uint32_t>& targetFaces,
    bool cutThrough
) {
    if (localPoints.size() < 2) return false;

    // Determine candidate faces
    std::vector<uint32_t> initialFaceIndices;
    if (!cutThrough && !targetFaces.empty()) {
        std::set<uint32_t> uniqueTargets(targetFaces.begin(), targetFaces.end());
        for (uint32_t f : uniqueTargets) {
            if (f < mesh.getFaces().size() && !mesh.getFaces()[f].deleted) {
                initialFaceIndices.push_back(f);
            }
        }
    } else {
        // Cut through mode: check all faces in mesh
        for (size_t f = 0; f < mesh.getFaces().size(); ++f) {
            if (!mesh.getFaces()[f].deleted) {
                initialFaceIndices.push_back((uint32_t)f);
            }
        }
    }

    bool anyCut = false;
    for (size_t i = 0; i + 1 < localPoints.size(); ++i) {
        const auto& p0 = localPoints[i];
        const auto& p1 = localPoints[i + 1];
        Engine::Math::Vector3 segDir = (p1 - p0);
        if (segDir.length() < 1e-4f) continue;

        for (uint32_t fIdx : initialFaceIndices) {
            if (fIdx >= mesh.getFaces().size() || mesh.getFaces()[fIdx].deleted) continue;

            if (cutFaceWithRaySegment(mesh, fIdx, p0, p1)) {
                anyCut = true;
            } else if (cutThrough) {
                // Cut-through projection: find intersections of the cutting plane with this face's edges
                auto face = mesh.getFaces()[fIdx];
                if (face.vertices.size() < 3) continue;

                mesh.calculateFaceNormal(fIdx);
                Engine::Math::Vector3 fn = face.normal;

                // Cutting plane normal perpendicular to segment and camera/face
                Engine::Math::Vector3 planeNorm = segDir.cross(fn);
                if (planeNorm.length() < 1e-4f) {
                    planeNorm = segDir.cross(Engine::Math::Vector3(0, 1, 0));
                    if (planeNorm.length() < 1e-4f) planeNorm = segDir.cross(Engine::Math::Vector3(1, 0, 0));
                }
                planeNorm = planeNorm.normalized();

                // Find edge-plane intersections
                std::vector<Engine::Math::Vector3> edgeHits;
                for (size_t vi = 0; vi < face.vertices.size(); ++vi) {
                    size_t nextVi = (vi + 1) % face.vertices.size();
                    Engine::Math::Vector3 ev0 = mesh.getVertices()[face.vertices[vi]].position;
                    Engine::Math::Vector3 ev1 = mesh.getVertices()[face.vertices[nextVi]].position;

                    float dist0 = (ev0 - p0).dot(planeNorm);
                    float dist1 = (ev1 - p0).dot(planeNorm);

                    if ((dist0 > 1e-4f && dist1 < -1e-4f) || (dist0 < -1e-4f && dist1 > 1e-4f)) {
                        float t = dist0 / (dist0 - dist1);
                        edgeHits.push_back(ev0 + (ev1 - ev0) * t);
                    }
                }

                if (edgeHits.size() == 2) {
                    if (cutFaceWithRaySegment(mesh, fIdx, edgeHits[0], edgeHits[1])) {
                        anyCut = true;
                    }
                }
            }
        }
    }

    if (anyCut) {
        mesh.rebuildTopology();
        mesh.recalculateAllNormals(false);
    }
    return anyCut;
}

bool MeshCutOperators::cutFaceWithRaySegment(
    EditableMesh& mesh,
    uint32_t faceIndex,
    const Engine::Math::Vector3& p0,
    const Engine::Math::Vector3& p1
) {
    auto& faces = mesh.getFaces();
    auto& vertices = mesh.getVertices();
    if (faceIndex >= faces.size() || faces[faceIndex].deleted) return false;

    auto face = faces[faceIndex];
    if (face.vertices.size() < 3) return false;

    mesh.calculateFaceNormal(faceIndex);
    Engine::Math::Vector3 fn = face.normal;
    Engine::Math::Vector3 fp = vertices[face.vertices[0]].position;

    float d0 = std::abs((p0 - fp).dot(fn));
    float d1 = std::abs((p1 - fp).dot(fn));
    // BOTH end points of the cut segment MUST lie precisely on this face's plane
    if (d0 > 0.05f || d1 > 0.05f) return false;

    // Check if p0 or p1 snaps directly to existing face corner vertices
    int matchIdx0 = -1;
    int matchIdx1 = -1;
    for (size_t i = 0; i < face.vertices.size(); ++i) {
        uint32_t v = face.vertices[i];
        if ((vertices[v].position - p0).length() < 0.08f) matchIdx0 = (int)i;
        if ((vertices[v].position - p1).length() < 0.08f) matchIdx1 = (int)i;
    }

    // Vertex-to-Vertex Diagonal Split (e.g. Quad -> Two Triangles)
    if (matchIdx0 != -1 && matchIdx1 != -1 && matchIdx0 != matchIdx1) {
        if (face.vertices.size() == 4) {
            uint32_t v0 = face.vertices[0], v1 = face.vertices[1];
            uint32_t v2 = face.vertices[2], v3 = face.vertices[3];

            if ((matchIdx0 == 0 && matchIdx1 == 2) || (matchIdx0 == 2 && matchIdx1 == 0)) {
                mesh.removeFace(faceIndex);
                mesh.addFace({v0, v1, v2});
                mesh.addFace({v2, v3, v0});
                mesh.rebuildTopology();
                mesh.recalculateAllNormals(false);
                return true;
            } else if ((matchIdx0 == 1 && matchIdx1 == 3) || (matchIdx0 == 3 && matchIdx1 == 1)) {
                mesh.removeFace(faceIndex);
                mesh.addFace({v0, v1, v3});
                mesh.addFace({v1, v2, v3});
                mesh.rebuildTopology();
                mesh.recalculateAllNormals(false);
                return true;
            }
        }
    }

    // General Cut: Add safe split vertices
    uint32_t nv0 = (matchIdx0 != -1) ? face.vertices[matchIdx0] : mesh.addVertex(p0, 0.5f, 0.5f, fn);
    uint32_t nv1 = (matchIdx1 != -1) ? face.vertices[matchIdx1] : mesh.addVertex(p1, 0.5f, 0.5f, fn);

    if (nv0 == nv1) return false;

    mesh.removeFace(faceIndex);

    size_t count = face.vertices.size();
    size_t mid = count / 2;

    std::vector<uint32_t> fA = { nv0, nv1 };
    for (size_t i = 0; i < mid; ++i) {
        if (face.vertices[i] != nv0 && face.vertices[i] != nv1) fA.push_back(face.vertices[i]);
    }

    std::vector<uint32_t> fB = { nv1, nv0 };
    for (size_t i = mid; i < count; ++i) {
        if (face.vertices[i] != nv0 && face.vertices[i] != nv1) fB.push_back(face.vertices[i]);
    }

    if (fA.size() >= 3) mesh.addFace(fA);
    if (fB.size() >= 3) mesh.addFace(fB);

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
    return true;
}

// ── Mirror & Symmetry ───────────────────────────────────────────────────────
void MeshCutOperators::applyMirror(
    EditableMesh& mesh,
    MirrorAxis axis,
    bool mergeCenter,
    float mergeThreshold
) {
    auto& vertices = mesh.getVertices();
    auto& faces = mesh.getFaces();

    size_t origVertCount = vertices.size();
    size_t origFaceCount = faces.size();

    std::vector<uint32_t> mirrorVertMap(origVertCount);

    for (size_t i = 0; i < origVertCount; ++i) {
        if (vertices[i].deleted) continue;
        Engine::Math::Vector3 pos = vertices[i].position;

        if (axis == MirrorAxis::X) pos.x = -pos.x;
        else if (axis == MirrorAxis::Y) pos.y = -pos.y;
        else if (axis == MirrorAxis::Z) pos.z = -pos.z;

        uint32_t newV = mesh.addVertex(pos, vertices[i].u, vertices[i].v, vertices[i].normal);
        mirrorVertMap[i] = newV;
    }

    // Mirror faces with reversed winding
    for (size_t f = 0; f < origFaceCount; ++f) {
        if (faces[f].deleted) continue;
        auto fVerts = faces[f].vertices;
        std::vector<uint32_t> mirroredFaceVerts(fVerts.size());

        for (size_t i = 0; i < fVerts.size(); ++i) {
            mirroredFaceVerts[i] = mirrorVertMap[fVerts[fVerts.size() - 1 - i]];
        }
        mesh.addFace(mirroredFaceVerts);
    }

    if (mergeCenter) {
        // Find center vertices and merge
        for (size_t i = 0; i < origVertCount; ++i) {
            if (vertices[i].deleted) continue;
            float val = 0.0f;
            if (axis == MirrorAxis::X) val = std::abs(vertices[i].position.x);
            else if (axis == MirrorAxis::Y) val = std::abs(vertices[i].position.y);
            else if (axis == MirrorAxis::Z) val = std::abs(vertices[i].position.z);

            if (val <= mergeThreshold) {
                uint32_t vMirrored = mirrorVertMap[i];
                if (axis == MirrorAxis::X) vertices[i].position.x = 0.0f;
                else if (axis == MirrorAxis::Y) vertices[i].position.y = 0.0f;
                else if (axis == MirrorAxis::Z) vertices[i].position.z = 0.0f;

                // Replace references to vMirrored with i
                for (size_t f = origFaceCount; f < mesh.getFaces().size(); ++f) {
                    for (auto& v : mesh.getFaces()[f].vertices) {
                        if (v == vMirrored) v = static_cast<uint32_t>(i);
                    }
                }
                mesh.removeVertex(vMirrored);
            }
        }
    }

    mesh.packAndCompact();
    mesh.recalculateAllNormals(false);
}

// ── Boolean CSG ─────────────────────────────────────────────────────────────
std::shared_ptr<EditableMesh> MeshCutOperators::applyBoolean(
    const EditableMesh& meshA,
    const EditableMesh& meshB,
    BooleanOperation op
) {
    auto result = std::make_shared<EditableMesh>(meshA);

    std::map<uint32_t, uint32_t> bVertRemap;
    for (size_t i = 0; i < meshB.getVertices().size(); ++i) {
        const auto& v = meshB.getVertices()[i];
        if (!v.deleted) {
            Engine::Math::Vector3 norm = (op == BooleanOperation::Difference) ? v.normal * -1.0f : v.normal;
            bVertRemap[static_cast<uint32_t>(i)] = result->addVertex(v.position, v.u, v.v, norm);
        }
    }

    for (const auto& f : meshB.getFaces()) {
        if (f.deleted) continue;
        std::vector<uint32_t> remapped;
        for (uint32_t v : f.vertices) {
            if (bVertRemap.count(v)) {
                remapped.push_back(bVertRemap[v]);
            }
        }
        if (remapped.size() >= 3) {
            if (op == BooleanOperation::Difference) {
                std::reverse(remapped.begin(), remapped.end()); // Invert winding
            }
            result->addFace(remapped);
        }
    }

    result->packAndCompact();
    result->recalculateAllNormals(false);
    return result;
}

} // namespace Engine::Geometry
