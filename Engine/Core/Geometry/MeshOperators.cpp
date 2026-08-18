#include "MeshOperators.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <limits>
#include <map>
#include <set>

namespace Engine::Geometry {

namespace {

int addFaceWithAttributes(
    EditableMesh& mesh,
    const std::vector<uint32_t>& vertices,
    const MeshFace& source,
    const std::vector<size_t>& sourceCorners = {}
) {
    const int faceIndex = mesh.addFace(vertices);
    if (faceIndex < 0 || faceIndex >= static_cast<int>(mesh.getFaces().size())) return faceIndex;

    auto& destination = mesh.getFaces()[faceIndex];
    destination.materialId = source.materialId;
    if (source.uvs.size() == source.vertices.size()) {
        destination.uvs.reserve(vertices.size());
        for (size_t i = 0; i < vertices.size(); ++i) {
            const size_t sourceIndex = sourceCorners.empty()
                ? std::min(i, source.uvs.size() - 1)
                : std::min(sourceCorners[i], source.uvs.size() - 1);
            destination.uvs.push_back(source.uvs[sourceIndex]);
        }
    }
    return faceIndex;
}

} // namespace

// ── 1. Extrude Faces ────────────────────────────────────────────────────────
std::vector<uint32_t> MeshOperators::extrudeFaces(
    EditableMesh& mesh,
    const std::vector<uint32_t>& faceIndices,
    float distance,
    const Engine::Math::Vector3& customDirection,
    bool individual
) {
    std::vector<uint32_t> newFaces;
    if (faceIndices.empty()) return newFaces;

    auto& vertices = mesh.getVertices();
    auto& faces = mesh.getFaces();

    std::set<uint32_t> selFaceSet;
    for (uint32_t f : faceIndices) {
        if (f < faces.size() && !faces[f].deleted && faces[f].vertices.size() >= 3) {
            selFaceSet.insert(f);
        }
    }
    if (selFaceSet.empty()) return newFaces;

    if (individual) {
        // Individual Face Extrusion: each face gets isolated copies and all side walls
        for (uint32_t fIdx : selFaceSet) {
            auto& face = faces[fIdx];
            const MeshFace sourceFace = face;
            mesh.calculateFaceNormal(fIdx);

            Engine::Math::Vector3 dir = (customDirection.length() > 0.0001f)
                ? customDirection.normalized()
                : face.normal;
            Engine::Math::Vector3 offset = dir * distance;

            size_t vertCount = face.vertices.size();
            std::vector<uint32_t> originalVerts = face.vertices;
            std::vector<uint32_t> extrudedVerts(vertCount);

            for (size_t i = 0; i < vertCount; ++i) {
                uint32_t oldV = originalVerts[i];
                const auto& srcV = vertices[oldV];
                extrudedVerts[i] = mesh.addVertex(srcV.position + offset, srcV.u, srcV.v, srcV.normal);
            }

            face.vertices = extrudedVerts;
            newFaces.push_back(fIdx);

            for (size_t i = 0; i < vertCount; ++i) {
                size_t next = (i + 1) % vertCount;
                uint32_t b0 = originalVerts[i];
                uint32_t b1 = originalVerts[next];
                uint32_t t1 = extrudedVerts[next];
                uint32_t t0 = extrudedVerts[i];
                addFaceWithAttributes(mesh, {b0, b1, t1, t0}, sourceFace, {i, next, next, i});
            }
        }
    } else {
        // Region Extrusion: seamless group extrusion without internal dividing walls
        // 1. Calculate average normal of selected region
        Engine::Math::Vector3 avgNormal(0, 0, 0);
        for (uint32_t fIdx : selFaceSet) {
            mesh.calculateFaceNormal(fIdx);
            avgNormal += faces[fIdx].normal;
        }
        Engine::Math::Vector3 dir = (customDirection.length() > 0.0001f)
            ? customDirection.normalized()
            : (avgNormal.length() > 1e-4f ? avgNormal.normalized() : Engine::Math::Vector3(0, 1, 0));
        Engine::Math::Vector3 offset = dir * distance;

        // 2. Count occurrences of directed edges in the selection
        // Edges appearing exactly once are boundary edges; edges appearing twice are internal shared edges.
        std::map<std::pair<uint32_t, uint32_t>, int> directedEdgeCount;
        std::map<std::pair<uint32_t, uint32_t>, std::pair<uint32_t, size_t>> edgeOwners;
        std::set<uint32_t> uniqueVerts;

        for (uint32_t fIdx : selFaceSet) {
            const auto& fVerts = faces[fIdx].vertices;
            size_t count = fVerts.size();
            for (size_t i = 0; i < count; ++i) {
                uint32_t v0 = fVerts[i];
                uint32_t v1 = fVerts[(i + 1) % count];
                directedEdgeCount[{v0, v1}]++;
                edgeOwners[{v0, v1}] = {fIdx, i};
                uniqueVerts.insert(v0);
            }
        }

        // 3. Create a single extruded copy for each unique vertex in the region
        std::map<uint32_t, uint32_t> oldToNewVertMap;
        for (uint32_t oldV : uniqueVerts) {
            const auto& srcV = vertices[oldV];
            oldToNewVertMap[oldV] = mesh.addVertex(srcV.position + offset, srcV.u, srcV.v, srcV.normal);
        }

        // 4. Update the selected faces to use their corresponding new extruded vertices
        for (uint32_t fIdx : selFaceSet) {
            auto& face = faces[fIdx];
            for (auto& v : face.vertices) {
                v = oldToNewVertMap[v];
            }
            newFaces.push_back(fIdx);
        }

        // 5. Create side quad walls ONLY along the true outer boundary edges
        for (const auto& [edge, count] : directedEdgeCount) {
            if (count == 1) {
                // Check if opposite directed edge is present in selection
                if (directedEdgeCount.find({edge.second, edge.first}) == directedEdgeCount.end()) {
                    uint32_t b0 = edge.first;
                    uint32_t b1 = edge.second;
                    uint32_t t1 = oldToNewVertMap[b1];
                    uint32_t t0 = oldToNewVertMap[b0];
                    const auto ownerIt = edgeOwners.find(edge);
                    if (ownerIt != edgeOwners.end()) {
                        const auto sourceFace = faces[ownerIt->second.first];
                        const size_t corner = ownerIt->second.second;
                        const size_t next = (corner + 1) % sourceFace.vertices.size();
                        addFaceWithAttributes(mesh, {b0, b1, t1, t0}, sourceFace,
                                              {corner, next, next, corner});
                    } else {
                        mesh.addFace({b0, b1, t1, t0});
                    }
                }
            }
        }
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
    return newFaces;
}

std::vector<uint32_t> MeshOperators::extrudeEdges(
    EditableMesh& mesh,
    const std::vector<uint32_t>& edgeIndices,
    float distance,
    const Engine::Math::Vector3& direction
) {
    std::vector<uint32_t> newFaces;
    auto& edges = mesh.getEdges();
    auto& vertices = mesh.getVertices();
    
    Engine::Math::Vector3 dir(0, 0, 0);
    if (direction.length() > 1e-4f) {
        dir = direction.normalized();
    } else {
        // Calculate average normal across incident faces of valid edges
        for (uint32_t eIdx : edgeIndices) {
            if (eIdx >= edges.size() || edges[eIdx].deleted) continue;
            auto edgeFaces = mesh.getEdgeFaces(eIdx);
            for (uint32_t f : edgeFaces) {
                if (f < mesh.getFaces().size() && !mesh.getFaces()[f].deleted) {
                    mesh.calculateFaceNormal(f);
                    dir += mesh.getFaces()[f].normal;
                }
            }
        }
        if (dir.length() > 1e-4f) {
            dir = dir.normalized();
        } else {
            dir = Engine::Math::Vector3(0, 1, 0);
        }
    }
    Engine::Math::Vector3 offset = dir * distance;

    std::set<uint32_t> uniqueVerts;
    std::vector<uint32_t> validEdges;
    std::map<uint32_t, MeshFace> edgeSourceFaces;

    for (uint32_t eIdx : edgeIndices) {
        if (eIdx >= edges.size() || edges[eIdx].deleted) continue;
        uint32_t v0 = edges[eIdx].v0;
        uint32_t v1 = edges[eIdx].v1;
        if (v0 >= vertices.size() || v1 >= vertices.size()) continue;

        uniqueVerts.insert(v0);
        uniqueVerts.insert(v1);
        validEdges.push_back(eIdx);
        const auto edgeFaces = mesh.getEdgeFaces(eIdx);
        if (!edgeFaces.empty() && edgeFaces.front() < mesh.getFaces().size()) {
            edgeSourceFaces.emplace(eIdx, mesh.getFaces()[edgeFaces.front()]);
        }
    }

    if (validEdges.empty()) return newFaces;

    // Create a single extruded counterpart for each unique vertex in the selection
    std::map<uint32_t, uint32_t> oldToNew;
    for (uint32_t v : uniqueVerts) {
        const auto& srcV = vertices[v];
        oldToNew[v] = mesh.addVertex(srcV.position + offset, srcV.u, srcV.v, srcV.normal);
    }

    // Build connected quad ribbon
    for (uint32_t eIdx : validEdges) {
        uint32_t v0 = edges[eIdx].v0;
        uint32_t v1 = edges[eIdx].v1;
        uint32_t newV0 = oldToNew[v0];
        uint32_t newV1 = oldToNew[v1];

        int f = -1;
        const auto sourceIt = edgeSourceFaces.find(eIdx);
        if (sourceIt != edgeSourceFaces.end()) {
            size_t cornerV0 = 0;
            size_t cornerV1 = 0;
            for (size_t i = 0; i < sourceIt->second.vertices.size(); ++i) {
                if (sourceIt->second.vertices[i] == v0) cornerV0 = i;
                if (sourceIt->second.vertices[i] == v1) cornerV1 = i;
            }
            f = addFaceWithAttributes(mesh, {v0, v1, newV1, newV0}, sourceIt->second,
                                      {cornerV0, cornerV1, cornerV1, cornerV0});
        } else {
            f = mesh.addFace({v0, v1, newV1, newV0});
        }
        if (f >= 0) newFaces.push_back(static_cast<uint32_t>(f));
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
    return newFaces;
}

std::vector<uint32_t> MeshOperators::extrudeVertices(
    EditableMesh& mesh,
    const std::vector<uint32_t>& vertIndices,
    float distance,
    const Engine::Math::Vector3& direction
) {
    std::vector<uint32_t> newVerts;
    auto& vertices = mesh.getVertices();
    Engine::Math::Vector3 dir(0, 0, 0);
    if (direction.length() > 1e-4f) {
        dir = direction.normalized();
    } else {
        for (uint32_t v : vertIndices) {
            if (v < vertices.size() && !vertices[v].deleted) {
                auto connFaces = mesh.getConnectedFaces(v);
                for (uint32_t f : connFaces) {
                    if (f < mesh.getFaces().size() && !mesh.getFaces()[f].deleted) {
                        mesh.calculateFaceNormal(f);
                        dir += mesh.getFaces()[f].normal;
                    }
                }
            }
        }
        if (dir.length() > 1e-4f) dir = dir.normalized();
        else dir = Engine::Math::Vector3(0, 1, 0);
    }
    Engine::Math::Vector3 offset = dir * distance;

    std::set<uint32_t> uniqueVerts;
    std::vector<uint32_t> validVerts;
    for (uint32_t v : vertIndices) {
        if (v < vertices.size() && !vertices[v].deleted && uniqueVerts.insert(v).second) {
            validVerts.push_back(v);
        }
    }
    if (validVerts.empty()) return newVerts;

    for (uint32_t v : validVerts) {
        const auto& src = vertices[v];
        uint32_t nv = mesh.addVertex(src.position + offset, src.u, src.v, src.normal);
        if (mesh.findEdge(v, nv) < 0) mesh.addEdge(v, nv);
        newVerts.push_back(nv);
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
    return newVerts;
}

void MeshOperators::shrinkFatten(
    EditableMesh& mesh,
    const std::vector<uint32_t>& vertIndices,
    float offset
) {
    auto& vertices = mesh.getVertices();
    for (uint32_t v : vertIndices) {
        if (v >= vertices.size() || vertices[v].deleted) continue;

        Engine::Math::Vector3 avgNorm(0, 0, 0);
        auto connFaces = mesh.getConnectedFaces(v);
        for (uint32_t f : connFaces) {
            if (f < mesh.getFaces().size() && !mesh.getFaces()[f].deleted) {
                mesh.calculateFaceNormal(f);
                avgNorm += mesh.getFaces()[f].normal;
            }
        }
        if (avgNorm.length() > 1e-4f) {
            avgNorm = avgNorm.normalized();
        } else if (vertices[v].normal.length() > 1e-4f) {
            avgNorm = vertices[v].normal.normalized();
        } else {
            avgNorm = Engine::Math::Vector3(0, 1, 0);
        }

        vertices[v].position += avgNorm * offset;
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
}

// ── 2. Inset Faces ──────────────────────────────────────────────────────────
std::vector<uint32_t> MeshOperators::insetFaces(
    EditableMesh& mesh,
    const std::vector<uint32_t>& faceIndices,
    float thickness,
    float depth,
    bool individual
) {
    std::vector<uint32_t> innerFaces;
    auto& vertices = mesh.getVertices();
    auto& faces = mesh.getFaces();

    std::set<uint32_t> selFaceSet;
    for (uint32_t f : faceIndices) {
        if (f < faces.size() && !faces[f].deleted && faces[f].vertices.size() >= 3) {
            selFaceSet.insert(f);
        }
    }
    if (selFaceSet.empty()) return innerFaces;

    if (individual || selFaceSet.size() == 1) {
        for (uint32_t fIdx : selFaceSet) {
            auto& face = faces[fIdx];
            mesh.calculateFaceNormal(fIdx);

            Engine::Math::Vector3 normal = face.normal;
            size_t count = face.vertices.size();
            std::vector<uint32_t> outerVerts = face.vertices;
            std::vector<uint32_t> innerVerts(count);
            int origMatId = face.materialId;

            Engine::Math::Vector3 center(0, 0, 0);
            for (uint32_t v : outerVerts) center += vertices[v].position;
            center = center * (1.0f / (float)count);

            float minPerpDist = 1e9f;
            for (size_t i = 0; i < count; ++i) {
                size_t next = (i + 1) % count;
                const auto& pA = vertices[outerVerts[i]].position;
                const auto& pB = vertices[outerVerts[next]].position;
                float d = ((pA + pB) * 0.5f - center).length();
                if (d < minPerpDist) minPerpDist = d;
            }

            bool shouldCollapse = (thickness >= minPerpDist * 0.98f && minPerpDist > 1e-4f);

            if (shouldCollapse) {
                uint32_t centerV = mesh.addVertex(center + normal * depth, 0.5f, 0.5f, normal);
                mesh.removeFace(fIdx);
                for (size_t i = 0; i < count; ++i) {
                    size_t next = (i + 1) % count;
                    uint32_t o0 = outerVerts[i];
                    uint32_t o1 = outerVerts[next];
                    int rIdx = mesh.addFace({o0, o1, centerV});
                    if (rIdx >= 0 && rIdx < static_cast<int>(faces.size())) {
                        faces[rIdx].materialId = origMatId;
                        innerFaces.push_back(static_cast<uint32_t>(rIdx));
                    }
                }
            } else {
                for (size_t i = 0; i < count; ++i) {
                    size_t prev = (i + count - 1) % count;
                    size_t next = (i + 1) % count;

                    uint32_t vPrev = outerVerts[prev];
                    uint32_t vCurr = outerVerts[i];
                    uint32_t vNext = outerVerts[next];

                    const auto& pPrev = vertices[vPrev].position;
                    const auto& pCurr = vertices[vCurr].position;
                    const auto& pNext = vertices[vNext].position;

                    Engine::Math::Vector3 ePrev = (pCurr - pPrev).normalized();
                    Engine::Math::Vector3 eNext = (pNext - pCurr).normalized();

                    Engine::Math::Vector3 nPrev = normal.cross(ePrev).normalized();
                    Engine::Math::Vector3 nNext = normal.cross(eNext).normalized();

                    float dotNorm = nPrev.dot(nNext);
                    Engine::Math::Vector3 miterBisect = (nPrev + nNext);
                    if (dotNorm > -0.999f && (1.0f + dotNorm) > 1e-4f) {
                        miterBisect = miterBisect * (1.0f / (1.0f + dotNorm));
                    } else {
                        miterBisect = nPrev;
                    }

                    Engine::Math::Vector3 inPos = pCurr + miterBisect * thickness + normal * depth;
                    innerVerts[i] = mesh.addVertex(inPos, vertices[vCurr].u, vertices[vCurr].v, normal);
                }

                face.vertices = innerVerts;
                innerFaces.push_back(fIdx);

                for (size_t i = 0; i < count; ++i) {
                    size_t next = (i + 1) % count;
                    uint32_t o0 = outerVerts[i];
                    uint32_t o1 = outerVerts[next];
                    uint32_t i1 = innerVerts[next];
                    uint32_t i0 = innerVerts[i];

                    int rimFaceIdx = mesh.addFace({o0, o1, i1, i0});
                    if (rimFaceIdx >= 0 && rimFaceIdx < static_cast<int>(faces.size())) {
                        faces[rimFaceIdx].materialId = origMatId;
                    }
                }
            }
        }
    } else {
        // Region Inset
        std::map<std::pair<uint32_t, uint32_t>, int> directedEdgeCount;
        std::map<std::pair<uint32_t, uint32_t>, std::pair<uint32_t, size_t>> edgeOwners;
        for (uint32_t fIdx : selFaceSet) {
            const auto& fVerts = faces[fIdx].vertices;
            size_t count = fVerts.size();
            for (size_t i = 0; i < count; ++i) {
                uint32_t v0 = fVerts[i];
                uint32_t v1 = fVerts[(i + 1) % count];
                directedEdgeCount[{v0, v1}]++;
                edgeOwners[{v0, v1}] = {fIdx, i};
            }
        }

        std::set<std::pair<uint32_t, uint32_t>> boundaryEdges;
        std::set<uint32_t> boundaryVerts;
        for (const auto& [edge, count] : directedEdgeCount) {
            if (count == 1 && directedEdgeCount.find({edge.second, edge.first}) == directedEdgeCount.end()) {
                boundaryEdges.insert(edge);
                boundaryVerts.insert(edge.first);
                boundaryVerts.insert(edge.second);
            }
        }

        // Calculate maximum safe thickness for the region before any face collapses
        float minRegionEdgeLen = 1e9f;
        for (const auto& [b0, b1] : boundaryEdges) {
            float len = (vertices[b1].position - vertices[b0].position).length();
            if (len < minRegionEdgeLen) minRegionEdgeLen = len;
        }
        float safeRegionThickness = std::max(0.0f, std::min(thickness, minRegionEdgeLen * 0.48f));

        std::map<uint32_t, uint32_t> oldToNewBoundaryVert;
        for (uint32_t vIdx : boundaryVerts) {
            const auto& srcV = vertices[vIdx];
            Engine::Math::Vector3 nIn(0, 0, 0);
            Engine::Math::Vector3 nOut(0, 0, 0);
            Engine::Math::Vector3 avgNorm(0, 0, 0);
            bool hasIn = false;
            bool hasOut = false;

            for (const auto& [e0, e1] : boundaryEdges) {
                if (e1 == vIdx) { // incoming: e0 -> vIdx
                    const auto owner = edgeOwners.find({e0, e1});
                    if (owner != edgeOwners.end()) {
                        uint32_t ownerF = owner->second.first;
                        mesh.calculateFaceNormal(ownerF);
                        Engine::Math::Vector3 fn = faces[ownerF].normal;
                        Engine::Math::Vector3 edgeDir = (srcV.position - vertices[e0].position).normalized();
                        nIn = fn.cross(edgeDir).normalized();
                        avgNorm += fn;
                        hasIn = true;
                    }
                }
                if (e0 == vIdx) { // outgoing: vIdx -> e1
                    const auto owner = edgeOwners.find({e0, e1});
                    if (owner != edgeOwners.end()) {
                        uint32_t ownerF = owner->second.first;
                        mesh.calculateFaceNormal(ownerF);
                        Engine::Math::Vector3 fn = faces[ownerF].normal;
                        Engine::Math::Vector3 edgeDir = (vertices[e1].position - srcV.position).normalized();
                        nOut = fn.cross(edgeDir).normalized();
                        avgNorm += fn;
                        hasOut = true;
                    }
                }
            }

            Engine::Math::Vector3 miterBisect(0, 0, 0);
            if (hasIn && hasOut) {
                float dotNorm = nIn.dot(nOut);
                miterBisect = (nIn + nOut);
                if (dotNorm > -0.999f && (1.0f + dotNorm) > 1e-4f) {
                    miterBisect = miterBisect * (1.0f / (1.0f + dotNorm));
                } else {
                    miterBisect = nIn;
                }
            } else if (hasIn) {
                miterBisect = nIn;
            } else if (hasOut) {
                miterBisect = nOut;
            }

            Engine::Math::Vector3 normDir = (avgNorm.length() > 1e-4f) ? avgNorm.normalized() : Engine::Math::Vector3(0, 1, 0);
            Engine::Math::Vector3 inPos = srcV.position + miterBisect * safeRegionThickness + normDir * depth;
            oldToNewBoundaryVert[vIdx] = mesh.addVertex(inPos, srcV.u, srcV.v, normDir);
        }

        for (uint32_t fIdx : selFaceSet) {
            auto& face = faces[fIdx];
            for (auto& v : face.vertices) {
                if (oldToNewBoundaryVert.find(v) != oldToNewBoundaryVert.end()) {
                    v = oldToNewBoundaryVert[v];
                }
            }
            innerFaces.push_back(fIdx);
        }

        for (const auto& [b0, b1] : boundaryEdges) {
            uint32_t i0 = oldToNewBoundaryVert[b0];
            uint32_t i1 = oldToNewBoundaryVert[b1];
            const auto owner = edgeOwners.find({b0, b1});
            if (owner != edgeOwners.end()) {
                const MeshFace sourceFace = faces[owner->second.first];
                const size_t corner = owner->second.second;
                const size_t next = (corner + 1) % sourceFace.vertices.size();
                addFaceWithAttributes(mesh, {b0, b1, i1, i0}, sourceFace,
                                      {corner, next, next, corner});
            } else {
                mesh.addFace({b0, b1, i1, i0});
            }
        }
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
    return innerFaces;
}

// ── 3. Bevel / Chamfer ──────────────────────────────────────────────────────
std::vector<uint32_t> MeshOperators::bevelFaces(
    EditableMesh& mesh,
    const std::vector<uint32_t>& faceIndices,
    float width,
    int segments,
    float profile,
    float depth
) {
    std::vector<uint32_t> newFaces;
    if (faceIndices.empty() || width <= 0.0001f) return newFaces;
    auto& faces = mesh.getFaces();

    // Find all perimeter boundary edges of the selected faces to bevel
    std::map<std::pair<uint32_t, uint32_t>, int> edgeCounts;
    for (uint32_t fIdx : faceIndices) {
        if (fIdx >= faces.size() || faces[fIdx].deleted) continue;
        const auto& fVerts = faces[fIdx].vertices;
        size_t count = fVerts.size();
        for (size_t i = 0; i < count; ++i) {
            uint32_t v0 = fVerts[i];
            uint32_t v1 = fVerts[(i + 1) % count];
            edgeCounts[{std::min(v0, v1), std::max(v0, v1)}]++;
        }
    }

    std::vector<uint32_t> perimeterEdges;
    for (const auto& [edge, count] : edgeCounts) {
        int eIdx = mesh.findEdge(edge.first, edge.second);
        if (eIdx >= 0) {
            perimeterEdges.push_back(static_cast<uint32_t>(eIdx));
        }
    }

    if (!perimeterEdges.empty()) {
        bevelEdges(mesh, perimeterEdges, width, segments, profile);
        newFaces = faceIndices;
    }
    return newFaces;
}

std::vector<uint32_t> MeshOperators::bevelEdges(
    EditableMesh& mesh,
    const std::vector<uint32_t>& edgeIndices,
    float width,
    int segments,
    float profile
) {
    std::vector<uint32_t> newEdges;
    if (edgeIndices.empty() || width <= 0.0001f) return newEdges;
    auto& edges = mesh.getEdges();
    auto& faces = mesh.getFaces();
    auto& vertices = mesh.getVertices();

    segments = std::max(1, std::min(8, segments));
    profile = std::max(0.0f, std::min(1.0f, profile));
    std::set<uint32_t> generatedVertices;

    std::vector<uint32_t> workEdges;
    for (uint32_t eIdx : edgeIndices) {
        if (eIdx >= edges.size() || edges[eIdx].deleted) continue;
        workEdges.push_back(eIdx);
    }
    if (workEdges.empty()) return newEdges;

    for (uint32_t eIdx : workEdges) {
        if (eIdx >= edges.size() || edges[eIdx].deleted) continue;

        uint32_t v0 = edges[eIdx].v0;
        uint32_t v1 = edges[eIdx].v1;
        if (v0 >= vertices.size() || v1 >= vertices.size()) continue;

        const auto p0 = vertices[v0].position;
        const auto p1 = vertices[v1].position;
        Engine::Math::Vector3 edgeDir = (p1 - p0);
        float edgeLen = edgeDir.length();
        if (edgeLen < 0.0001f) continue;
        edgeDir = edgeDir * (1.0f / edgeLen);

        float bevelWidth = width;

        auto connectedFaces = mesh.getEdgeFaces(eIdx);
        if (connectedFaces.empty()) continue;

        uint32_t fA_idx = connectedFaces[0];
        if (fA_idx >= faces.size()) continue;

        const bool hasFaceB = (connectedFaces.size() >= 2 && connectedFaces[1] < faces.size());
        uint32_t fB_idx = hasFaceB ? connectedFaces[1] : fA_idx;

        mesh.calculateFaceNormal(fA_idx);
        const MeshFace sourceFaceA = faces[fA_idx];
        auto nA = faces[fA_idx].normal;

        Engine::Math::Vector3 centerA(0, 0, 0);
        for (uint32_t v : faces[fA_idx].vertices) centerA += vertices[v].position;
        if (!faces[fA_idx].vertices.empty()) centerA = centerA * (1.0f / (float)faces[fA_idx].vertices.size());

        Engine::Math::Vector3 tanA = nA.cross(edgeDir).normalized();
        if ((centerA - p0).dot(tanA) < 0.0f) tanA = tanA * -1.0f;

        Engine::Math::Vector3 tanB(0, 0, 0);
        MeshFace sourceFaceB = sourceFaceA;
        if (hasFaceB) {
            mesh.calculateFaceNormal(fB_idx);
            sourceFaceB = faces[fB_idx];
            auto nB = faces[fB_idx].normal;
            Engine::Math::Vector3 centerB(0, 0, 0);
            for (uint32_t v : faces[fB_idx].vertices) centerB += vertices[v].position;
            if (!faces[fB_idx].vertices.empty()) centerB = centerB * (1.0f / (float)faces[fB_idx].vertices.size());

            tanB = nB.cross(edgeDir).normalized();
            if ((centerB - p0).dot(tanB) < 0.0f) tanB = tanB * -1.0f;
        }

        std::vector<uint32_t> splitA0(segments + 1);
        std::vector<uint32_t> splitA1(segments + 1);

        for (int s = 0; s <= segments; ++s) {
            float u = (float)s / (float)segments;
            float linA = 1.0f - u;
            float linB = u;

            float rad = u * 1.57079632679f;
            float circA = std::cos(rad);
            float circB = std::sin(rad);

            float delta = (profile - 0.5f) * 2.0f;
            float wA, wB;
            if (delta >= 0.0f) {
                wA = linA + delta * (circA - linA);
                wB = linB + delta * (circB - linB);
            } else {
                float invDelta = -delta;
                float concaveA = 1.0f - circB;
                float concaveB = 1.0f - circA;
                wA = linA + invDelta * (concaveA - linA);
                wB = linB + invDelta * (concaveB - linB);
            }

            Engine::Math::Vector3 pt0 = hasFaceB
                ? (p0 + tanA * (bevelWidth * wA) + tanB * (bevelWidth * wB))
                : (p0 + tanA * (bevelWidth * (1.0f - wA)));
            Engine::Math::Vector3 pt1 = hasFaceB
                ? (p1 + tanA * (bevelWidth * wA) + tanB * (bevelWidth * wB))
                : (p1 + tanA * (bevelWidth * (1.0f - wA)));

            splitA0[s] = mesh.addVertex(pt0, vertices[v0].u, vertices[v0].v);
            splitA1[s] = mesh.addVertex(pt1, vertices[v1].u, vertices[v1].v);
            generatedVertices.insert(splitA0[s]);
            generatedVertices.insert(splitA1[s]);
        }

        size_t sourceCornerV0 = 0;
        size_t sourceCornerV1 = 0;
        for (size_t i = 0; i < sourceFaceA.vertices.size(); ++i) {
            if (sourceFaceA.vertices[i] == v0) sourceCornerV0 = i;
            if (sourceFaceA.vertices[i] == v1) sourceCornerV1 = i;
        }

        // Replace v0, v1 in Face A
        auto& fA = faces[fA_idx];
        for (auto& v : fA.vertices) {
            if (v == v0) v = hasFaceB ? splitA0[0] : splitA0[segments];
            else if (v == v1) v = hasFaceB ? splitA1[0] : splitA1[segments];
        }

        // Replace v0, v1 in Face B if manifold
        if (hasFaceB) {
            auto& fB = faces[fB_idx];
            for (auto& v : fB.vertices) {
                if (v == v0) v = splitA0[segments];
                else if (v == v1) v = splitA1[segments];
            }
        }

        // Connect intermediate segments with quads and preserve materialId
        for (int s = 0; s < segments; ++s) {
            uint32_t p0_s  = splitA0[s];
            uint32_t p1_s  = splitA1[s];
            uint32_t p0_s1 = splitA0[s + 1];
            uint32_t p1_s1 = splitA1[s + 1];

            addFaceWithAttributes(mesh, { p0_s, p1_s, p1_s1, p0_s1 }, sourceFaceA,
                                  {sourceCornerV0, sourceCornerV1, sourceCornerV1, sourceCornerV0});
        }

        // For all other adjacent faces meeting at endpoints, replace endpoint with the split vertex pair
        const size_t curFaceCount = faces.size();
        for (size_t f = 0; f < curFaceCount; ++f) {
            if (f == fA_idx || (hasFaceB && f == fB_idx) || faces[f].deleted) continue;
            auto& fVerts = faces[f].vertices;
            for (size_t vi = 0; vi < fVerts.size(); ++vi) {
                if (fVerts[vi] == v0) {
                    fVerts[vi] = splitA0[0];
                    fVerts.insert(fVerts.begin() + static_cast<std::ptrdiff_t>(vi + 1), splitA0[segments]);
                    break;
                } else if (fVerts[vi] == v1) {
                    fVerts[vi] = splitA1[segments];
                    fVerts.insert(fVerts.begin() + static_cast<std::ptrdiff_t>(vi + 1), splitA1[0]);
                    break;
                }
            }
        }
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
    for (size_t e = 0; e < mesh.getEdges().size(); ++e) {
        const auto& edge = mesh.getEdges()[e];
        if (!edge.deleted && (generatedVertices.count(edge.v0) || generatedVertices.count(edge.v1))) {
            newEdges.push_back(static_cast<uint32_t>(e));
        }
    }
    return newEdges;
}

std::vector<uint32_t> MeshOperators::bevelVertices(
    EditableMesh& mesh,
    const std::vector<uint32_t>& vertIndices,
    float width,
    int segments
) {
    std::vector<uint32_t> newVertices;
    if (vertIndices.empty() || width <= 0.0001f) return newVertices;
    auto& vertices = mesh.getVertices();
    segments = std::clamp(segments, 1, 8);

    for (uint32_t vIdx : vertIndices) {
        if (vIdx >= vertices.size() || vertices[vIdx].deleted) continue;

        const auto centerPos = vertices[vIdx].position;
        auto connectedFaces = mesh.getConnectedFaces(vIdx);
        if (connectedFaces.size() < 3) continue;

        struct CornerCut {
            MeshFace sourceFace;
            uint32_t prevVertex = 0;
            uint32_t nextVertex = 0;
            uint32_t prevCut = 0;
            uint32_t nextCut = 0;
            std::vector<uint32_t> arc;
        };
        std::vector<CornerCut> cuts;
        cuts.reserve(connectedFaces.size());
        vertices.reserve(vertices.size() + connectedFaces.size() * static_cast<size_t>(segments + 2));
        mesh.getFaces().reserve(mesh.getFaces().size() + connectedFaces.size() * 4 + 1);

        Engine::Math::Vector3 avgNormal(0, 0, 0);
        for (uint32_t fIdx : connectedFaces) {
            mesh.calculateFaceNormal(fIdx);
            avgNormal += mesh.getFaces()[fIdx].normal;
        }
        if (avgNormal.length() < 1e-4f) continue;
        avgNormal = avgNormal.normalized();

        for (uint32_t fIdx : connectedFaces) {
            if (fIdx >= mesh.getFaces().size() || mesh.getFaces()[fIdx].deleted) continue;
            const MeshFace sourceFace = mesh.getFaces()[fIdx];
            const auto it = std::find(sourceFace.vertices.begin(), sourceFace.vertices.end(), vIdx);
            if (it == sourceFace.vertices.end()) continue;
            const size_t corner = static_cast<size_t>(it - sourceFace.vertices.begin());
            const size_t prev = (corner + sourceFace.vertices.size() - 1) % sourceFace.vertices.size();
            const size_t next = (corner + 1) % sourceFace.vertices.size();
            const uint32_t prevV = sourceFace.vertices[prev];
            const uint32_t nextV = sourceFace.vertices[next];
            if (prevV >= vertices.size() || nextV >= vertices.size()) continue;

            const float prevLen = (vertices[prevV].position - centerPos).length();
            const float nextLen = (vertices[nextV].position - centerPos).length();
            const float localWidth = std::min(width, 0.45f * std::min(prevLen, nextLen));
            if (localWidth <= 1e-5f) continue;

            const auto prevDir = (vertices[prevV].position - centerPos).normalized();
            const auto nextDir = (vertices[nextV].position - centerPos).normalized();
            const float cornerU = vertices[vIdx].u;
            const float cornerV = vertices[vIdx].v;
            const auto faceNormal = sourceFace.normal;
            std::vector<uint32_t> arc;
            arc.reserve(static_cast<size_t>(segments) + 1);
            for (int s = 0; s <= segments; ++s) {
                const float t = static_cast<float>(s) / static_cast<float>(segments);
                // Spherical interpolation is unnecessary for this local
                // planar corner; normalized lerp gives a stable circular arc
                // even when the two incident edges are not perpendicular.
                auto dir = (prevDir * (1.0f - t) + nextDir * t).normalized();
                arc.push_back(mesh.addVertex(centerPos + dir * localWidth,
                                             cornerU, cornerV, faceNormal));
            }
            const uint32_t prevCut = arc.front();
            const uint32_t nextCut = arc.back();

            // Replace the corner by the two cut points, preserving the face loop.
            auto& face = mesh.getFaces()[fIdx];
            face.vertices.erase(face.vertices.begin() + static_cast<std::ptrdiff_t>(corner));
            face.vertices.insert(face.vertices.begin() + static_cast<std::ptrdiff_t>(corner),
                                 arc.begin(), arc.end());
            newVertices.insert(newVertices.end(), arc.begin(), arc.end());
            cuts.push_back({sourceFace, prevV, nextV, prevCut, nextCut, std::move(arc)});
        }

        // Each original edge incident to the bevelled vertex borders two
        // corner cuts. A triangle through the original neighbor closes the
        // gap between those two cut points.
        std::set<uint32_t> incidentNeighbors;
        for (const auto& cut : cuts) {
            incidentNeighbors.insert(cut.prevVertex);
            incidentNeighbors.insert(cut.nextVertex);
        }
        for (uint32_t neighbor : incidentNeighbors) {
            std::vector<uint32_t> edgeCuts;
            for (const auto& cut : cuts) {
                if (cut.prevVertex == neighbor) edgeCuts.push_back(cut.prevCut);
                if (cut.nextVertex == neighbor) edgeCuts.push_back(cut.nextCut);
            }
            if (edgeCuts.size() == 2 && edgeCuts[0] != edgeCuts[1]) {
                const auto cut0Pos = mesh.getVertices()[edgeCuts[0]].position;
                auto cut1Pos = mesh.getVertices()[edgeCuts[1]].position;
                const auto neighborPos = mesh.getVertices()[neighbor].position;
                const auto sideNormal = cuts.front().sourceFace.normal.length() > 1e-4f
                    ? cuts.front().sourceFace.normal
                    : Engine::Math::Vector3(0.0f, 1.0f, 0.0f);
                if ((neighborPos - cut0Pos).cross(cut1Pos - cut0Pos).length() > 1e-6f) {
                    addFaceWithAttributes(mesh, {edgeCuts[0], neighbor, edgeCuts[1]}, cuts.front().sourceFace);
                } else {
                    const float fanOffset = std::max(0.001f, (cut0Pos - neighborPos).length() * 0.05f);
                    if ((cut1Pos - cut0Pos).length() <= 1e-6f) {
                        auto separation = sideNormal - (neighborPos - cut0Pos).normalized() *
                            sideNormal.dot((neighborPos - cut0Pos).normalized());
                        if (separation.length() <= 1e-4f) {
                            separation = (neighborPos - cut0Pos).normalized().cross(
                                std::abs((neighborPos - cut0Pos).normalized().x) < 0.9f
                                    ? Engine::Math::Vector3(1.0f, 0.0f, 0.0f)
                                    : Engine::Math::Vector3(0.0f, 1.0f, 0.0f));
                        }
                        mesh.getVertices()[edgeCuts[1]].position += separation.normalized() * fanOffset;
                        cut1Pos = mesh.getVertices()[edgeCuts[1]].position;
                    }
                    if ((neighborPos - cut0Pos).cross(cut1Pos - cut0Pos).length() > 1e-6f) {
                        addFaceWithAttributes(mesh, {edgeCuts[0], neighbor, edgeCuts[1]}, cuts.front().sourceFace);
                        continue;
                    }

                    const auto edgeDirection = (cut1Pos - cut0Pos).normalized();
                    auto fanDirection = sideNormal - edgeDirection * sideNormal.dot(edgeDirection);
                    if (fanDirection.length() <= 1e-4f) {
                        const auto fallbackAxis = (std::abs(edgeDirection.x) < 0.9f)
                            ? Engine::Math::Vector3(1.0f, 0.0f, 0.0f)
                            : Engine::Math::Vector3(0.0f, 1.0f, 0.0f);
                        fanDirection = edgeDirection.cross(fallbackAxis).normalized();
                    } else {
                        fanDirection = fanDirection.normalized();
                    }
                    const uint32_t fanVertex = mesh.addVertex(
                        (cut0Pos + cut1Pos + neighborPos) * (1.0f / 3.0f) + fanDirection * fanOffset);
                    newVertices.push_back(fanVertex);
                    addFaceWithAttributes(mesh, {edgeCuts[0], neighbor, fanVertex}, cuts.front().sourceFace);
                    addFaceWithAttributes(mesh, {neighbor, edgeCuts[1], fanVertex}, cuts.front().sourceFace);
                    addFaceWithAttributes(mesh, {edgeCuts[1], edgeCuts[0], fanVertex}, cuts.front().sourceFace);
                }
            }
        }

        // Follow the actual boundary graph to build the cap. Sorting points
        // by angle is unreliable for multi-segment arcs and can leave the
        // intermediate arc vertices disconnected.
        std::map<uint32_t, std::vector<uint32_t>> capAdjacency;
        const auto addCapEdge = [&](uint32_t a, uint32_t b) {
            if (a == b) return;
            capAdjacency[a].push_back(b);
            capAdjacency[b].push_back(a);
        };
        for (const auto& cut : cuts) {
            for (size_t i = 1; i < cut.arc.size(); ++i) {
                addCapEdge(cut.arc[i - 1], cut.arc[i]);
            }
        }
        for (uint32_t neighbor : incidentNeighbors) {
            std::vector<uint32_t> edgeCuts;
            for (const auto& cut : cuts) {
                if (cut.prevVertex == neighbor) edgeCuts.push_back(cut.prevCut);
                if (cut.nextVertex == neighbor) edgeCuts.push_back(cut.nextCut);
            }
            if (edgeCuts.size() == 2) addCapEdge(edgeCuts[0], edgeCuts[1]);
        }

        std::vector<uint32_t> capVerts;
        if (!cuts.empty()) {
            const uint32_t start = cuts.front().arc.front();
            uint32_t previous = 0xFFFFFFFFu;
            uint32_t current = start;
            do {
                capVerts.push_back(current);
                const auto neighborsIt = capAdjacency.find(current);
                if (neighborsIt == capAdjacency.end() || neighborsIt->second.empty()) break;
                uint32_t next = neighborsIt->second.front();
                if (next == previous && neighborsIt->second.size() > 1) next = neighborsIt->second[1];
                previous = current;
                current = next;
            } while (current != start && capVerts.size() <= capAdjacency.size() + 1);

            if (current == start && capVerts.size() >= 3) {
                addFaceWithAttributes(mesh, capVerts, cuts.front().sourceFace);
            }
        }

        vertices[vIdx].deleted = true;
        mesh.rebuildTopology();
    }

    std::vector<uint32_t> vertRemap;
    mesh.packAndCompact(&vertRemap);
    for (auto& v : newVertices) {
        v = (v < vertRemap.size()) ? vertRemap[v] : 0xFFFFFFFFu;
    }
    newVertices.erase(
        std::remove(newVertices.begin(), newVertices.end(), 0xFFFFFFFFu),
        newVertices.end());
    mesh.recalculateAllNormals(false);
    return newVertices;
}

// ── 4. Subdivide ────────────────────────────────────────────────────────────
std::vector<uint32_t> MeshOperators::subdivideFaces(
    EditableMesh& mesh,
    const std::vector<uint32_t>& faceIndices,
    int cuts,
    float smoothness
) {
    if (faceIndices.empty()) return {};
    cuts = std::max(1, std::min(4, cuts));

    std::vector<uint32_t> currentFaces = faceIndices;

    for (int c = 0; c < cuts; ++c) {
        auto& faces = mesh.getFaces();
        std::vector<uint32_t> nextFaces;

        size_t estimatedFaces = 0;
        size_t estimatedVertices = 0;
        for (uint32_t fIdx : currentFaces) {
            if (fIdx < faces.size() && !faces[fIdx].deleted && faces[fIdx].vertices.size() >= 3) {
                estimatedFaces += faces[fIdx].vertices.size();
                estimatedVertices += faces[fIdx].vertices.size() + 1;
            }
        }
        faces.reserve(faces.size() + estimatedFaces);
        mesh.getVertices().reserve(mesh.getVertices().size() + estimatedVertices);

        // Shared edge midpoint map to avoid duplicate split vertices and holes
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> sharedMidpoints;
        auto getOrAddMidpoint = [&](uint32_t va, uint32_t vb) -> uint32_t {
            uint32_t mn = std::min(va, vb);
            uint32_t mx = std::max(va, vb);
            auto key = std::make_pair(mn, mx);
            auto it = sharedMidpoints.find(key);
            if (it != sharedMidpoints.end()) return it->second;

            Engine::Math::Vector3 midPos = (mesh.getVertices()[va].position + mesh.getVertices()[vb].position) * 0.5f;
            float midU = (mesh.getVertices()[va].u + mesh.getVertices()[vb].u) * 0.5f;
            float midV = (mesh.getVertices()[va].v + mesh.getVertices()[vb].v) * 0.5f;
            uint32_t midIdx = mesh.addVertex(midPos, midU, midV);
            sharedMidpoints[key] = midIdx;
            return midIdx;
        };

        for (uint32_t fIdx : currentFaces) {
            if (fIdx >= faces.size() || faces[fIdx].deleted) continue;
            auto face = faces[fIdx]; // copy
            size_t count = face.vertices.size();
            if (count < 3) continue;

            Engine::Math::Vector3 centerPos(0, 0, 0);
            for (uint32_t v : face.vertices) centerPos += mesh.getVertices()[v].position;
            centerPos = centerPos * (1.0f / (float)count);

            uint32_t centerVertIdx = mesh.addVertex(centerPos, 0.5f, 0.5f);

            std::vector<uint32_t> edgeMidpoints(count);
            for (size_t i = 0; i < count; ++i) {
                uint32_t v0 = face.vertices[i];
                uint32_t v1 = face.vertices[(i + 1) % count];
                edgeMidpoints[i] = getOrAddMidpoint(v0, v1);
            }

            const auto cornerUV = [&](size_t corner) {
                if (face.uvs.size() == count) return face.uvs[corner];
                const auto& vertex = mesh.getVertices()[face.vertices[corner]];
                return std::pair<float, float>{vertex.u, vertex.v};
            };
            const auto midpointUV = [&](size_t edge) {
                const auto a = cornerUV(edge);
                const auto b = cornerUV((edge + 1) % count);
                return std::pair<float, float>{(a.first + b.first) * 0.5f, (a.second + b.second) * 0.5f};
            };
            std::pair<float, float> centerUV{0.0f, 0.0f};
            for (size_t i = 0; i < count; ++i) {
                const auto uv = cornerUV(i);
                centerUV.first += uv.first;
                centerUV.second += uv.second;
            }
            centerUV.first /= static_cast<float>(count);
            centerUV.second /= static_cast<float>(count);

            mesh.removeFace(fIdx);
            for (size_t i = 0; i < count; ++i) {
                size_t prev = (i + count - 1) % count;
                uint32_t corner = face.vertices[i];
                uint32_t midCur = edgeMidpoints[i];
                uint32_t midPrev = edgeMidpoints[prev];

                int newFIdx = mesh.addFaceWithUVs(
                    {corner, midCur, centerVertIdx, midPrev},
                    {cornerUV(i), midpointUV(i), centerUV, midpointUV(prev)},
                    face.materialId);
                if (newFIdx >= 0) {
                    nextFaces.push_back(static_cast<uint32_t>(newFIdx));
                }
            }
        }

        currentFaces = nextFaces;
        mesh.rebuildTopology();
    }

    std::vector<uint32_t> faceRemap;
    mesh.packAndCompact(nullptr, &faceRemap);
    for (auto& fIdx : currentFaces) {
        fIdx = (fIdx < faceRemap.size()) ? faceRemap[fIdx] : 0xFFFFFFFFu;
    }
    currentFaces.erase(
        std::remove(currentFaces.begin(), currentFaces.end(), 0xFFFFFFFFu),
        currentFaces.end());
    mesh.recalculateAllNormals(smoothness > 0.0f);
    return currentFaces;
}

// ── 5. Merge / Weld ─────────────────────────────────────────────────────────
void MeshOperators::mergeVertices(
    EditableMesh& mesh,
    const std::vector<uint32_t>& vertIndices,
    MergeMode mode,
    const Engine::Math::Vector3& targetPos
) {
    auto& vertices = mesh.getVertices();
    std::vector<uint32_t> validVertices;
    std::set<uint32_t> uniqueVertices;
    for (uint32_t v : vertIndices) {
        if (v < vertices.size() && !vertices[v].deleted && uniqueVertices.insert(v).second) {
            validVertices.push_back(v);
        }
    }
    if (validVertices.size() < 2) return;

    Engine::Math::Vector3 finalPos(0, 0, 0);

    if (mode == MergeMode::Distance) {
        float threshold = (targetPos.length() > 1e-6f) ? targetPos.x : 0.001f;
        weldVerticesByDistance(mesh, threshold);
        return;
    }

    if (mode == MergeMode::Center || mode == MergeMode::Collapse) {
        for (uint32_t v : validVertices) {
            finalPos += vertices[v].position;
        }
        finalPos = finalPos * (1.0f / static_cast<float>(validVertices.size()));
    } else if (mode == MergeMode::First) {
        finalPos = vertices[validVertices.front()].position;
    } else if (mode == MergeMode::Last) {
        finalPos = vertices[validVertices.back()].position;
    } else if (mode == MergeMode::Cursor) {
        finalPos = targetPos;
    }

    uint32_t survivingVert = validVertices[0];
    vertices[survivingVert].position = finalPos;

    // Remap all faces referencing merged vertices to the surviving one
    std::set<uint32_t> mergeSet(validVertices.begin() + 1, validVertices.end());
    auto& faces = mesh.getFaces();

    for (size_t f = 0; f < faces.size(); ++f) {
        if (faces[f].deleted) continue;
        for (auto& v : faces[f].vertices) {
            if (mergeSet.count(v)) {
                v = survivingVert;
            }
        }

        // Remove degenerate consecutive duplicate vertices
        auto& fVerts = faces[f].vertices;
        fVerts.erase(std::unique(fVerts.begin(), fVerts.end()), fVerts.end());
        if (fVerts.size() > 1 && fVerts.front() == fVerts.back()) fVerts.pop_back();

        if (fVerts.size() < 3) {
            faces[f].deleted = true;
        }
    }

    for (uint32_t v : mergeSet) {
        mesh.removeVertex(v);
    }

    mesh.packAndCompact();
}

void MeshOperators::weldVerticesByDistance(EditableMesh& mesh, float distanceThreshold) {
    auto& vertices = mesh.getVertices();
    if (vertices.empty()) return;
    distanceThreshold = std::max(0.0f, distanceThreshold);
    float threshSq = distanceThreshold * distanceThreshold;
    if (threshSq <= 0.0f) return;

    // Disjoint-Set (Union-Find) structure
    std::vector<uint32_t> parent(vertices.size());
    for (size_t i = 0; i < parent.size(); ++i) parent[i] = static_cast<uint32_t>(i);

    auto findRoot = [&](uint32_t i, auto& self) -> uint32_t {
        if (parent[i] == i) return i;
        return parent[i] = self(parent[i], self);
    };

    float cellSize = std::max(distanceThreshold, 0.0001f);
    float invCellSize = 1.0f / cellSize;

    struct CellCoord {
        int64_t x, y, z;
        bool operator==(const CellCoord& o) const { return x == o.x && y == o.y && z == o.z; }
    };
    struct CellHash {
        std::size_t operator()(const CellCoord& c) const {
            std::size_t h1 = std::hash<int64_t>{}(c.x);
            std::size_t h2 = std::hash<int64_t>{}(c.y);
            std::size_t h3 = std::hash<int64_t>{}(c.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    std::unordered_map<CellCoord, std::vector<uint32_t>, CellHash> spatialGrid;
    spatialGrid.reserve(vertices.size());

    for (size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].deleted) continue;
        int64_t cx = static_cast<int64_t>(std::floor(vertices[i].position.x * invCellSize));
        int64_t cy = static_cast<int64_t>(std::floor(vertices[i].position.y * invCellSize));
        int64_t cz = static_cast<int64_t>(std::floor(vertices[i].position.z * invCellSize));
        spatialGrid[{cx, cy, cz}].push_back(static_cast<uint32_t>(i));
    }

    for (size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].deleted) continue;
        int64_t cx = static_cast<int64_t>(std::floor(vertices[i].position.x * invCellSize));
        int64_t cy = static_cast<int64_t>(std::floor(vertices[i].position.y * invCellSize));
        int64_t cz = static_cast<int64_t>(std::floor(vertices[i].position.z * invCellSize));

        for (int64_t dx = -1; dx <= 1; ++dx) {
            for (int64_t dy = -1; dy <= 1; ++dy) {
                for (int64_t dz = -1; dz <= 1; ++dz) {
                    auto it = spatialGrid.find({cx + dx, cy + dy, cz + dz});
                    if (it == spatialGrid.end()) continue;

                    for (uint32_t j : it->second) {
                        if (j <= i || vertices[j].deleted) continue;
                        auto delta = vertices[i].position - vertices[j].position;
                        float distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
                        if (distSq <= threshSq) {
                            uint32_t rootI = findRoot(static_cast<uint32_t>(i), findRoot);
                            uint32_t rootJ = findRoot(j, findRoot);
                            if (rootI != rootJ) {
                                parent[rootJ] = rootI;
                            }
                        }
                    }
                }
            }
        }
    }

    // Remap faces
    auto& faces = mesh.getFaces();
    for (size_t f = 0; f < faces.size(); ++f) {
        if (faces[f].deleted) continue;
        for (auto& v : faces[f].vertices) {
            v = findRoot(v, findRoot);
        }
        // Remove duplicate consecutive vertices in face
        auto& fVerts = faces[f].vertices;
        fVerts.erase(std::unique(fVerts.begin(), fVerts.end()), fVerts.end());
        if (fVerts.size() > 1 && fVerts.front() == fVerts.back()) fVerts.pop_back();
        if (fVerts.size() < 3) faces[f].deleted = true;
    }

    for (size_t i = 0; i < vertices.size(); ++i) {
        if (parent[i] != static_cast<uint32_t>(i)) {
            mesh.removeVertex(static_cast<uint32_t>(i));
        }
    }

    mesh.packAndCompact();
    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
}

// ── 6. Delete & Dissolve ────────────────────────────────────────────────────
void MeshOperators::deleteElements(
    EditableMesh& mesh,
    const std::vector<uint32_t>& indices,
    SubElementType type
) {
    for (uint32_t idx : indices) {
        if (type == SubElementType::Vertex) mesh.removeVertex(idx);
        else if (type == SubElementType::Edge) mesh.removeEdge(idx);
        else if (type == SubElementType::Face) mesh.removeFace(idx);
    }
    mesh.packAndCompact();
}

void MeshOperators::dissolveEdges(
    EditableMesh& mesh,
    const std::vector<uint32_t>& edgeIndices
) {
    auto& edges = mesh.getEdges();
    auto& faces = mesh.getFaces();

    // Merge the two face cycles around each selected manifold edge. The
    // previous implementation concatenated vertex sets, which destroyed
    // cyclic ordering and could create bow-tie/self-intersecting n-gons.
    auto walkWithoutEdge = [](const std::vector<uint32_t>& verts,
                              uint32_t start, uint32_t end) {
        std::vector<uint32_t> path;
        if (verts.empty()) return path;
        size_t startPos = verts.size();
        for (size_t i = 0; i < verts.size(); ++i) {
            if (verts[i] == start) { startPos = i; break; }
        }
        if (startPos == verts.size()) return path;

        size_t pos = startPos;
        for (size_t n = 0; n < verts.size(); ++n) {
            const uint32_t current = verts[pos];
            if (current == end) break;
            path.push_back(current);
            pos = (pos + 1) % verts.size();
        }
        return path;
    };

    for (uint32_t eIdx : edgeIndices) {
        if (eIdx >= edges.size() || edges[eIdx].deleted) continue;
        auto connectedFaces = mesh.getEdgeFaces(eIdx);

        if (connectedFaces.size() == 2) {
            uint32_t f0 = connectedFaces[0];
            uint32_t f1 = connectedFaces[1];
            uint32_t v0 = edges[eIdx].v0;
            uint32_t v1 = edges[eIdx].v1;

            if (f0 >= faces.size() || f1 >= faces.size() ||
                faces[f0].deleted || faces[f1].deleted) continue;

            const auto& f0v = faces[f0].vertices;
            const auto& f1v = faces[f1].vertices;
            bool f0Forward = false;
            bool f1Forward = false;
            for (size_t i = 0; i < f0v.size(); ++i) {
                if (f0v[i] == v0 && f0v[(i + 1) % f0v.size()] == v1) f0Forward = true;
                if (f0v[i] == v1 && f0v[(i + 1) % f0v.size()] == v0) f0Forward = false;
            }
            for (size_t i = 0; i < f1v.size(); ++i) {
                if (f1v[i] == v0 && f1v[(i + 1) % f1v.size()] == v1) f1Forward = true;
                if (f1v[i] == v1 && f1v[(i + 1) % f1v.size()] == v0) f1Forward = false;
            }

            uint32_t a = f0Forward ? v0 : v1;
            uint32_t b = f0Forward ? v1 : v0;
            // Walk f0 from b to a and f1 from a to b, omitting the shared edge.
            auto firstPath = walkWithoutEdge(f0v, b, a);
            auto secondPath = walkWithoutEdge(f1v, a, b);
            std::vector<uint32_t> combinedVerts = firstPath;
            combinedVerts.insert(combinedVerts.end(), secondPath.begin(), secondPath.end());

            std::vector<uint32_t> compactVerts;
            for (uint32_t v : combinedVerts) {
                if (compactVerts.empty() || compactVerts.back() != v) compactVerts.push_back(v);
            }
            if (compactVerts.size() > 1 && compactVerts.front() == compactVerts.back()) {
                compactVerts.pop_back();
            }

            if (compactVerts.size() >= 3) {
                faces[f0].vertices = std::move(compactVerts);
                faces[f1].deleted = true;
                edges[eIdx].deleted = true;
            }
        }
    }

    mesh.packAndCompact();
    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
}

void MeshOperators::dissolveVertices(
    EditableMesh& mesh,
    const std::vector<uint32_t>& vertIndices
) {
    auto& vertices = mesh.getVertices();
    auto& faces = mesh.getFaces();

    for (uint32_t vIdx : vertIndices) {
        if (vIdx >= vertices.size() || vertices[vIdx].deleted) continue;
        auto connectedEdges = mesh.getConnectedEdges(vIdx);
        if (connectedEdges.size() == 2) {
            // A valence-two dissolve removes the vertex from each incident
            // face cycle. RebuildTopology will create the replacement edge;
            // deleting the vertex first would incorrectly delete all faces.
            for (auto& face : faces) {
                if (face.deleted) continue;
                auto it = std::find(face.vertices.begin(), face.vertices.end(), vIdx);
                if (it == face.vertices.end()) continue;
                face.vertices.erase(it);
                if (face.vertices.size() < 3) face.deleted = true;
            }
            vertices[vIdx].deleted = true;
        }
    }
    mesh.packAndCompact();
}

// ── 7. Fill & Bridge ────────────────────────────────────────────────────────
int MeshOperators::fillFace(
    EditableMesh& mesh,
    const std::vector<uint32_t>& vertIndices
) {
    if (vertIndices.size() < 3) return -1;
    int f = mesh.addFace(vertIndices);
    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
    return f;
}

void MeshOperators::bridgeEdgeLoops(
    EditableMesh& mesh,
    const std::vector<uint32_t>& loop1Verts,
    const std::vector<uint32_t>& loop2Verts
) {
    if (loop1Verts.empty() || loop1Verts.size() != loop2Verts.size()) return;
    size_t count = loop1Verts.size();

    for (size_t i = 0; i < count; ++i) {
        size_t next = (i + 1) % count;
        uint32_t a0 = loop1Verts[i];
        uint32_t a1 = loop1Verts[next];
        uint32_t b1 = loop2Verts[next];
        uint32_t b0 = loop2Verts[i];

        mesh.addFace({a0, a1, b1, b0});
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
}

// ── 8. Advanced Topology & Utility Operators ────────────────────────────────
void MeshOperators::pokeFaces(
    EditableMesh& mesh,
    const std::vector<uint32_t>& faceIndices,
    float offset
) {
    auto& faces = mesh.getFaces();
    auto& vertices = mesh.getVertices();

    for (uint32_t fIdx : faceIndices) {
        if (fIdx >= faces.size() || faces[fIdx].deleted) continue;
        auto face = faces[fIdx];
        size_t count = face.vertices.size();
        if (count < 3) continue;

        mesh.calculateFaceNormal(fIdx);
        Engine::Math::Vector3 center = face.center + face.normal * offset;
        uint32_t centerV = mesh.addVertex(center, 0.5f, 0.5f, face.normal);

        mesh.removeFace(fIdx);
        for (size_t i = 0; i < count; ++i) {
            uint32_t v0 = face.vertices[i];
            uint32_t v1 = face.vertices[(i + 1) % count];
            mesh.addFace({v0, v1, centerV});
        }
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
}

void MeshOperators::triangulateFaces(
    EditableMesh& mesh,
    const std::vector<uint32_t>& faceIndices
) {
    auto& faces = mesh.getFaces();
    const auto& vertices = mesh.getVertices();

    for (uint32_t fIdx : faceIndices) {
        if (fIdx >= faces.size() || faces[fIdx].deleted) continue;
        auto face = faces[fIdx];
        size_t count = face.vertices.size();
        if (count <= 3) continue;

        mesh.removeFace(fIdx);

        if (count == 4) {
            mesh.addFace({face.vertices[0], face.vertices[1], face.vertices[2]});
            mesh.addFace({face.vertices[0], face.vertices[2], face.vertices[3]});
        } else {
            // Robust Ear Clipping in 2D
            Engine::Math::Vector3 normal = face.normal;
            Engine::Math::Vector3 axisX = (std::abs(normal.x) > 0.9f) ? Engine::Math::Vector3(0, 1, 0) : Engine::Math::Vector3(1, 0, 0);
            Engine::Math::Vector3 uAxis = normal.cross(axisX).normalized();
            Engine::Math::Vector3 vAxis = normal.cross(uAxis).normalized();

            std::vector<std::pair<float, float>> poly2D(count);
            std::vector<uint32_t> polyIndices(count);
            for (size_t i = 0; i < count; ++i) {
                const auto& p = vertices[face.vertices[i]].position;
                poly2D[i] = { p.dot(uAxis), p.dot(vAxis) };
                polyIndices[i] = face.vertices[i];
            }

            // Normalize winding to CCW
            float signedArea = 0.0f;
            for (size_t i = 0; i < count; ++i) {
                size_t next = (i + 1) % count;
                signedArea += poly2D[i].first * poly2D[next].second - poly2D[next].first * poly2D[i].second;
            }
            if (signedArea < 0.0f) {
                std::reverse(poly2D.begin(), poly2D.end());
                std::reverse(polyIndices.begin(), polyIndices.end());
            }

            auto isConvex = [&](size_t prev, size_t curr, size_t next) -> bool {
                float x1 = poly2D[curr].first - poly2D[prev].first;
                float y1 = poly2D[curr].second - poly2D[prev].second;
                float x2 = poly2D[next].first - poly2D[curr].first;
                float y2 = poly2D[next].second - poly2D[curr].second;
                return (x1 * y2 - y1 * x2) >= 0.0f;
            };

            auto pointInTri = [](float px, float py, float ax, float ay, float bx, float by, float cx, float cy) -> bool {
                float d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
                float d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
                float d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
                bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
                bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
                return !(has_neg && has_pos);
            };

            int maxIters = static_cast<int>(count * 3);
            while (polyIndices.size() > 2 && maxIters-- > 0) {
                size_t n = polyIndices.size();
                bool earFound = false;

                for (size_t i = 0; i < n; ++i) {
                    size_t prev = (i + n - 1) % n;
                    size_t curr = i;
                    size_t next = (i + 1) % n;

                    if (isConvex(prev, curr, next)) {
                        bool inside = false;
                        for (size_t j = 0; j < n; ++j) {
                            if (j == prev || j == curr || j == next) continue;
                            if (pointInTri(poly2D[j].first, poly2D[j].second,
                                           poly2D[prev].first, poly2D[prev].second,
                                           poly2D[curr].first, poly2D[curr].second,
                                           poly2D[next].first, poly2D[next].second)) {
                                inside = true;
                                break;
                            }
                        }

                        if (!inside) {
                            mesh.addFace({polyIndices[prev], polyIndices[curr], polyIndices[next]});
                            polyIndices.erase(polyIndices.begin() + i);
                            poly2D.erase(poly2D.begin() + i);
                            earFound = true;
                            break;
                        }
                    }
                }

                if (!earFound) {
                    for (size_t i = 1; i + 1 < polyIndices.size(); ++i) {
                        mesh.addFace({polyIndices[0], polyIndices[i], polyIndices[i + 1]});
                    }
                    break;
                }
            }
        }
    }

    mesh.packAndCompact();
    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
}

void MeshOperators::trisToQuads(
    EditableMesh& mesh,
    const std::vector<uint32_t>& faceIndices
) {
    auto& faces = mesh.getFaces();
    auto& edges = mesh.getEdges();

    std::set<uint32_t> triSet;
    for (uint32_t f : faceIndices) {
        if (f < faces.size() && !faces[f].deleted && faces[f].vertices.size() == 3) {
            triSet.insert(f);
        }
    }

    std::set<uint32_t> processedFaces;

    for (uint32_t fA : triSet) {
        if (processedFaces.count(fA) || faces[fA].deleted) continue;

        // Find a neighboring triangle that shares an edge
        for (size_t e = 0; e < edges.size(); ++e) {
            if (edges[e].deleted) continue;
            auto edgeFaces = mesh.getEdgeFaces((uint32_t)e);
            if (edgeFaces.size() == 2) {
                uint32_t f0 = edgeFaces[0], f1 = edgeFaces[1];
                if ((f0 == fA && triSet.count(f1) && !processedFaces.count(f1)) ||
                    (f1 == fA && triSet.count(f0) && !processedFaces.count(f0))) {
                    uint32_t fB = (f0 == fA) ? f1 : f0;

                    uint32_t ev0 = edges[e].v0, ev1 = edges[e].v1;
                    const auto& vA = faces[fA].vertices;
                    const auto& vB = faces[fB].vertices;

                    uint32_t oppA = 0xFFFFFFFF, oppB = 0xFFFFFFFF;
                    for (uint32_t v : vA) if (v != ev0 && v != ev1) oppA = v;
                    for (uint32_t v : vB) if (v != ev0 && v != ev1) oppB = v;

                    if (oppA != 0xFFFFFFFF && oppB != 0xFFFFFFFF) {
                        mesh.removeFace(fA);
                        mesh.removeFace(fB);
                        mesh.addFace({oppA, ev0, oppB, ev1});
                        processedFaces.insert(fA);
                        processedFaces.insert(fB);
                        break;
                    }
                }
            }
        }
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
}

void MeshOperators::slideVertices(
    EditableMesh& mesh,
    const std::vector<uint32_t>& vertIndices,
    float factor
) {
    auto& vertices = mesh.getVertices();

    for (uint32_t vIdx : vertIndices) {
        if (vIdx >= vertices.size() || vertices[vIdx].deleted) continue;
        auto adj = mesh.getAdjacentVertices(vIdx);
        if (adj.empty()) continue;

        // Pick dominant connected edge direction to slide along
        uint32_t targetV = (factor >= 0.0f) ? adj.front() : adj.back();
        float absF = std::clamp(std::abs(factor), 0.0f, 0.95f);
        vertices[vIdx].position += (vertices[targetV].position - vertices[vIdx].position) * absF;
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
}

void MeshOperators::edgeSplit(
    EditableMesh& mesh,
    const std::vector<uint32_t>& edgeIndices
) {
    auto& edges = mesh.getEdges();
    auto& faces = mesh.getFaces();
    auto& vertices = mesh.getVertices();

    for (uint32_t eIdx : edgeIndices) {
        if (eIdx >= edges.size() || edges[eIdx].deleted) continue;
        auto edgeFaces = mesh.getEdgeFaces(eIdx);
        if (edgeFaces.size() < 2) continue; // Boundary already

        uint32_t f0 = edgeFaces[0];
        uint32_t f1 = edgeFaces[1];
        uint32_t v0 = edges[eIdx].v0;
        uint32_t v1 = edges[eIdx].v1;

        // Duplicate v0 and v1 for face f1
        uint32_t newV0 = mesh.addVertex(vertices[v0].position, vertices[v0].u, vertices[v0].v, vertices[v0].normal);
        uint32_t newV1 = mesh.addVertex(vertices[v1].position, vertices[v1].u, vertices[v1].v, vertices[v1].normal);

        for (auto& v : faces[f1].vertices) {
            if (v == v0) v = newV0;
            else if (v == v1) v = newV1;
        }
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
}

void MeshOperators::flipNormals(
    EditableMesh& mesh,
    const std::vector<uint32_t>& faceIndices
) {
    auto& faces = mesh.getFaces();
    for (uint32_t fIdx : faceIndices) {
        if (fIdx >= faces.size() || faces[fIdx].deleted) continue;
        std::reverse(faces[fIdx].vertices.begin(), faces[fIdx].vertices.end());
        if (!faces[fIdx].uvs.empty()) {
            std::reverse(faces[fIdx].uvs.begin(), faces[fIdx].uvs.end());
        }
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
}

// ── 9. Hard-Surface & Greyboxing Operators ──────────────────────────────────
void MeshOperators::solidify(
    EditableMesh& mesh,
    float thickness,
    bool rimFill
) {
    auto& vertices = mesh.getVertices();
    auto& faces = mesh.getFaces();
    if (vertices.empty() || faces.empty()) return;

    // Ensure all face normals are up to date
    for (size_t f = 0; f < faces.size(); ++f) {
        if (!faces[f].deleted) mesh.calculateFaceNormal(static_cast<uint32_t>(f));
    }

    size_t origVertCount = vertices.size();
    size_t origFaceCount = faces.size();

    // Compute robust per-vertex normal from adjacent faces
    std::vector<Engine::Math::Vector3> vertNormals(origVertCount, Engine::Math::Vector3(0, 0, 0));
    for (size_t f = 0; f < origFaceCount; ++f) {
        if (faces[f].deleted) continue;
        for (uint32_t v : faces[f].vertices) {
            if (v < origVertCount) {
                vertNormals[v] += faces[f].normal;
            }
        }
    }
    for (size_t i = 0; i < origVertCount; ++i) {
        if (vertNormals[i].length() > 1e-4f) {
            vertNormals[i] = vertNormals[i].normalized();
        } else {
            vertNormals[i] = vertices[i].normal;
        }
    }

    // 1. Create inner extruded shell vertices
    std::vector<uint32_t> innerVertMap(origVertCount, 0xFFFFFFFF);
    for (size_t i = 0; i < origVertCount; ++i) {
        if (vertices[i].deleted) continue;
        const auto& src = vertices[i];
        Engine::Math::Vector3 inPos = src.position - vertNormals[i] * thickness;
        innerVertMap[i] = mesh.addVertex(inPos, src.u, src.v, vertNormals[i] * -1.0f);
    }

    // 2. Count directed boundary edges for rim quad walls
    std::map<std::pair<uint32_t, uint32_t>, int> edgeCount;
    for (size_t f = 0; f < origFaceCount; ++f) {
        if (faces[f].deleted) continue;
        const auto& fv = faces[f].vertices;
        size_t count = fv.size();
        for (size_t i = 0; i < count; ++i) {
            uint32_t v0 = fv[i];
            uint32_t v1 = fv[(i + 1) % count];
            edgeCount[{v0, v1}]++;
        }
    }

    // 3. Add flipped inner faces
    for (size_t f = 0; f < origFaceCount; ++f) {
        if (faces[f].deleted) continue;
        const auto& fv = faces[f].vertices;
        size_t count = fv.size();
        std::vector<uint32_t> innerF(count);
        for (size_t i = 0; i < count; ++i) {
            innerF[i] = innerVertMap[fv[count - 1 - i]]; // Flipped CCW order
        }
        mesh.addFace(innerF);
    }

    // 4. Fill boundary rims connecting outer and inner shells
    if (rimFill) {
        for (const auto& [edge, count] : edgeCount) {
            if (count == 1 && edgeCount.find({edge.second, edge.first}) == edgeCount.end()) {
                uint32_t b0 = edge.first;
                uint32_t b1 = edge.second;
                uint32_t in0 = innerVertMap[b0];
                uint32_t in1 = innerVertMap[b1];
                if (in0 != 0xFFFFFFFF && in1 != 0xFFFFFFFF) {
                    mesh.addFace({b0, in0, in1, b1});
                }
            }
        }
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
}

void MeshOperators::bisectPlane(
    EditableMesh& mesh,
    const Engine::Math::Vector3& planePoint,
    const Engine::Math::Vector3& planeNormal,
    bool clearInner,
    bool clearOuter,
    bool fillCut
) {
    auto& faces = mesh.getFaces();
    auto& vertices = mesh.getVertices();
    Engine::Math::Vector3 pn = (planeNormal.length() > 1e-4f) ? planeNormal.normalized() : Engine::Math::Vector3(0, 1, 0);

    size_t origFaceCount = faces.size();

    std::map<std::pair<uint32_t, uint32_t>, uint32_t> edgeSplitMap;
    auto getOrCreateSplitVert = [&](uint32_t vA, uint32_t vB, float dA, float dB) -> uint32_t {
        auto key = std::minmax(vA, vB);
        auto it = edgeSplitMap.find(key);
        if (it != edgeSplitMap.end()) return it->second;

        float t = std::clamp(-dA / (dB - dA), 0.0f, 1.0f);
        const auto& vertA = vertices[vA];
        const auto& vertB = vertices[vB];
        Engine::Math::Vector3 p = vertA.position + (vertB.position - vertA.position) * t;
        float u = vertA.u + (vertB.u - vertA.u) * t;
        float v = vertA.v + (vertB.v - vertA.v) * t;
        Engine::Math::Vector3 norm = (vertA.normal + (vertB.normal - vertA.normal) * t);
        if (norm.length() > 1e-4f) norm = norm.normalized();
        else norm = pn;

        uint32_t nv = mesh.addVertex(p, u, v, norm);
        edgeSplitMap[key] = nv;
        return nv;
    };

    std::vector<std::pair<uint32_t, uint32_t>> capEdges;

    for (size_t f = 0; f < origFaceCount; ++f) {
        if (faces[f].deleted) continue;
        const auto& fVerts = faces[f].vertices;
        size_t n = fVerts.size();
        if (n < 3) continue;

        std::vector<float> dists(n);
        int posCount = 0;
        int negCount = 0;

        for (size_t i = 0; i < n; ++i) {
            dists[i] = (vertices[fVerts[i]].position - planePoint).dot(pn);
            if (dists[i] > 1e-5f) posCount++;
            else if (dists[i] < -1e-5f) negCount++;
        }

        // Case 1: Entirely on positive side (outer)
        if (negCount == 0) {
            if (clearOuter) {
                mesh.removeFace(static_cast<uint32_t>(f));
            }
            continue;
        }

        // Case 2: Entirely on negative side (inner)
        if (posCount == 0) {
            if (clearInner) {
                mesh.removeFace(static_cast<uint32_t>(f));
            }
            continue;
        }

        // Case 3: Face straddles the plane -> split polygon
        std::vector<uint32_t> posPoly;
        std::vector<uint32_t> negPoly;
        std::vector<uint32_t> splitVertsInFace;

        for (size_t i = 0; i < n; ++i) {
            uint32_t curV = fVerts[i];
            uint32_t nextV = fVerts[(i + 1) % n];
            float curD = dists[i];
            float nextD = dists[(i + 1) % n];

            if (curD >= -1e-5f) posPoly.push_back(curV);
            if (curD <= 1e-5f) negPoly.push_back(curV);

            if ((curD > 1e-5f && nextD < -1e-5f) || (curD < -1e-5f && nextD > 1e-5f)) {
                uint32_t splitV = getOrCreateSplitVert(curV, nextV, curD, nextD);
                posPoly.push_back(splitV);
                negPoly.push_back(splitV);
                splitVertsInFace.push_back(splitV);
            }
        }

        if (splitVertsInFace.size() == 2) {
            capEdges.push_back({splitVertsInFace[0], splitVertsInFace[1]});
        }

        // Remove original face and add split halves
        mesh.removeFace(static_cast<uint32_t>(f));

        auto sanitizePoly = [&](const std::vector<uint32_t>& poly) -> std::vector<uint32_t> {
            std::vector<uint32_t> clean;
            std::set<uint32_t> seen;
            for (uint32_t v : poly) {
                if (v < vertices.size() && !vertices[v].deleted && seen.insert(v).second) {
                    clean.push_back(v);
                }
            }
            return clean;
        };

        auto hasNonZeroArea = [&](const std::vector<uint32_t>& poly) -> bool {
            if (poly.size() < 3) return false;
            Engine::Math::Vector3 areaNormal(0.0f, 0.0f, 0.0f);
            size_t vCount = poly.size();
            for (size_t i = 0; i < vCount; ++i) {
                const auto& a = vertices[poly[i]].position;
                const auto& b = vertices[poly[(i + 1) % vCount]].position;
                areaNormal += a.cross(b);
            }
            return areaNormal.length() > 1e-5f;
        };

        std::vector<uint32_t> cleanPos = sanitizePoly(posPoly);
        std::vector<uint32_t> cleanNeg = sanitizePoly(negPoly);

        if (!clearOuter && cleanPos.size() >= 3 && hasNonZeroArea(cleanPos)) {
            mesh.addFace(cleanPos);
        }
        if (!clearInner && cleanNeg.size() >= 3 && hasNonZeroArea(cleanNeg)) {
            mesh.addFace(cleanNeg);
        }
    }

    // Fill cut cap if requested and we cut an open cross section (clearInner or clearOuter)
    if (fillCut && (clearInner || clearOuter) && capEdges.size() >= 3) {
        std::vector<uint32_t> capPoly;
        std::set<uint32_t> visitedCapEdges;
        std::set<uint32_t> seenInCap;
        
        uint32_t cur = capEdges[0].first;
        uint32_t next = capEdges[0].second;
        capPoly.push_back(cur);
        seenInCap.insert(cur);
        visitedCapEdges.insert(0);

        while (capPoly.size() < capEdges.size() + 2) {
            if (seenInCap.insert(next).second) {
                capPoly.push_back(next);
            }
            bool foundNext = false;
            for (size_t i = 0; i < capEdges.size(); ++i) {
                if (visitedCapEdges.count((uint32_t)i)) continue;
                if (capEdges[i].first == next) {
                    next = capEdges[i].second;
                    visitedCapEdges.insert((uint32_t)i);
                    foundNext = true;
                    break;
                } else if (capEdges[i].second == next) {
                    next = capEdges[i].first;
                    visitedCapEdges.insert((uint32_t)i);
                    foundNext = true;
                    break;
                }
            }
            if (!foundNext || next == capPoly[0]) break;
        }

        if (capPoly.size() >= 3) {
            Engine::Math::Vector3 capNorm(0.0f, 0.0f, 0.0f);
            for (size_t i = 0; i < capPoly.size(); ++i) {
                const auto& a = vertices[capPoly[i]].position;
                const auto& b = vertices[capPoly[(i + 1) % capPoly.size()]].position;
                capNorm += a.cross(b);
            }
            if (clearOuter && capNorm.dot(pn) > 0.0f) {
                std::reverse(capPoly.begin(), capPoly.end());
            } else if (clearInner && capNorm.dot(pn) < 0.0f) {
                std::reverse(capPoly.begin(), capPoly.end());
            }
            mesh.addFace(capPoly);
        }
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
}

} // namespace Engine::Geometry
