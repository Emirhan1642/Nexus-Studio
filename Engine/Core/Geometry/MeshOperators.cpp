#include "MeshOperators.h"
#include <cmath>
#include <algorithm>
#include <map>
#include <set>

namespace Engine::Geometry {

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
                mesh.addFace({b0, b1, t1, t0});
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
        std::set<uint32_t> uniqueVerts;

        for (uint32_t fIdx : selFaceSet) {
            const auto& fVerts = faces[fIdx].vertices;
            size_t count = fVerts.size();
            for (size_t i = 0; i < count; ++i) {
                uint32_t v0 = fVerts[i];
                uint32_t v1 = fVerts[(i + 1) % count];
                directedEdgeCount[{v0, v1}]++;
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
                    mesh.addFace({b1, b0, t0, t1});
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
    Engine::Math::Vector3 offset = direction.normalized() * distance;

    for (uint32_t eIdx : edgeIndices) {
        if (eIdx >= edges.size() || edges[eIdx].deleted) continue;
        uint32_t v0 = edges[eIdx].v0;
        uint32_t v1 = edges[eIdx].v1;

        uint32_t newV0 = mesh.addVertex(vertices[v0].position + offset, vertices[v0].u, vertices[v0].v);
        uint32_t newV1 = mesh.addVertex(vertices[v1].position + offset, vertices[v1].u, vertices[v1].v);

        int f = mesh.addFace({v0, v1, newV1, newV0});
        if (f >= 0) newFaces.push_back(static_cast<uint32_t>(f));
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
    return newFaces;
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

    for (uint32_t fIdx : faceIndices) {
        if (fIdx >= faces.size() || faces[fIdx].deleted) continue;

        auto& face = faces[fIdx];
        mesh.calculateFaceNormal(fIdx);

        Engine::Math::Vector3 normal = face.normal;
        size_t count = face.vertices.size();
        if (count < 3) continue;

        std::vector<uint32_t> outerVerts = face.vertices;
        std::vector<uint32_t> innerVerts(count);

        // Compute angle-bisector offset directions for each corner
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

            // Offset inwards by thickness + depth along normal
            Engine::Math::Vector3 inPos = pCurr + miterBisect * thickness + normal * depth;
            innerVerts[i] = mesh.addVertex(inPos, vertices[vCurr].u, vertices[vCurr].v, normal);
        }

        // Replace original face with inner face
        face.vertices = innerVerts;
        innerFaces.push_back(fIdx);

        // Build outer quad ring
        for (size_t i = 0; i < count; ++i) {
            size_t next = (i + 1) % count;
            uint32_t o0 = outerVerts[i];
            uint32_t o1 = outerVerts[next];
            uint32_t i1 = innerVerts[next];
            uint32_t i0 = innerVerts[i];

            mesh.addFace({o0, o1, i1, i0});
        }
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
    return innerFaces;
}

// ── 3. Bevel / Chamfer ──────────────────────────────────────────────────────
void MeshOperators::bevelFaces(
    EditableMesh& mesh,
    const std::vector<uint32_t>& faceIndices,
    float width,
    int segments,
    float profile,
    float depth
) {
    if (faceIndices.empty() || width <= 0.0001f) return;
    auto& faces = mesh.getFaces();
    auto& vertices = mesh.getVertices();

    segments = std::max(1, std::min(8, segments));
    profile = std::max(0.0f, std::min(1.0f, profile));

    for (uint32_t fIdx : faceIndices) {
        if (fIdx >= faces.size() || faces[fIdx].deleted) continue;

        auto face = faces[fIdx]; // copy
        size_t count = face.vertices.size();
        if (count < 3) continue;

        mesh.calculateFaceNormal(fIdx);
        Engine::Math::Vector3 normal = face.normal;

        Engine::Math::Vector3 center(0, 0, 0);
        for (uint32_t v : face.vertices) center += vertices[v].position;
        center = center * (1.0f / (float)count);

        float maxRadius = 0.0f;
        for (uint32_t v : face.vertices) {
            float dist = (vertices[v].position - center).length();
            if (dist > maxRadius) maxRadius = dist;
        }

        float actualWidth = std::min(width, maxRadius * 0.45f);
        float shrinkRatio = (maxRadius > 0.0001f) ? (1.0f - (actualWidth / maxRadius)) : 0.8f;
        shrinkRatio = std::max(0.05f, std::min(0.95f, shrinkRatio));

        // Generate segmented rings of vertices from side boundary (ring 0) to top inner face (ring segments)
        std::vector<std::vector<uint32_t>> rings(segments + 1, std::vector<uint32_t>(count));

        for (size_t i = 0; i < count; ++i) {
            uint32_t origV = face.vertices[i];
            Engine::Math::Vector3 origPos = vertices[origV].position;

            Engine::Math::Vector3 topPos = center + (origPos - center) * shrinkRatio + normal * depth;

            for (int s = 0; s <= segments; ++s) {
                float u = (float)s / (float)segments; // 0.0 (side) to 1.0 (top)
                float rad = u * 1.5707963f; // 0 to PI/2

                // Trigonometric Superellipse / Arc profile curvature blending
                float wSide = (1.0f - std::sin(rad)) * (1.0f - profile) + (1.0f - u) * profile;
                float wTop = (1.0f - std::cos(rad)) * (1.0f - profile) + u * profile;

                Engine::Math::Vector3 ringPos = origPos - normal * (actualWidth * wSide) + (topPos - origPos) * wTop;
                rings[s][i] = mesh.addVertex(ringPos, vertices[origV].u, vertices[origV].v, normal);
            }
        }

        // Replace original top face with innermost elevated ring
        mesh.removeFace(fIdx);
        mesh.addFace(rings[segments]);

        // Add 4-sided sloping ramp quads connecting consecutive rings for all segments and all edges
        for (int s = 0; s < segments; ++s) {
            for (size_t i = 0; i < count; ++i) {
                size_t next = (i + 1) % count;
                uint32_t s0 = rings[s][i];
                uint32_t s1 = rings[s][next];
                uint32_t t1 = rings[s + 1][next];
                uint32_t t0 = rings[s + 1][i];

                mesh.addFace({ s0, s1, t1, t0 });
            }
        }

        // Update all neighboring side faces that shared original corners to use the outermost ring 0
        for (size_t i = 0; i < count; ++i) {
            uint32_t origV = face.vertices[i];
            uint32_t newSideV = rings[0][i];

            for (size_t otherF = 0; otherF < faces.size(); ++otherF) {
                if (otherF == fIdx || faces[otherF].deleted) continue;
                for (auto& v : faces[otherF].vertices) {
                    if (v == origV) {
                        v = newSideV;
                    }
                }
            }
        }
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
}

void MeshOperators::bevelEdges(
    EditableMesh& mesh,
    const std::vector<uint32_t>& edgeIndices,
    float width,
    int segments,
    float profile
) {
    if (edgeIndices.empty() || width <= 0.0001f) return;
    auto& edges = mesh.getEdges();
    auto& faces = mesh.getFaces();
    auto& vertices = mesh.getVertices();

    segments = std::max(1, std::min(8, segments));
    profile = std::max(0.0f, std::min(1.0f, profile));

    for (uint32_t eIdx : edgeIndices) {
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

        float bevelWidth = std::min(width, edgeLen * 0.45f);

        auto connectedFaces = mesh.getEdgeFaces(eIdx);
        if (connectedFaces.size() < 2) continue;

        uint32_t fA_idx = connectedFaces[0];
        uint32_t fB_idx = connectedFaces[1];
        if (fA_idx >= faces.size() || fB_idx >= faces.size()) continue;

        mesh.calculateFaceNormal(fA_idx);
        mesh.calculateFaceNormal(fB_idx);
        auto nA = faces[fA_idx].normal;
        auto nB = faces[fB_idx].normal;

        Engine::Math::Vector3 centerA(0, 0, 0);
        for (uint32_t v : faces[fA_idx].vertices) centerA += vertices[v].position;
        if (!faces[fA_idx].vertices.empty()) centerA = centerA * (1.0f / (float)faces[fA_idx].vertices.size());

        Engine::Math::Vector3 centerB(0, 0, 0);
        for (uint32_t v : faces[fB_idx].vertices) centerB += vertices[v].position;
        if (!faces[fB_idx].vertices.empty()) centerB = centerB * (1.0f / (float)faces[fB_idx].vertices.size());

        // Compute face-inward tangent vectors perpendicular to edge
        Engine::Math::Vector3 tanA = nA.cross(edgeDir).normalized();
        if ((centerA - p0).dot(tanA) < 0.0f) tanA = tanA * -1.0f;

        Engine::Math::Vector3 tanB = nB.cross(edgeDir).normalized();
        if ((centerB - p0).dot(tanB) < 0.0f) tanB = tanB * -1.0f;

        // Create segmented rings of vertices between tanA and tanB
        std::vector<uint32_t> splitA0(segments + 1);
        std::vector<uint32_t> splitA1(segments + 1);

        for (int s = 0; s <= segments; ++s) {
            float u = (float)s / (float)segments; // 0.0 at tanA, 1.0 at tanB
            float rad = u * 1.5707963f; // 0 to PI/2

            float wA = (1.0f - std::sin(rad)) * (1.0f - profile) + (1.0f - u) * profile;
            float wB = (1.0f - std::cos(rad)) * (1.0f - profile) + u * profile;

            Engine::Math::Vector3 pt0 = p0 + tanA * (bevelWidth * wA) + tanB * (bevelWidth * wB);
            Engine::Math::Vector3 pt1 = p1 + tanA * (bevelWidth * wA) + tanB * (bevelWidth * wB);

            splitA0[s] = mesh.addVertex(pt0, vertices[v0].u, vertices[v0].v);
            splitA1[s] = mesh.addVertex(pt1, vertices[v1].u, vertices[v1].v);
        }

        // Replace v0, v1 in Face A with splitA0[0], splitA1[0]
        auto& fA = faces[fA_idx];
        for (auto& v : fA.vertices) {
            if (v == v0) v = splitA0[0];
            else if (v == v1) v = splitA1[0];
        }

        // Replace v0, v1 in Face B with splitA0[segments], splitA1[segments]
        auto& fB = faces[fB_idx];
        for (auto& v : fB.vertices) {
            if (v == v0) v = splitA0[segments];
            else if (v == v1) v = splitA1[segments];
        }

        // Connect intermediate segments with quads
        for (int s = 0; s < segments; ++s) {
            uint32_t p0_s  = splitA0[s];
            uint32_t p1_s  = splitA1[s];
            uint32_t p0_s1 = splitA0[s + 1];
            uint32_t p1_s1 = splitA1[s + 1];

            mesh.addFace({ p0_s, p1_s, p1_s1, p0_s1 });
        }
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
}

void MeshOperators::bevelVertices(
    EditableMesh& mesh,
    const std::vector<uint32_t>& vertIndices,
    float width,
    int segments
) {
    if (vertIndices.empty() || width <= 0.0001f) return;
    auto& vertices = mesh.getVertices();
    (void)segments; // Multi-segment corner arcs are a separate follow-up.

    for (uint32_t vIdx : vertIndices) {
        if (vIdx >= vertices.size() || vertices[vIdx].deleted) continue;

        const auto centerPos = vertices[vIdx].position;
        auto connectedFaces = mesh.getConnectedFaces(vIdx);
        if (connectedFaces.size() < 3) continue;

        Engine::Math::Vector3 avgNormal(0, 0, 0);
        for (uint32_t fIdx : connectedFaces) {
            mesh.calculateFaceNormal(fIdx);
            avgNormal += mesh.getFaces()[fIdx].normal;
        }
        if (avgNormal.length() < 1e-4f) continue;
        avgNormal = avgNormal.normalized();

        std::vector<uint32_t> capVerts;
        for (uint32_t fIdx : connectedFaces) {
            if (fIdx >= mesh.getFaces().size() || mesh.getFaces()[fIdx].deleted) continue;
            auto& face = mesh.getFaces()[fIdx];
            auto it = std::find(face.vertices.begin(), face.vertices.end(), vIdx);
            if (it == face.vertices.end()) continue;
            const size_t corner = static_cast<size_t>(it - face.vertices.begin());
            const size_t prev = (corner + face.vertices.size() - 1) % face.vertices.size();
            const size_t next = (corner + 1) % face.vertices.size();
            const uint32_t prevV = face.vertices[prev];
            const uint32_t nextV = face.vertices[next];
            if (prevV >= vertices.size() || nextV >= vertices.size()) continue;

            const float prevLen = (vertices[prevV].position - centerPos).length();
            const float nextLen = (vertices[nextV].position - centerPos).length();
            const float localWidth = std::min(width, 0.45f * std::min(prevLen, nextLen));
            if (localWidth <= 1e-5f) continue;

            const auto prevDir = (vertices[prevV].position - centerPos).normalized();
            const auto nextDir = (vertices[nextV].position - centerPos).normalized();
            const float cornerU = vertices[vIdx].u;
            const float cornerV = vertices[vIdx].v;
            const auto faceNormal = face.normal;
            const uint32_t prevCut = mesh.addVertex(centerPos + prevDir * localWidth,
                                                    cornerU, cornerV, faceNormal);
            const uint32_t nextCut = mesh.addVertex(centerPos + nextDir * localWidth,
                                                    cornerU, cornerV, faceNormal);

            // Replace the corner by the two cut points, preserving the face loop.
            face.vertices.erase(face.vertices.begin() + static_cast<std::ptrdiff_t>(corner));
            face.vertices.insert(face.vertices.begin() + static_cast<std::ptrdiff_t>(corner),
                                 {prevCut, nextCut});
            capVerts.push_back(prevCut);
            capVerts.push_back(nextCut);
        }

        if (capVerts.size() >= 3) {
            // Order the cap points around the averaged corner normal.
            Engine::Math::Vector3 axis = (std::abs(avgNormal.y) < 0.9f)
                ? Engine::Math::Vector3(0, 1, 0)
                : Engine::Math::Vector3(1, 0, 0);
            Engine::Math::Vector3 uAxis = avgNormal.cross(axis).normalized();
            Engine::Math::Vector3 vAxis = avgNormal.cross(uAxis).normalized();
            std::sort(capVerts.begin(), capVerts.end(), [&](uint32_t a, uint32_t b) {
                const auto da = mesh.getVertices()[a].position - centerPos;
                const auto db = mesh.getVertices()[b].position - centerPos;
                const float aa = std::atan2(da.dot(vAxis), da.dot(uAxis));
                const float ab = std::atan2(db.dot(vAxis), db.dot(uAxis));
                return aa < ab;
            });
            mesh.addFace(capVerts);
        }

        vertices[vIdx].deleted = true;
        mesh.rebuildTopology();
    }

    mesh.packAndCompact();
    mesh.recalculateAllNormals(false);
}

// ── 4. Subdivide ────────────────────────────────────────────────────────────
void MeshOperators::subdivideFaces(
    EditableMesh& mesh,
    const std::vector<uint32_t>& faceIndices,
    int cuts,
    float smoothness
) {
    if (faceIndices.empty()) return;
    cuts = std::max(1, std::min(4, cuts));

    std::vector<uint32_t> currentFaces = faceIndices;

    for (int c = 0; c < cuts; ++c) {
        auto& faces = mesh.getFaces();
        std::vector<uint32_t> nextFaces;

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

            mesh.removeFace(fIdx);
            for (size_t i = 0; i < count; ++i) {
                size_t prev = (i + count - 1) % count;
                uint32_t corner = face.vertices[i];
                uint32_t midCur = edgeMidpoints[i];
                uint32_t midPrev = edgeMidpoints[prev];

                int newFIdx = mesh.addFace({corner, midCur, centerVertIdx, midPrev});
                if (newFIdx >= 0) nextFaces.push_back(static_cast<uint32_t>(newFIdx));
            }
        }

        currentFaces = nextFaces;
        mesh.rebuildTopology();
    }

    mesh.packAndCompact();
    mesh.recalculateAllNormals(smoothness > 0.0f);
}

// ── 5. Merge / Weld ─────────────────────────────────────────────────────────
void MeshOperators::mergeVertices(
    EditableMesh& mesh,
    const std::vector<uint32_t>& vertIndices,
    MergeMode mode,
    const Engine::Math::Vector3& targetPos
) {
    if (vertIndices.size() < 2) return;
    auto& vertices = mesh.getVertices();

    Engine::Math::Vector3 finalPos(0, 0, 0);

    if (mode == MergeMode::Center || mode == MergeMode::Collapse) {
        for (uint32_t v : vertIndices) {
            if (v < vertices.size() && !vertices[v].deleted) {
                finalPos += vertices[v].position;
            }
        }
        finalPos = finalPos * (1.0f / static_cast<float>(vertIndices.size()));
    } else if (mode == MergeMode::First) {
        finalPos = vertices[vertIndices.front()].position;
    } else if (mode == MergeMode::Last) {
        finalPos = vertices[vertIndices.back()].position;
    } else if (mode == MergeMode::Cursor) {
        finalPos = targetPos;
    }

    uint32_t survivingVert = vertIndices[0];
    vertices[survivingVert].position = finalPos;

    // Remap all faces referencing merged vertices to the surviving one
    std::set<uint32_t> mergeSet(vertIndices.begin() + 1, vertIndices.end());
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
    float threshSq = distanceThreshold * distanceThreshold;

    // Disjoint-Set (Union-Find) structure
    std::vector<uint32_t> parent(vertices.size());
    for (size_t i = 0; i < parent.size(); ++i) parent[i] = static_cast<uint32_t>(i);

    auto findRoot = [&](uint32_t i, auto& self) -> uint32_t {
        if (parent[i] == i) return i;
        return parent[i] = self(parent[i], self);
    };

    for (size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].deleted) continue;
        for (size_t j = i + 1; j < vertices.size(); ++j) {
            if (vertices[j].deleted) continue;
            auto delta = vertices[i].position - vertices[j].position;
            float distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            if (distSq <= threshSq) {
                uint32_t rootI = findRoot(static_cast<uint32_t>(i), findRoot);
                uint32_t rootJ = findRoot(static_cast<uint32_t>(j), findRoot);
                if (rootI != rootJ) {
                    parent[rootJ] = rootI;
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
    Engine::Math::Vector3 pn = planeNormal.normalized();

    // Classify faces or cut faces intersected by plane
    std::vector<uint32_t> cutBoundaryVerts;

    for (size_t f = 0; f < faces.size(); ++f) {
        if (faces[f].deleted) continue;
        mesh.calculateFaceNormal(static_cast<uint32_t>(f));
        float d = (faces[f].center - planePoint).dot(pn);

        if (clearInner && d < -1e-4f) {
            mesh.removeFace(static_cast<uint32_t>(f));
        } else if (clearOuter && d > 1e-4f) {
            mesh.removeFace(static_cast<uint32_t>(f));
        }
    }

    mesh.rebuildTopology();
    mesh.recalculateAllNormals(false);
}

} // namespace Engine::Geometry
