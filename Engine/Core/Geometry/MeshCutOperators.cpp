#include "MeshCutOperators.h"
#include <cmath>
#include <algorithm>
#include <set>
#include <map>
#include <functional>
#include <tuple>
#include <optional>
#include <memory>

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

std::vector<uint32_t> MeshCutOperators::findFaceLoop(
    const EditableMesh& mesh,
    uint32_t startFaceIdx,
    uint32_t edgeHintIdx
) {
    std::vector<uint32_t> loop;
    const auto& faces = mesh.getFaces();
    const auto& edges = mesh.getEdges();

    if (startFaceIdx >= faces.size() || faces[startFaceIdx].deleted || faces[startFaceIdx].vertices.size() != 4) {
        return loop;
    }

    std::set<uint32_t> visitedFaces;
    loop.push_back(startFaceIdx);
    visitedFaces.insert(startFaceIdx);

    const auto& fVerts = faces[startFaceIdx].vertices;
    uint32_t forwardEdge = 0xFFFFFFFFu;
    uint32_t backwardEdge = 0xFFFFFFFFu;

    if (edgeHintIdx < edges.size() && !edges[edgeHintIdx].deleted) {
        forwardEdge = edgeHintIdx;
        int edgePos = -1;
        uint32_t ev0 = edges[edgeHintIdx].v0;
        uint32_t ev1 = edges[edgeHintIdx].v1;
        for (int i = 0; i < 4; ++i) {
            uint32_t vA = fVerts[i];
            uint32_t vB = fVerts[(i + 1) % 4];
            if ((vA == ev0 && vB == ev1) || (vA == ev1 && vB == ev0)) {
                edgePos = i;
                break;
            }
        }
        if (edgePos != -1) {
            int oppPos = (edgePos + 2) % 4;
            int oppE = mesh.findEdge(fVerts[oppPos], fVerts[(oppPos + 1) % 4]);
            if (oppE >= 0) backwardEdge = static_cast<uint32_t>(oppE);
        }
    } else {
        int e0 = mesh.findEdge(fVerts[0], fVerts[1]);
        int e2 = mesh.findEdge(fVerts[2], fVerts[3]);
        if (e0 >= 0) forwardEdge = static_cast<uint32_t>(e0);
        if (e2 >= 0) backwardEdge = static_cast<uint32_t>(e2);
    }

    auto traverseFrom = [&](uint32_t inEdge, uint32_t fromFace) {
        uint32_t curEdge = inEdge;
        uint32_t curFace = fromFace;
        while (curEdge < edges.size()) {
            auto connectedFaces = mesh.getEdgeFaces(curEdge);
            uint32_t nextFace = 0xFFFFFFFFu;
            for (uint32_t f : connectedFaces) {
                if (f != curFace && f < faces.size() && !faces[f].deleted && faces[f].vertices.size() == 4) {
                    nextFace = f;
                    break;
                }
            }
            if (nextFace == 0xFFFFFFFFu || visitedFaces.count(nextFace)) break;

            visitedFaces.insert(nextFace);
            loop.push_back(nextFace);

            const auto& nVerts = faces[nextFace].vertices;
            uint32_t ev0 = edges[curEdge].v0;
            uint32_t ev1 = edges[curEdge].v1;
            int edgePos = -1;
            for (int i = 0; i < 4; ++i) {
                uint32_t vA = nVerts[i];
                uint32_t vB = nVerts[(i + 1) % 4];
                if ((vA == ev0 && vB == ev1) || (vA == ev1 && vB == ev0)) {
                    edgePos = i;
                    break;
                }
            }
            if (edgePos == -1) break;

            int oppPos = (edgePos + 2) % 4;
            int oppE = mesh.findEdge(nVerts[oppPos], nVerts[(oppPos + 1) % 4]);
            if (oppE < 0) break;

            curFace = nextFace;
            curEdge = static_cast<uint32_t>(oppE);
        }
    };

    if (forwardEdge != 0xFFFFFFFFu) traverseFrom(forwardEdge, startFaceIdx);
    if (backwardEdge != 0xFFFFFFFFu) traverseFrom(backwardEdge, startFaceIdx);

    return loop;
}

std::vector<uint32_t> MeshCutOperators::applyLoopCut(
    EditableMesh& mesh,
    const std::vector<uint32_t>& edgeLoop,
    float slideFactor,
    int numCuts
) {
    std::vector<uint32_t> newEdges;
    std::vector<std::pair<uint32_t, uint32_t>> newEdgeVertexPairs;
    if (edgeLoop.empty()) return newEdges;

    auto& edges = mesh.getEdges();
    auto& vertices = mesh.getVertices();
    auto& faces = mesh.getFaces();

    numCuts = std::max(1, std::min(6, numCuts));
    slideFactor = std::max(-0.90f, std::min(0.90f, slideFactor));

    // Split vertices and replacement faces are appended below. Reserve the
    // worst-case working set so references used while processing the original
    // faces remain stable across addVertex/addFace calls.
    vertices.reserve(vertices.size() + edgeLoop.size() * static_cast<size_t>(numCuts * 3));
    faces.reserve(faces.size() + edgeLoop.size() * static_cast<size_t>((numCuts + 2) * 4));

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
        int origMatId = face.materialId;

        const auto addCutFace = [&](const std::vector<uint32_t>& faceVertices) {
            std::vector<std::pair<float, float>> cornerUVs;
            if (face.uvs.size() == face.vertices.size()) {
                cornerUVs.reserve(faceVertices.size());
                for (uint32_t vertex : faceVertices) {
                    auto sourceCorner = std::find(face.vertices.begin(), face.vertices.end(), vertex);
                    if (sourceCorner != face.vertices.end()) {
                        cornerUVs.push_back(face.uvs[static_cast<size_t>(sourceCorner - face.vertices.begin())]);
                    } else if (vertex < vertices.size()) {
                        cornerUVs.emplace_back(vertices[vertex].u, vertices[vertex].v);
                    } else {
                        cornerUVs.emplace_back(0.0f, 0.0f);
                    }
                }
            }
            return mesh.addFaceWithUVs(faceVertices, cornerUVs, origMatId);
        };

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
                        float u = vertices[v0].u + (vertices[v1].u - vertices[v0].u) * t;
                        float v = vertices[v0].v + (vertices[v1].v - vertices[v0].v) * t;
                        sv0[k] = mesh.addVertex(pA, u, v);
                    }
                    if (!has2) {
                        Engine::Math::Vector3 pB = vertices[v3].position + (vertices[v2].position - vertices[v3].position) * t;
                        float u = vertices[v3].u + (vertices[v2].u - vertices[v3].u) * t;
                        float v = vertices[v3].v + (vertices[v2].v - vertices[v3].v) * t;
                        sv2[k] = mesh.addVertex(pB, u, v);
                    }
                }
            }

            mesh.removeFace(fIdx);
            addCutFace({ v0, sv0[0], sv2[0], v3 });

            for (int k = 0; k + 1 < numCuts; ++k) {
                addCutFace({ sv0[k], sv0[k + 1], sv2[k + 1], sv2[k] });
            }
            addCutFace({ sv0[numCuts - 1], v1, v2, sv2[numCuts - 1] });

            for (int k = 0; k < numCuts; ++k) {
                int ne = mesh.addEdge(sv0[k], sv2[k]);
                if (ne >= 0) {
                    newEdgeVertexPairs.emplace_back(mesh.getEdges()[ne].v0, mesh.getEdges()[ne].v1);
                }
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
                        float u = vertices[v1].u + (vertices[v2].u - vertices[v1].u) * t;
                        float v = vertices[v1].v + (vertices[v2].v - vertices[v1].v) * t;
                        sv1[k] = mesh.addVertex(pA, u, v);
                    }
                    if (!has3) {
                        Engine::Math::Vector3 pB = vertices[v0].position + (vertices[v3].position - vertices[v0].position) * t;
                        float u = vertices[v0].u + (vertices[v3].u - vertices[v0].u) * t;
                        float v = vertices[v0].v + (vertices[v3].v - vertices[v0].v) * t;
                        sv3[k] = mesh.addVertex(pB, u, v);
                    }
                }
            }

            mesh.removeFace(fIdx);
            addCutFace({ v0, v1, sv1[0], sv3[0] });

            for (int k = 0; k + 1 < numCuts; ++k) {
                addCutFace({ sv3[k], sv1[k], sv1[k + 1], sv3[k + 1] });
            }
            addCutFace({ sv3[numCuts - 1], sv1[numCuts - 1], v2, v3 });

            for (int k = 0; k < numCuts; ++k) {
                int ne = mesh.addEdge(sv3[k], sv1[k]);
                if (ne >= 0) {
                    newEdgeVertexPairs.emplace_back(mesh.getEdges()[ne].v0, mesh.getEdges()[ne].v1);
                }
            }
        }
    }

    std::vector<uint32_t> vertRemap;
    mesh.packAndCompact(&vertRemap);
    // packAndCompact reindexes vertices and rebuilds edges. Re-resolve the
    // returned cut edges instead of exposing stale pre-compaction IDs.
    for (const auto& pair : newEdgeVertexPairs) {
        if (pair.first >= vertRemap.size() || pair.second >= vertRemap.size()) continue;
        const uint32_t v0 = vertRemap[pair.first];
        const uint32_t v1 = vertRemap[pair.second];
        if (v0 == 0xFFFFFFFF || v1 == 0xFFFFFFFF) continue;
        const int edge = mesh.findEdge(v0, v1);
        if (edge >= 0) newEdges.push_back(static_cast<uint32_t>(edge));
    }
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

    bool anyCut = false;
    std::vector<uint32_t> candidateFaces;
    auto collectLiveFaces = [&]() {
        std::vector<uint32_t> live;
        for (size_t f = 0; f < mesh.getFaces().size(); ++f) {
            if (!mesh.getFaces()[f].deleted && mesh.getFaces()[f].vertices.size() >= 3) {
                live.push_back(static_cast<uint32_t>(f));
            }
        }
        return live;
    };

    if (!cutThrough && !targetFaces.empty()) {
        std::set<uint32_t> uniqueTargets(targetFaces.begin(), targetFaces.end());
        for (uint32_t f : uniqueTargets) {
            if (f < mesh.getFaces().size() && !mesh.getFaces()[f].deleted) candidateFaces.push_back(f);
        }
    } else {
        candidateFaces = collectLiveFaces();
    }

    for (size_t i = 0; i + 1 < localPoints.size(); ++i) {
        const auto& p0 = localPoints[i];
        const auto& p1 = localPoints[i + 1];
        Engine::Math::Vector3 segDir = (p1 - p0);
        if (segDir.length() < 1e-4f) continue;

        if (cutThrough || targetFaces.empty()) candidateFaces = collectLiveFaces();
        std::vector<uint32_t> nextTargetFaces;

        for (uint32_t fIdx : candidateFaces) {
            if (fIdx >= mesh.getFaces().size() || mesh.getFaces()[fIdx].deleted) continue;

            const size_t faceCountBefore = mesh.getFaces().size();
            if (cutFaceWithRaySegment(mesh, fIdx, p0, p1, false)) {
                anyCut = true;
                if (!cutThrough && !targetFaces.empty()) {
                    for (size_t f = faceCountBefore; f < mesh.getFaces().size(); ++f) {
                        if (!mesh.getFaces()[f].deleted) nextTargetFaces.push_back(static_cast<uint32_t>(f));
                    }
                }
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
                    if (cutFaceWithRaySegment(mesh, fIdx, edgeHits[0], edgeHits[1], false)) {
                        anyCut = true;
                    }
                }
            }
        }

        if (!cutThrough && !targetFaces.empty()) {
            for (uint32_t f : candidateFaces) {
                if (f < mesh.getFaces().size() && !mesh.getFaces()[f].deleted) nextTargetFaces.push_back(f);
            }
            candidateFaces = std::move(nextTargetFaces);
        }
    }

    if (anyCut) {
        mesh.packAndCompact();
        mesh.rebuildTopology();
        mesh.recalculateAllNormals(false);
    }
    return anyCut;
}

bool MeshCutOperators::cutFaceWithRaySegment(
    EditableMesh& mesh,
    uint32_t faceIndex,
    const Engine::Math::Vector3& p0,
    const Engine::Math::Vector3& p1,
    bool compactResult
) {
    auto& faces = mesh.getFaces();
    auto& vertices = mesh.getVertices();
    if (faceIndex >= faces.size() || faces[faceIndex].deleted) return false;

    auto face = faces[faceIndex];
    size_t fvCount = face.vertices.size();
    if (fvCount < 3) return false;

    mesh.calculateFaceNormal(faceIndex);
    Engine::Math::Vector3 fn = face.normal;
    Engine::Math::Vector3 fp = vertices[face.vertices[0]].position;

    const float d0 = std::abs((p0 - fp).dot(fn));
    const float d1 = std::abs((p1 - fp).dot(fn));
    if (d0 > 0.05f || d1 > 0.05f) return false;

    Engine::Math::Vector3 segDir = p1 - p0;
    float segLen = segDir.length();
    if (segLen < 1e-4f) return false;
    segDir = segDir * (1.0f / segLen);

    Engine::Math::Vector3 uAxis = segDir;
    Engine::Math::Vector3 vAxis = fn.cross(uAxis).normalized();

    auto to2D = [&](const Engine::Math::Vector3& p) -> std::pair<float, float> {
        return { (p - p0).dot(uAxis), (p - p0).dot(vAxis) };
    };

    struct Intersection {
        uint32_t vertex = 0xFFFFFFFF;
        uint32_t a = 0xFFFFFFFF;
        uint32_t b = 0xFFFFFFFF;
        Engine::Math::Vector3 pos;
        float uVal = 0.0f;
    };
    std::vector<Intersection> hits;

    float maxEdgeLength = 0.0f;
    for (size_t i = 0; i < fvCount; ++i) {
        const auto& a = vertices[face.vertices[i]].position;
        const auto& b = vertices[face.vertices[(i + 1) % fvCount]].position;
        maxEdgeLength = std::max(maxEdgeLength, (b - a).length());
    }
    const float tol = std::max(1e-4f, maxEdgeLength * 0.02f);

    // 1. Check if any polygon vertices lie on the cut line
    for (uint32_t v : face.vertices) {
        auto [u, vCoord] = to2D(vertices[v].position);
        if (std::abs(vCoord) <= tol) {
            hits.push_back({v, v, v, vertices[v].position, u});
        }
    }

    // 2. Check each polygon edge for intersection with the cut line
    for (size_t i = 0; i < fvCount; ++i) {
        uint32_t a = face.vertices[i];
        uint32_t b = face.vertices[(i + 1) % fvCount];
        auto [uA, vA] = to2D(vertices[a].position);
        auto [uB, vB] = to2D(vertices[b].position);

        if (std::abs(vA) <= tol || std::abs(vB) <= tol) continue;

        if ((vA > 0.0f && vB < 0.0f) || (vA < 0.0f && vB > 0.0f)) {
            float t = -vA / (vB - vA);
            if (t > 1e-4f && t < 1.0f - 1e-4f) {
                Engine::Math::Vector3 pos = vertices[a].position + (vertices[b].position - vertices[a].position) * t;
                float uInterp = uA + (uB - uA) * t;
                hits.push_back({0xFFFFFFFF, a, b, pos, uInterp});
            }
        }
    }

    if (hits.size() < 2) return false;

    // Deduplicate hits that are too close to each other
    std::sort(hits.begin(), hits.end(), [](const Intersection& h1, const Intersection& h2) {
        return h1.uVal < h2.uVal;
    });

    std::vector<Intersection> uniqueHits;
    for (const auto& hit : hits) {
        if (uniqueHits.empty() || (hit.pos - uniqueHits.back().pos).length() > tol) {
            uniqueHits.push_back(hit);
        }
    }
    if (uniqueHits.size() < 2) return false;

    Intersection h0 = uniqueHits.front();
    Intersection h1 = uniqueHits.back();

    if (h0.vertex == 0xFFFFFFFF) {
        h0.vertex = mesh.addVertex(h0.pos, 0.5f, 0.5f, fn);
    }
    if (h1.vertex == 0xFFFFFFFF) {
        h1.vertex = mesh.addVertex(h1.pos, 0.5f, 0.5f, fn);
    }

    if (h0.vertex == h1.vertex) return false;

    auto insertOnAdjacentFaces = [&](const Intersection& hit) {
        if (hit.a == hit.b) return;
        for (size_t f = 0; f < faces.size(); ++f) {
            if (f == faceIndex || faces[f].deleted) continue;
            auto& verts = faces[f].vertices;
            for (size_t i = 0; i < verts.size(); ++i) {
                const uint32_t a = verts[i];
                const uint32_t b = verts[(i + 1) % verts.size()];
                if ((a == hit.a && b == hit.b) || (a == hit.b && b == hit.a)) {
                    verts.insert(verts.begin() + static_cast<std::ptrdiff_t>(i + 1), hit.vertex);
                    break;
                }
            }
        }
    };
    insertOnAdjacentFaces(h0);
    insertOnAdjacentFaces(h1);

    std::vector<uint32_t> expanded;
    for (size_t i = 0; i < fvCount; ++i) {
        const uint32_t a = face.vertices[i];
        const uint32_t b = face.vertices[(i + 1) % fvCount];
        expanded.push_back(a);
        if (h0.a != h0.b && ((a == h0.a && b == h0.b) || (a == h0.b && b == h0.a))) expanded.push_back(h0.vertex);
        if (h1.a != h1.b && ((a == h1.a && b == h1.b) || (a == h1.b && b == h1.a))) expanded.push_back(h1.vertex);
    }

    std::vector<uint32_t> cleanExpanded;
    for (uint32_t v : expanded) {
        if (cleanExpanded.empty() || cleanExpanded.back() != v) {
            cleanExpanded.push_back(v);
        }
    }
    if (cleanExpanded.size() > 1 && cleanExpanded.front() == cleanExpanded.back()) {
        cleanExpanded.pop_back();
    }
    expanded = cleanExpanded;

    auto findIndex = [&](uint32_t v) -> size_t {
        return static_cast<size_t>(std::find(expanded.begin(), expanded.end(), v) - expanded.begin());
    };
    const size_t i0 = findIndex(h0.vertex);
    const size_t i1 = findIndex(h1.vertex);
    if (i0 >= expanded.size() || i1 >= expanded.size() || i0 == i1) return false;

    auto path = [&](size_t start, size_t end) {
        std::vector<uint32_t> result;
        size_t i = start;
        for (size_t n = 0; n <= expanded.size(); ++n) {
            result.push_back(expanded[i]);
            if (i == end) break;
            i = (i + 1) % expanded.size();
        }
        return result;
    };
    const auto first = path(i0, i1);
    const auto second = path(i1, i0);
    if (first.size() < 3 || second.size() < 3) return false;

    mesh.removeFace(faceIndex);
    mesh.addFace(first);
    mesh.addFace(second);
    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
    if (compactResult) mesh.packAndCompact();
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

// ── Polygon-BSP Boolean CSG ─────────────────────────────────────────────────
// This is the classic BSP solid-geometry algorithm. It splits arbitrary
// planar polygons against the other solid's planes, so concave closed meshes
// are supported as well (unlike the previous convex half-space shortcut).
namespace {
using Vec3 = Engine::Math::Vector3;
struct CsgVertex { Vec3 p; std::pair<float, float> uv{0.0f, 0.0f}; };
struct Plane { Vec3 n; float d = 0.0f; }; // inside is n dot p + d <= eps

struct BspPolygon { std::vector<CsgVertex> vertices; Plane plane; };

static BspPolygon makeBspPolygon(const std::vector<CsgVertex>& vertices) {
    BspPolygon p{vertices, {}};
    if (vertices.size() >= 3) {
        Vec3 n = (vertices[1].p - vertices[0].p).cross(vertices[2].p - vertices[0].p).normalized();
        p.plane = {n, -n.dot(vertices[0].p)};
    }
    return p;
}

static void splitPolygon(const Plane& plane, const BspPolygon& polygon,
                         std::vector<BspPolygon>& coplanarFront,
                         std::vector<BspPolygon>& coplanarBack,
                         std::vector<BspPolygon>& front,
                         std::vector<BspPolygon>& back) {
    constexpr float eps = 1e-5f;
    int polygonType = 0;
    std::vector<int> types;
    for (const auto& v : polygon.vertices) {
        float t = plane.n.dot(v.p) + plane.d;
        int type = t < -eps ? 1 : (t > eps ? 2 : 0);
        polygonType |= type;
        types.push_back(type);
    }
    if (polygonType == 0) {
        (plane.n.dot(polygon.plane.n) >= 0.0f ? coplanarFront : coplanarBack).push_back(polygon);
    } else if (polygonType == 1) back.push_back(polygon);
    else if (polygonType == 2) front.push_back(polygon);
    else {
        std::vector<CsgVertex> f, b;
        for (size_t i = 0; i < polygon.vertices.size(); ++i) {
            const auto& a = polygon.vertices[i];
            const auto& c = polygon.vertices[(i + 1) % polygon.vertices.size()];
            int ta = types[i], tc = types[(i + 1) % types.size()];
            if (ta != 1) f.push_back(a);
            if (ta != 2) b.push_back(a);
            if ((ta | tc) == 3) {
                float da = plane.n.dot(a.p) + plane.d;
                float dc = plane.n.dot(c.p) + plane.d;
                float u = std::abs(da - dc) > 1e-8f ? da / (da - dc) : 0.5f;
                u = std::clamp(u, 0.0f, 1.0f);
                CsgVertex x;
                x.p = a.p + (c.p - a.p) * u;
                x.uv = {a.uv.first + (c.uv.first - a.uv.first) * u,
                        a.uv.second + (c.uv.second - a.uv.second) * u};
                f.push_back(x); b.push_back(x);
            }
        }
        if (f.size() >= 3) front.push_back(makeBspPolygon(f));
        if (b.size() >= 3) back.push_back(makeBspPolygon(b));
    }
}

class BspNode {
public:
    std::optional<Plane> plane;
    std::vector<BspPolygon> polygons;
    std::unique_ptr<BspNode> front, back;
    BspNode() = default;
    explicit BspNode(const std::vector<BspPolygon>& p) { build(p); }
    BspNode(const BspNode& o) : plane(o.plane), polygons(o.polygons) {
        if (o.front) front = std::make_unique<BspNode>(*o.front);
        if (o.back) back = std::make_unique<BspNode>(*o.back);
    }
    std::vector<BspPolygon> allPolygons() const {
        auto out = polygons;
        if (front) { auto p = front->allPolygons(); out.insert(out.end(), p.begin(), p.end()); }
        if (back) { auto p = back->allPolygons(); out.insert(out.end(), p.begin(), p.end()); }
        return out;
    }
    void invert() {
        for (auto& p : polygons) { std::reverse(p.vertices.begin(), p.vertices.end()); p.plane.n = p.plane.n * -1.0f; p.plane.d = -p.plane.d; }
        if (plane) { plane->n = plane->n * -1.0f; plane->d = -plane->d; }
        if (front) front->invert(); if (back) back->invert(); std::swap(front, back);
    }
    void build(const std::vector<BspPolygon>& input) {
        if (input.empty()) return;
        if (!plane) plane = input.front().plane;
        std::vector<BspPolygon> f, b, cf, cb;
        for (const auto& p : input) {
            cf.clear(); cb.clear(); splitPolygon(*plane, p, cf, cb, f, b);
            polygons.insert(polygons.end(), cf.begin(), cf.end());
            polygons.insert(polygons.end(), cb.begin(), cb.end());
        }
        if (!f.empty()) { if (!front) front = std::make_unique<BspNode>(); front->build(f); }
        if (!b.empty()) { if (!back) back = std::make_unique<BspNode>(); back->build(b); }
    }
    std::vector<BspPolygon> clipPolygons(const std::vector<BspPolygon>& input) const {
        if (!plane) return input;
        std::vector<BspPolygon> f, b, cf, cb;
        for (const auto& p : input) {
            splitPolygon(*plane, p, cf, cb, f, b);
        }
        f.insert(f.end(), cf.begin(), cf.end());
        if (front) f = front->clipPolygons(f);
        if (back) b = back->clipPolygons(b);
        else b.clear();
        f.insert(f.end(), b.begin(), b.end());
        return f;
    }
    void clipTo(const BspNode& other) { polygons = other.clipPolygons(polygons); if (front) front->clipTo(other); if (back) back->clipTo(other); }
};

static std::vector<BspPolygon> meshToBsp(const EditableMesh& mesh) {
    std::vector<BspPolygon> out;
    const auto& verts = mesh.getVertices();
    for (const auto& f : mesh.getFaces()) {
        if (f.deleted || f.vertices.size() < 3) continue;
        std::vector<CsgVertex> poly;
        for (size_t i = 0; i < f.vertices.size(); ++i) {
            uint32_t vi = f.vertices[i]; if (vi >= verts.size() || verts[vi].deleted) { poly.clear(); break; }
            poly.push_back({verts[vi].position, i < f.uvs.size() ? f.uvs[i] : std::make_pair(verts[vi].u, verts[vi].v)});
        }
        if (poly.size() >= 3) out.push_back(makeBspPolygon(poly));
    }
    return out;
}

static bool isConvex(const EditableMesh& mesh, std::vector<Plane>& planes) {
    const auto& faces = mesh.getFaces();
    const auto& verts = mesh.getVertices();
    Vec3 minP, maxP; mesh.computeBounds(minP, maxP);
    Vec3 center = (minP + maxP) * 0.5f;
    for (const auto& f : faces) {
        if (f.deleted || f.vertices.size() < 3) continue;
        Vec3 n = f.normal.normalized();
        if (n.length() < 0.5f) return false;
        Vec3 fc = verts[f.vertices[0]].position;
        // Ensure the plane points outward, independent of input winding.
        if (n.dot(f.center - center) < 0.0f) n = n * -1.0f;
        Plane plane{n, -n.dot(fc)};
        for (const auto& v : verts) {
            if (v.deleted) continue;
            if (plane.n.dot(v.position) + plane.d > 1e-4f) return false;
        }
        planes.push_back(plane);
    }
    return !planes.empty();
}

static std::vector<CsgVertex> clipHalfSpace(const std::vector<CsgVertex>& poly, const Plane& plane, bool keepInside) {
    std::vector<CsgVertex> out;
    if (poly.empty()) return out;
    auto value = [&](const CsgVertex& v) { return plane.n.dot(v.p) + plane.d; };
    auto accepted = [&](float x) { return keepInside ? x <= 1e-5f : x >= -1e-5f; };
    for (size_t i = 0; i < poly.size(); ++i) {
        const CsgVertex& a = poly[i];
        const CsgVertex& b = poly[(i + 1) % poly.size()];
        float da = value(a), db = value(b);
        bool ina = accepted(da), inb = accepted(db);
        if (ina) out.push_back(a);
        if (ina != inb) {
            float denom = da - db;
            float t = std::abs(denom) > 1e-8f ? da / denom : 0.0f;
            t = std::clamp(t, 0.0f, 1.0f);
            CsgVertex x;
            x.p = a.p + (b.p - a.p) * t;
            x.uv = {a.uv.first + (b.uv.first - a.uv.first) * t,
                    a.uv.second + (b.uv.second - a.uv.second) * t};
            out.push_back(x);
        }
    }
    return out;
}

using CsgPiece = std::pair<std::vector<CsgVertex>, bool>; // polygon, reverse winding
static void collectPieces(const EditableMesh& source, const std::vector<Plane>& cutter,
                          bool keepInside, bool reverse, std::vector<CsgPiece>& pieces) {
    const auto& verts = source.getVertices();
    for (const auto& f : source.getFaces()) {
        if (f.deleted || f.vertices.size() < 3) continue;
        std::vector<CsgVertex> poly;
        for (size_t i = 0; i < f.vertices.size(); ++i) {
            uint32_t vi = f.vertices[i];
            if (vi >= verts.size() || verts[vi].deleted) { poly.clear(); break; }
            poly.push_back({verts[vi].position,
                i < f.uvs.size() ? f.uvs[i] : std::make_pair(verts[vi].u, verts[vi].v)});
        }
        if (poly.size() < 3) continue;

        if (keepInside) {
            for (const auto& plane : cutter) poly = clipHalfSpace(poly, plane, true);
            if (poly.size() >= 3) pieces.emplace_back(std::move(poly), reverse);
        } else {
            // The complement of an intersection of half-spaces is a union.
            // Split successively; emit the outside branch and continue only
            // with the inside branch, preventing duplicate surface fragments.
            for (const auto& plane : cutter) {
                auto outside = clipHalfSpace(poly, plane, false);
                if (outside.size() >= 3) pieces.emplace_back(std::move(outside), reverse);
                poly = clipHalfSpace(poly, plane, true);
                if (poly.size() < 3) break;
            }
        }
    }
}
}

std::shared_ptr<EditableMesh> MeshCutOperators::applyBoolean(
    const EditableMesh& meshA, const EditableMesh& meshB, BooleanOperation op) {
    auto pa = meshToBsp(meshA), pb = meshToBsp(meshB);
    if (pa.empty() || pb.empty()) return nullptr;
    BspNode a(pa), b(pb);
    std::vector<BspPolygon> pieces;
    if (op == BooleanOperation::Union) {
        a.clipTo(b); b.clipTo(a); b.invert(); b.clipTo(a); b.invert(); a.build(b.allPolygons()); pieces = a.allPolygons();
    } else if (op == BooleanOperation::Difference) {
        a.invert(); a.clipTo(b); b.clipTo(a); b.invert(); b.clipTo(a); b.invert(); a.build(b.allPolygons()); a.invert(); pieces = a.allPolygons();
    } else {
        a.invert(); b.clipTo(a); b.invert(); a.clipTo(b); b.clipTo(a); a.build(b.allPolygons()); a.invert(); pieces = a.allPolygons();
    }
    if (pieces.empty()) return nullptr;

    auto result = std::make_shared<EditableMesh>();
    std::map<std::tuple<long long, long long, long long>, uint32_t> welded;
    auto getOrAdd = [&](const CsgVertex& v) {
        constexpr double q = 100000.0;
        auto key = std::make_tuple(static_cast<long long>(std::llround(v.p.x * q)),
                                   static_cast<long long>(std::llround(v.p.y * q)),
                                   static_cast<long long>(std::llround(v.p.z * q)));
        auto it = welded.find(key);
        if (it != welded.end()) return it->second;
        uint32_t id = result->addVertex(v.p, v.uv.first, v.uv.second);
        welded.emplace(key, id);
        return id;
    };
    for (auto& piece : pieces) {
        auto& poly = piece.vertices;
        std::vector<uint32_t> ids;
        std::vector<std::pair<float, float>> uvs;
        for (const auto& v : poly) { ids.push_back(getOrAdd(v)); uvs.push_back(v.uv); }
        if (ids.size() >= 3) result->addFaceWithUVs(ids, uvs);
    }
    result->rebuildTopology();
    result->recalculateAllNormals(false);
    return result->validate() ? result : nullptr;
}

} // namespace Engine::Geometry
