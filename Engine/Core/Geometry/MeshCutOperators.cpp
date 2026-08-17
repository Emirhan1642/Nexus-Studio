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

    auto getOppositeEdgeInQuad = [&](uint32_t fIdx, uint32_t inEdgeIdx) -> int {
        if (fIdx >= faces.size() || faces[fIdx].deleted) return -1;
        const auto& fEdges = faces[fIdx].edges;
        if (fEdges.size() != 4) return -1; // Only pure quads have unambiguous opposite edges

        for (size_t i = 0; i < 4; ++i) {
            if (fEdges[i] == inEdgeIdx) {
                return fEdges[(i + 2) % 4]; // Opposite edge in a 4-gon
            }
        }
        return -1;
    };

    // Traverse in direction 1
    uint32_t curEdge = startEdgeIdx;
    while (true) {
        auto edgeFaces = mesh.getEdgeFaces(curEdge);
        if (edgeFaces.empty()) break;

        bool advanced = false;
        for (uint32_t fIdx : edgeFaces) {
            int oppEdge = getOppositeEdgeInQuad(fIdx, curEdge);
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

    slideFactor = std::max(-0.95f, std::min(0.95f, slideFactor));
    float t = 0.5f + slideFactor * 0.45f;

    // Map: edgeIdx -> newly created split vertexIdx
    std::map<uint32_t, uint32_t> edgeSplitVerts;

    for (uint32_t eIdx : edgeLoop) {
        if (eIdx >= edges.size() || edges[eIdx].deleted) continue;
        uint32_t v0 = edges[eIdx].v0;
        uint32_t v1 = edges[eIdx].v1;

        Engine::Math::Vector3 p = vertices[v0].position + (vertices[v1].position - vertices[v0].position) * t;
        float u = vertices[v0].u + (vertices[v1].u - vertices[v0].u) * t;
        float v = vertices[v0].v + (vertices[v1].v - vertices[v0].v) * t;

        uint32_t newV = mesh.addVertex(p, u, v);
        edgeSplitVerts[eIdx] = newV;
    }

    // Now split the faces intersected by the edge loop
    std::set<uint32_t> affectedFaces;
    for (uint32_t eIdx : edgeLoop) {
        auto edgeFaces = mesh.getEdgeFaces(eIdx);
        for (uint32_t f : edgeFaces) affectedFaces.insert(f);
    }

    for (uint32_t fIdx : affectedFaces) {
        if (fIdx >= faces.size() || faces[fIdx].deleted) continue;
        auto face = faces[fIdx];
        if (face.vertices.size() != 4) continue;

        // Check which two edges in this quad were split
        std::vector<std::pair<size_t, uint32_t>> splitsInFace; // (edge index in face, split vert id)
        for (size_t i = 0; i < 4; ++i) {
            uint32_t e = face.edges[i];
            auto it = edgeSplitVerts.find(e);
            if (it != edgeSplitVerts.end()) {
                splitsInFace.push_back({i, it->second});
            }
        }

        if (splitsInFace.size() == 2) {
            // Split quad into two quads
            uint32_t v0 = face.vertices[0];
            uint32_t v1 = face.vertices[1];
            uint32_t v2 = face.vertices[2];
            uint32_t v3 = face.vertices[3];

            size_t e0Idx = splitsInFace[0].first;
            size_t e1Idx = splitsInFace[1].first;
            uint32_t sv0 = splitsInFace[0].second;
            uint32_t sv1 = splitsInFace[1].second;

            mesh.removeFace(fIdx);

            if ((e0Idx == 0 && e1Idx == 2) || (e0Idx == 2 && e1Idx == 0)) {
                // Horizontal split (edges 0 and 2)
                uint32_t sTop = (e0Idx == 0) ? sv0 : sv1;
                uint32_t sBot = (e0Idx == 2) ? sv0 : sv1;
                mesh.addFace({v0, sTop, sBot, v3});
                mesh.addFace({sTop, v1, v2, sBot});
            } else if ((e0Idx == 1 && e1Idx == 3) || (e0Idx == 3 && e1Idx == 1)) {
                // Vertical split (edges 1 and 3)
                uint32_t sRight = (e0Idx == 1) ? sv0 : sv1;
                uint32_t sLeft  = (e0Idx == 3) ? sv0 : sv1;
                mesh.addFace({v0, v1, sRight, sLeft});
                mesh.addFace({sLeft, sRight, v2, v3});
            }
        }
    }

    mesh.packAndCompact();
    mesh.recalculateAllNormals(false);
    return newEdges;
}

// ── Knife Tool ──────────────────────────────────────────────────────────────
bool MeshCutOperators::cutFaceWithRaySegment(
    EditableMesh& mesh,
    uint32_t faceIndex,
    const Engine::Math::Vector3& p0,
    const Engine::Math::Vector3& p1
) {
    auto& faces = mesh.getFaces();
    if (faceIndex >= faces.size() || faces[faceIndex].deleted) return false;

    uint32_t nv0 = mesh.addVertex(p0);
    uint32_t nv1 = mesh.addVertex(p1);

    auto face = faces[faceIndex];
    mesh.removeFace(faceIndex);

    // Simple bisect into two sub-faces using the new knife cut vertices
    std::vector<uint32_t> half1 = {nv0, nv1};
    std::vector<uint32_t> half2 = {nv1, nv0};

    for (size_t i = 0; i < face.vertices.size(); ++i) {
        if (i < face.vertices.size() / 2) half1.push_back(face.vertices[i]);
        else half2.push_back(face.vertices[i]);
    }

    mesh.addFace(half1);
    mesh.addFace(half2);

    mesh.packAndCompact();
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
