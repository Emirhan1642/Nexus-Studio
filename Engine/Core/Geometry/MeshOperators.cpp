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

    for (uint32_t fIdx : faceIndices) {
        if (fIdx >= faces.size() || faces[fIdx].deleted) continue;

        auto& face = faces[fIdx];
        mesh.calculateFaceNormal(fIdx);

        Engine::Math::Vector3 dir = (customDirection.length() > 0.0001f)
            ? customDirection.normalized()
            : face.normal;

        Engine::Math::Vector3 offset = dir * distance;

        // Create duplicate vertices for the extruded face
        size_t vertCount = face.vertices.size();
        std::vector<uint32_t> originalVerts = face.vertices;
        std::vector<uint32_t> extrudedVerts(vertCount);

        for (size_t i = 0; i < vertCount; ++i) {
            uint32_t oldV = originalVerts[i];
            const auto& srcV = vertices[oldV];
            extrudedVerts[i] = mesh.addVertex(srcV.position + offset, srcV.u, srcV.v, srcV.normal);
        }

        // Update original face to use the extruded vertices
        face.vertices = extrudedVerts;
        newFaces.push_back(fIdx);

        // Create side Quad walls connecting original base to extruded top
        for (size_t i = 0; i < vertCount; ++i) {
            size_t next = (i + 1) % vertCount;
            uint32_t b0 = originalVerts[i];
            uint32_t b1 = originalVerts[next];
            uint32_t t1 = extrudedVerts[next];
            uint32_t t0 = extrudedVerts[i];

            mesh.addFace({b0, b1, t1, t0});
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

        Engine::Math::Vector3 center = face.center;
        Engine::Math::Vector3 normal = face.normal;
        size_t count = face.vertices.size();
        std::vector<uint32_t> outerVerts = face.vertices;
        std::vector<uint32_t> innerVerts(count);

        float scaleFactor = std::max(0.01f, 1.0f - thickness);

        for (size_t i = 0; i < count; ++i) {
            uint32_t origV = outerVerts[i];
            const auto& origPos = vertices[origV].position;
            // Lerp towards center
            Engine::Math::Vector3 inPos = center + (origPos - center) * scaleFactor + normal * depth;
            innerVerts[i] = mesh.addVertex(inPos, vertices[origV].u, vertices[origV].v, normal);
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
// ── 3. Bevel / Chamfer ──────────────────────────────────────────────────────
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

        float actualWidth = std::min(width, maxRadius * 0.85f);
        float shrinkRatio = (maxRadius > 0.0001f) ? (1.0f - (actualWidth / maxRadius)) : 0.8f;
        shrinkRatio = std::max(0.05f, std::min(0.95f, shrinkRatio));

        // Create rings of vertices from outer boundary to inner top face
        // ring 0 = lowered boundary ring, ring segments = inner elevated top ring
        std::vector<std::vector<uint32_t>> rings(segments + 1, std::vector<uint32_t>(count));

        for (size_t i = 0; i < count; ++i) {
            uint32_t origV = face.vertices[i];
            Engine::Math::Vector3 origPos = vertices[origV].position;
            Engine::Math::Vector3 topPos = center + (origPos - center) * shrinkRatio + normal * depth;
            Engine::Math::Vector3 basePos = origPos - normal * actualWidth;

            // Generate intermediate profile curved positions
            for (int s = 0; s <= segments; ++s) {
                float u = (float)s / (float)segments; // 0.0 (base) to 1.0 (top)

                // Superellipse / Arc profile curvature blending
                float rad = u * 1.5707963f; // 0 to PI/2
                float curveH = std::sin(rad) * (1.0f - profile) + u * profile;
                float curveW = (1.0f - std::cos(rad)) * (1.0f - profile) + u * profile;

                Engine::Math::Vector3 ringPos = basePos + (origPos - basePos) * (1.0f - curveW) + (topPos - origPos) * curveW + normal * (actualWidth * curveH);
                rings[s][i] = mesh.addVertex(ringPos, vertices[origV].u, vertices[origV].v, normal);
            }
        }

        // Replace original face with top inner face
        mesh.removeFace(fIdx);
        mesh.addFace(rings[segments]);

        // Connect segments with quads
        for (int s = 0; s < segments; ++s) {
            for (size_t i = 0; i < count; ++i) {
                size_t next = (i + 1) % count;
                uint32_t v0 = rings[s][i];
                uint32_t v1 = rings[s][next];
                uint32_t iv1 = rings[s + 1][next];
                uint32_t iv0 = rings[s + 1][i];

                mesh.addFace({ v0, v1, iv1, iv0 });
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

        // Create split vertex pairs for face A and face B
        uint32_t v0_A = mesh.addVertex(p0 + tanA * bevelWidth, vertices[v0].u, vertices[v0].v);
        uint32_t v1_A = mesh.addVertex(p1 + tanA * bevelWidth, vertices[v1].u, vertices[v1].v);

        uint32_t v0_B = mesh.addVertex(p0 + tanB * bevelWidth, vertices[v0].u, vertices[v0].v);
        uint32_t v1_B = mesh.addVertex(p1 + tanB * bevelWidth, vertices[v1].u, vertices[v1].v);

        // Replace v0, v1 in Face A with v0_A, v1_A
        auto& fA = faces[fA_idx];
        for (auto& v : fA.vertices) {
            if (v == v0) v = v0_A;
            else if (v == v1) v = v1_A;
        }

        // Replace v0, v1 in Face B with v0_B, v1_B
        auto& fB = faces[fB_idx];
        for (auto& v : fB.vertices) {
            if (v == v0) v = v0_B;
            else if (v == v1) v = v1_B;
        }

        // Add connecting chamfer quad strip with guaranteed CCW outwards normal
        mesh.addFace({ v0_A, v1_A, v1_B, v0_B });
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

    for (uint32_t vIdx : vertIndices) {
        if (vIdx >= vertices.size() || vertices[vIdx].deleted) continue;

        auto adjVerts = mesh.getAdjacentVertices(vIdx);
        if (adjVerts.size() < 3) continue;

        const auto centerPos = vertices[vIdx].position;
        std::vector<uint32_t> capVerts;

        for (uint32_t adjV : adjVerts) {
            if (adjV >= vertices.size()) continue;
            Engine::Math::Vector3 dir = (vertices[adjV].position - centerPos).normalized();
            uint32_t newV = mesh.addVertex(centerPos + dir * width, vertices[vIdx].u, vertices[vIdx].v);
            capVerts.push_back(newV);
        }

        if (capVerts.size() >= 3) {
            mesh.addFace(capVerts);
        }
        mesh.removeVertex(vIdx);
    }

    mesh.packAndCompact();
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

    for (int c = 0; c < cuts; ++c) {
        auto& faces = mesh.getFaces();
        auto& vertices = mesh.getVertices();

        std::vector<uint32_t> currentFaces;
        if (c == 0) {
            currentFaces = faceIndices;
        } else {
            for (size_t i = 0; i < faces.size(); ++i) {
                if (!faces[i].deleted) currentFaces.push_back(static_cast<uint32_t>(i));
            }
        }

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

                mesh.addFace({corner, midCur, centerVertIdx, midPrev});
            }
        }

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
    float threshSq = distanceThreshold * distanceThreshold;

    for (size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].deleted) continue;
        std::vector<uint32_t> toMerge;
        toMerge.push_back(static_cast<uint32_t>(i));

        for (size_t j = i + 1; j < vertices.size(); ++j) {
            if (vertices[j].deleted) continue;
            auto delta = vertices[i].position - vertices[j].position;
            float distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            if (distSq <= threshSq) {
                toMerge.push_back(static_cast<uint32_t>(j));
            }
        }

        if (toMerge.size() > 1) {
            mergeVertices(mesh, toMerge, MergeMode::First);
        }
    }
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

    for (uint32_t eIdx : edgeIndices) {
        if (eIdx >= edges.size() || edges[eIdx].deleted) continue;
        auto connectedFaces = mesh.getEdgeFaces(eIdx);

        if (connectedFaces.size() == 2) {
            uint32_t f0 = connectedFaces[0];
            uint32_t f1 = connectedFaces[1];
            uint32_t v0 = edges[eIdx].v0;
            uint32_t v1 = edges[eIdx].v1;

            // Merge f0 and f1 into a single n-gon face
            std::vector<uint32_t> combinedVerts;
            const auto& f0v = faces[f0].vertices;
            const auto& f1v = faces[f1].vertices;

            for (uint32_t v : f0v) {
                if (v != v0 && v != v1) combinedVerts.push_back(v);
                else combinedVerts.push_back(v);
            }
            for (uint32_t v : f1v) {
                if (v != v0 && v != v1 && std::find(combinedVerts.begin(), combinedVerts.end(), v) == combinedVerts.end()) {
                    combinedVerts.push_back(v);
                }
            }

            if (combinedVerts.size() >= 3) {
                faces[f0].vertices = combinedVerts;
                faces[f1].deleted = true;
                edges[eIdx].deleted = true;
            }
        }
    }

    mesh.packAndCompact();
}

void MeshOperators::dissolveVertices(
    EditableMesh& mesh,
    const std::vector<uint32_t>& vertIndices
) {
    for (uint32_t vIdx : vertIndices) {
        auto connectedEdges = mesh.getConnectedEdges(vIdx);
        if (connectedEdges.size() == 2) {
            // Straight vertex between two edges: remove and connect end vertices
            auto adj = mesh.getAdjacentVertices(vIdx);
            if (adj.size() == 2) {
                mesh.addEdge(adj[0], adj[1]);
                mesh.removeVertex(vIdx);
            }
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

} // namespace Engine::Geometry
