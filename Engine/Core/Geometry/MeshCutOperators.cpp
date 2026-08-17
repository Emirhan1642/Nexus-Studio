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

    // Map: edgeIdx -> vector of newly created split vertexIdx (size = numCuts)
    std::map<uint32_t, std::vector<uint32_t>> edgeSplitMap;

    for (uint32_t eIdx : edgeLoop) {
        if (eIdx >= edges.size() || edges[eIdx].deleted) continue;
        uint32_t v0 = edges[eIdx].v0;
        uint32_t v1 = edges[eIdx].v1;
        if (v0 >= vertices.size() || v1 >= vertices.size()) continue;

        std::vector<uint32_t> splits(numCuts);
        for (int k = 0; k < numCuts; ++k) {
            float baseT = (float)(k + 1) / (float)(numCuts + 1);
            float t = baseT + slideFactor * (0.40f / (float)(numCuts + 1));
            t = std::max(0.02f, std::min(0.98f, t));

            Engine::Math::Vector3 p = vertices[v0].position + (vertices[v1].position - vertices[v0].position) * t;
            float u = vertices[v0].u + (vertices[v1].u - vertices[v0].u) * t;
            float v = vertices[v0].v + (vertices[v1].v - vertices[v0].v) * t;

            splits[k] = mesh.addVertex(p, u, v);
        }
        edgeSplitMap[eIdx] = splits;
    }

    // Identify all affected quad faces
    std::set<uint32_t> affectedFaces;
    for (uint32_t eIdx : edgeLoop) {
        auto edgeFaces = mesh.getEdgeFaces(eIdx);
        for (uint32_t f : edgeFaces) affectedFaces.insert(f);
    }

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

        bool has0 = (e0 >= 0 && edgeSplitMap.count((uint32_t)e0));
        bool has1 = (e1 >= 0 && edgeSplitMap.count((uint32_t)e1));
        bool has2 = (e2 >= 0 && edgeSplitMap.count((uint32_t)e2));
        bool has3 = (e3 >= 0 && edgeSplitMap.count((uint32_t)e3));

        if (has0 && has2) {
            const auto& sv0 = edgeSplitMap[(uint32_t)e0];
            const auto& sv2 = edgeSplitMap[(uint32_t)e2];
            mesh.removeFace(fIdx);

            // First quad
            mesh.addFace({v0, sv0[0], sv2[0], v3});

            // Intermediate quads
            for (int k = 0; k + 1 < numCuts; ++k) {
                mesh.addFace({sv0[k], sv0[k + 1], sv2[k + 1], sv2[k]});
            }

            // Last quad
            mesh.addFace({sv0[numCuts - 1], v1, v2, sv2[numCuts - 1]});
        } else if (has1 && has3) {
            const auto& sv1 = edgeSplitMap[(uint32_t)e1];
            const auto& sv3 = edgeSplitMap[(uint32_t)e3];
            mesh.removeFace(fIdx);

            // First quad
            mesh.addFace({v0, v1, sv1[0], sv3[0]});

            // Intermediate quads
            for (int k = 0; k + 1 < numCuts; ++k) {
                mesh.addFace({sv3[k], sv1[k], sv1[k + 1], sv3[k + 1]});
            }

            // Last quad
            mesh.addFace({sv3[numCuts - 1], sv1[numCuts - 1], v2, v3});
        }
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
    return newEdges;
}

// ── Knife Tool ──────────────────────────────────────────────────────────────
bool MeshCutOperators::cutMeshWithKnifePolyline(
    EditableMesh& mesh,
    const std::vector<Engine::Math::Vector3>& localPoints
) {
    if (localPoints.size() < 2) return false;

    // Fixed snapshot of face indices at the beginning of the cut to prevent iterator invalidation
    std::vector<uint32_t> initialFaceIndices;
    for (size_t f = 0; f < mesh.getFaces().size(); ++f) {
        if (!mesh.getFaces()[f].deleted) {
            initialFaceIndices.push_back((uint32_t)f);
        }
    }

    bool anyCut = false;
    for (size_t i = 0; i + 1 < localPoints.size(); ++i) {
        const auto& p0 = localPoints[i];
        const auto& p1 = localPoints[i + 1];

        for (uint32_t fIdx : initialFaceIndices) {
            if (fIdx < mesh.getFaces().size() && !mesh.getFaces()[fIdx].deleted) {
                if (cutFaceWithRaySegment(mesh, fIdx, p0, p1)) {
                    anyCut = true;
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
    if (d0 > 0.6f && d1 > 0.6f) return false; // Segment does not lie on this face plane

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

    // Combine meshB into result based on operation
    size_t baseVertIdx = result->getVertices().size();

    if (op == BooleanOperation::Union) {
        // Append all vertices and faces of B
        for (const auto& v : meshB.getVertices()) {
            if (!v.deleted) result->addVertex(v.position, v.u, v.v, v.normal);
        }
        for (const auto& f : meshB.getFaces()) {
            if (!f.deleted) {
                std::vector<uint32_t> remapped = f.vertices;
                for (auto& v : remapped) v += static_cast<uint32_t>(baseVertIdx);
                result->addFace(remapped);
            }
        }
    } else if (op == BooleanOperation::Difference) {
        // Subtract B from A: Invert B's normals and merge
        for (const auto& v : meshB.getVertices()) {
            if (!v.deleted) result->addVertex(v.position, v.u, v.v, v.normal * -1.0f);
        }
        for (const auto& f : meshB.getFaces()) {
            if (!f.deleted) {
                std::vector<uint32_t> remapped = f.vertices;
                std::reverse(remapped.begin(), remapped.end()); // Invert winding
                for (auto& v : remapped) v += static_cast<uint32_t>(baseVertIdx);
                result->addFace(remapped);
            }
        }
    } else if (op == BooleanOperation::Intersect) {
        for (const auto& v : meshB.getVertices()) {
            if (!v.deleted) result->addVertex(v.position, v.u, v.v, v.normal);
        }
        for (const auto& f : meshB.getFaces()) {
            if (!f.deleted) {
                std::vector<uint32_t> remapped = f.vertices;
                for (auto& v : remapped) v += static_cast<uint32_t>(baseVertIdx);
                result->addFace(remapped);
            }
        }
    }

    result->packAndCompact();
    result->recalculateAllNormals(false);
    return result;
}

} // namespace Engine::Geometry
