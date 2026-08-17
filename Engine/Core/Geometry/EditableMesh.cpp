#include "EditableMesh.h"
#include <algorithm>
#include <cmath>

namespace Engine::Geometry {

uint32_t EditableMesh::addVertex(const Engine::Math::Vector3& pos, float u, float v, const Engine::Math::Vector3& normal) {
    MeshVertex vert;
    vert.position = pos;
    vert.u = u;
    vert.v = v;
    vert.normal = normal;
    m_vertices.push_back(vert);
    return static_cast<uint32_t>(m_vertices.size() - 1);
}

int EditableMesh::addEdge(uint32_t v0, uint32_t v1) {
    if (v0 == v1 || v0 >= m_vertices.size() || v1 >= m_vertices.size()) return -1;
    uint64_t key = makeEdgeKey(v0, v1);
    auto it = m_edgeMap.find(key);
    if (it != m_edgeMap.end()) return it->second;

    MeshEdge edge;
    edge.v0 = std::min(v0, v1);
    edge.v1 = std::max(v0, v1);
    m_edges.push_back(edge);
    int edgeIdx = static_cast<int>(m_edges.size() - 1);
    m_edgeMap[key] = edgeIdx;
    return edgeIdx;
}

int EditableMesh::addFace(const std::vector<uint32_t>& vertIndices) {
    if (vertIndices.size() < 3) return -1;

    // Reject duplicate consecutive vertices and check bounds
    std::vector<uint32_t> cleanVerts;
    cleanVerts.reserve(vertIndices.size());
    for (size_t i = 0; i < vertIndices.size(); ++i) {
        uint32_t v = vertIndices[i];
        if (v >= m_vertices.size() || m_vertices[v].deleted) return -1;
        if (cleanVerts.empty() || cleanVerts.back() != v) {
            cleanVerts.push_back(v);
        }
    }
    if (cleanVerts.size() > 1 && cleanVerts.front() == cleanVerts.back()) {
        cleanVerts.pop_back();
    }
    if (cleanVerts.size() < 3) return -1;

    MeshFace face;
    face.vertices = cleanVerts;

    for (size_t i = 0; i < cleanVerts.size(); ++i) {
        uint32_t v0 = cleanVerts[i];
        uint32_t v1 = cleanVerts[(i + 1) % cleanVerts.size()];
        int edgeIdx = addEdge(v0, v1);
        if (edgeIdx >= 0) face.edges.push_back(static_cast<uint32_t>(edgeIdx));
    }

    m_faces.push_back(face);
    int faceIdx = static_cast<int>(m_faces.size() - 1);
    calculateFaceNormal(faceIdx);
    return faceIdx;
}

int EditableMesh::addFaceWithUVs(
    const std::vector<uint32_t>& vertIndices,
    const std::vector<std::pair<float, float>>& cornerUVs,
    int matId
) {
    int fIdx = addFace(vertIndices);
    if (fIdx >= 0 && fIdx < static_cast<int>(m_faces.size())) {
        m_faces[fIdx].uvs = cornerUVs;
        m_faces[fIdx].materialId = matId;
    }
    return fIdx;
}

void EditableMesh::removeVertex(uint32_t index) {
    if (index >= m_vertices.size()) return;
    m_vertices[index].deleted = true;
    for (size_t f = 0; f < m_faces.size(); ++f) {
        if (m_faces[f].deleted) continue;
        for (uint32_t v : m_faces[f].vertices) {
            if (v == index) {
                removeFace(static_cast<uint32_t>(f));
                break;
            }
        }
    }
}

void EditableMesh::removeEdge(uint32_t index) {
    if (index >= m_edges.size()) return;
    m_edges[index].deleted = true;
    for (size_t f = 0; f < m_faces.size(); ++f) {
        if (m_faces[f].deleted) continue;
        for (uint32_t e : m_faces[f].edges) {
            if (e == index) {
                removeFace(static_cast<uint32_t>(f));
                break;
            }
        }
    }
}

void EditableMesh::removeFace(uint32_t index) {
    if (index >= m_faces.size()) return;
    m_faces[index].deleted = true;
}

void EditableMesh::clear() {
    m_vertices.clear();
    m_edges.clear();
    m_faces.clear();
    m_halfEdges.clear();
    m_edgeMap.clear();
}

bool EditableMesh::validate() const {
    // 1. Verify vertices
    for (size_t i = 0; i < m_vertices.size(); ++i) {
        if (m_vertices[i].deleted) continue;
        const auto& p = m_vertices[i].position;
        if (std::isnan(p.x) || std::isnan(p.y) || std::isnan(p.z) ||
            std::isinf(p.x) || std::isinf(p.y) || std::isinf(p.z)) {
            return false;
        }
    }

    // 2. Verify faces
    for (size_t f = 0; f < m_faces.size(); ++f) {
        if (m_faces[f].deleted) continue;
        const auto& face = m_faces[f];
        size_t vCount = face.vertices.size();
        if (vCount < 3) return false;

        std::unordered_set<uint32_t> seen;
        for (size_t i = 0; i < vCount; ++i) {
            uint32_t v = face.vertices[i];
            if (v >= m_vertices.size() || m_vertices[v].deleted) return false;
            if (seen.count(v)) return false; // Duplicate vertex in same face
            seen.insert(v);
        }

        if (!face.uvs.empty() && face.uvs.size() != vCount) return false;
        if (!face.normals.empty() && face.normals.size() != vCount) return false;
        for (const auto& uv : face.uvs) {
            if (!std::isfinite(uv.first) || !std::isfinite(uv.second)) return false;
        }
        for (const auto& n : face.normals) {
            if (!std::isfinite(n.x) || !std::isfinite(n.y) || !std::isfinite(n.z)) return false;
        }
        // A zero-area polygon cannot produce a valid render or physics face.
        Engine::Math::Vector3 areaNormal(0.0f, 0.0f, 0.0f);
        for (size_t i = 0; i < vCount; ++i) {
            const auto& a = m_vertices[face.vertices[i]].position;
            const auto& b = m_vertices[face.vertices[(i + 1) % vCount]].position;
            areaNormal += a.cross(b);
        }
        if (!std::isfinite(areaNormal.x) || !std::isfinite(areaNormal.y) ||
            !std::isfinite(areaNormal.z) || areaNormal.length() <= 1e-7f) return false;
    }

    // 3. Verify half-edges
    for (size_t i = 0; i < m_halfEdges.size(); ++i) {
        const auto& he = m_halfEdges[i];
        if (he.origin >= m_vertices.size() || m_vertices[he.origin].deleted) return false;
        if (he.face < 0 || he.face >= static_cast<int>(m_faces.size()) || m_faces[he.face].deleted) return false;

        if (he.edge < 0 || he.edge >= static_cast<int>(m_edges.size())) return false;
        if (he.next < 0 || he.next >= static_cast<int>(m_halfEdges.size())) return false;
        if (he.prev < 0 || he.prev >= static_cast<int>(m_halfEdges.size())) return false;
        if (m_halfEdges[he.next].prev != static_cast<int>(i)) return false;
        if (m_halfEdges[he.prev].next != static_cast<int>(i)) return false;
        if (m_edges[he.edge].halfEdge0 != static_cast<int>(i) &&
            m_edges[he.edge].halfEdge1 != static_cast<int>(i)) return false;

        if (he.twin != -1) {
            if (he.twin < 0 || he.twin >= static_cast<int>(m_halfEdges.size())) return false;
            if (m_halfEdges[he.twin].twin != static_cast<int>(i)) return false; // Twin symmetry broken
        }
    }

    // Verify every face's half-edge cycle and edge list agree with its vertex loop.
    for (size_t f = 0; f < m_faces.size(); ++f) {
        const auto& face = m_faces[f];
        if (face.deleted) continue;
        if (face.firstHalfEdge < 0 || face.firstHalfEdge >= static_cast<int>(m_halfEdges.size())) return false;
        if (face.edges.size() != face.vertices.size()) return false;

        int heIdx = face.firstHalfEdge;
        for (size_t i = 0; i < face.vertices.size(); ++i) {
            if (heIdx < 0 || heIdx >= static_cast<int>(m_halfEdges.size())) return false;
            const auto& he = m_halfEdges[heIdx];
            if (he.face != static_cast<int>(f) || he.origin != face.vertices[i]) return false;
            if (he.edge != static_cast<int>(face.edges[i])) return false;
            heIdx = he.next;
        }
        if (heIdx != face.firstHalfEdge) return false;
    }

    // An edge can be boundary (one half-edge) or manifold (two), never more.
    for (size_t e = 0; e < m_edges.size(); ++e) {
        const auto& edge = m_edges[e];
        if (edge.deleted) continue;
        if (edge.v0 >= m_vertices.size() || edge.v1 >= m_vertices.size() ||
            edge.v0 == edge.v1 || m_vertices[edge.v0].deleted || m_vertices[edge.v1].deleted) return false;
        int count = (edge.halfEdge0 != -1 ? 1 : 0) + (edge.halfEdge1 != -1 ? 1 : 0);
        if (count == 0 || count > 2) return false;
        if (edge.halfEdge0 != -1 && (edge.halfEdge0 >= static_cast<int>(m_halfEdges.size()) ||
            m_halfEdges[edge.halfEdge0].edge != static_cast<int>(e))) return false;
        if (edge.halfEdge1 != -1 && (edge.halfEdge1 >= static_cast<int>(m_halfEdges.size()) ||
            m_halfEdges[edge.halfEdge1].edge != static_cast<int>(e))) return false;
        if (edge.halfEdge0 != -1) {
            const auto& he = m_halfEdges[edge.halfEdge0];
            if (he.next < 0 || he.next >= static_cast<int>(m_halfEdges.size())) return false;
            const auto& end = m_halfEdges[he.next];
            if (!((he.origin == edge.v0 && end.origin == edge.v1) ||
                  (he.origin == edge.v1 && end.origin == edge.v0))) return false;
        }
    }

    return true;
}

void EditableMesh::calculateFaceNormal(uint32_t faceIndex) {
    if (faceIndex >= m_faces.size()) return;
    auto& face = m_faces[faceIndex];
    if (face.deleted || face.vertices.size() < 3) return;

    // Newell's method for arbitrary n-gons
    Engine::Math::Vector3 normal(0, 0, 0);
    Engine::Math::Vector3 center(0, 0, 0);
    size_t count = face.vertices.size();

    for (size_t i = 0; i < count; ++i) {
        uint32_t curIdx = face.vertices[i];
        uint32_t nextIdx = face.vertices[(i + 1) % count];
        if (curIdx >= m_vertices.size() || nextIdx >= m_vertices.size()) return;

        const auto& c = m_vertices[curIdx].position;
        const auto& n = m_vertices[nextIdx].position;

        normal.x += (c.y - n.y) * (c.z + n.z);
        normal.y += (c.z - n.z) * (c.x + n.x);
        normal.z += (c.x - n.x) * (c.y + n.y);
        center += c;
    }

    face.center = center * (1.0f / static_cast<float>(count));
    float len = normal.length();
    if (len > 1e-6f) {
        face.normal = normal * (1.0f / len);
    } else {
        face.normal = {0.0f, 1.0f, 0.0f};
    }
}

void EditableMesh::recalculateAllNormals(bool smooth, float autoSmoothAngleDeg) {
    for (size_t f = 0; f < m_faces.size(); ++f) {
        calculateFaceNormal(static_cast<uint32_t>(f));
    }

    // Build vertex-to-face adjacency
    std::vector<std::vector<uint32_t>> vertFaces(m_vertices.size());
    for (size_t f = 0; f < m_faces.size(); ++f) {
        if (m_faces[f].deleted) continue;
        for (uint32_t v : m_faces[f].vertices) {
            if (v < vertFaces.size()) {
                vertFaces[v].push_back(static_cast<uint32_t>(f));
            }
        }
    }

    float cosThreshold = std::cos(autoSmoothAngleDeg * (3.14159265f / 180.0f));

    // Calculate face-corner normals for every corner of every face
    for (size_t f = 0; f < m_faces.size(); ++f) {
        auto& face = m_faces[f];
        if (face.deleted) continue;

        size_t count = face.vertices.size();
        face.normals.resize(count);

        if (!smooth) {
            for (size_t i = 0; i < count; ++i) {
                face.normals[i] = face.normal;
            }
        } else {
            for (size_t i = 0; i < count; ++i) {
                uint32_t v = face.vertices[i];
                Engine::Math::Vector3 cornerNormal(0, 0, 0);

                // Cluster connected face normals sharing same smoothing angle
                for (uint32_t connFaceIdx : vertFaces[v]) {
                    const auto& connFace = m_faces[connFaceIdx];
                    if (connFace.normal.dot(face.normal) >= cosThreshold) {
                        cornerNormal += connFace.normal;
                    }
                }

                if (cornerNormal.length() > 1e-3f) {
                    face.normals[i] = cornerNormal.normalized();
                } else {
                    face.normals[i] = face.normal;
                }
            }
        }
    }

    // Update vertex primary normal for general queries
    for (size_t v = 0; v < m_vertices.size(); ++v) {
        if (m_vertices[v].deleted || vertFaces[v].empty()) continue;
        Engine::Math::Vector3 avg(0, 0, 0);
        for (uint32_t fIdx : vertFaces[v]) {
            avg += m_faces[fIdx].normal;
        }
        if (avg.length() > 1e-3f) m_vertices[v].normal = avg.normalized();
    }
}

void EditableMesh::rebuildTopology() {
    m_halfEdges.clear();
    m_edgeMap.clear();
    m_edges.clear();

    for (size_t v = 0; v < m_vertices.size(); ++v) {
        m_vertices[v].firstHalfEdge = -1;
    }

    // Direct mapping from directed edge (v0 -> v1) to half-edge index
    std::unordered_map<uint64_t, int> directedHalfEdgeMap;

    for (size_t f = 0; f < m_faces.size(); ++f) {
        auto& face = m_faces[f];
        if (face.deleted || face.vertices.size() < 3) continue;

        calculateFaceNormal(static_cast<uint32_t>(f));
        int faceFirstHe = static_cast<int>(m_halfEdges.size());
        face.firstHalfEdge = faceFirstHe;
        face.edges.clear();

        size_t count = face.vertices.size();
        for (size_t i = 0; i < count; ++i) {
            uint32_t v0 = face.vertices[i];
            uint32_t v1 = face.vertices[(i + 1) % count];

            int edgeIdx = addEdge(v0, v1);
            face.edges.push_back(static_cast<uint32_t>(edgeIdx));

            MeshHalfEdge he;
            he.origin = v0;
            he.face = static_cast<int>(f);
            he.edge = edgeIdx;
            
            int curHeIdx = static_cast<int>(m_halfEdges.size());
            m_halfEdges.push_back(he);

            uint64_t directedKey = (static_cast<uint64_t>(v0) << 32) | static_cast<uint64_t>(v1);
            directedHalfEdgeMap[directedKey] = curHeIdx;

            if (m_vertices[v0].firstHalfEdge == -1) {
                m_vertices[v0].firstHalfEdge = curHeIdx;
            }

            if (edgeIdx >= 0 && edgeIdx < static_cast<int>(m_edges.size())) {
                if (m_edges[edgeIdx].halfEdge0 == -1) m_edges[edgeIdx].halfEdge0 = curHeIdx;
                else if (m_edges[edgeIdx].halfEdge1 == -1) m_edges[edgeIdx].halfEdge1 = curHeIdx;
            }
        }

        // Link next / prev within face loop
        for (size_t i = 0; i < count; ++i) {
            int curHe = faceFirstHe + static_cast<int>(i);
            int nextHe = faceFirstHe + static_cast<int>((i + 1) % count);
            int prevHe = faceFirstHe + static_cast<int>((i + count - 1) % count);
            m_halfEdges[curHe].next = nextHe;
            m_halfEdges[curHe].prev = prevHe;
        }
    }

    // Link twins
    for (size_t heIdx = 0; heIdx < m_halfEdges.size(); ++heIdx) {
        auto& he = m_halfEdges[heIdx];
        if (he.next == -1) continue;
        uint32_t v0 = he.origin;
        uint32_t v1 = m_halfEdges[he.next].origin;
        uint64_t twinKey = (static_cast<uint64_t>(v1) << 32) | static_cast<uint64_t>(v0);
        auto it = directedHalfEdgeMap.find(twinKey);
        if (it != directedHalfEdgeMap.end()) {
            he.twin = it->second;
        }
    }
}

void EditableMesh::packAndCompact(std::vector<uint32_t>* outVertMap, std::vector<uint32_t>* outFaceMap) {
    std::vector<MeshVertex> newVertices;
    std::vector<int> vertRemap(m_vertices.size(), -1);

    for (size_t i = 0; i < m_vertices.size(); ++i) {
        if (!m_vertices[i].deleted) {
            vertRemap[i] = static_cast<int>(newVertices.size());
            newVertices.push_back(m_vertices[i]);
        }
    }

    if (outVertMap) {
        outVertMap->resize(m_vertices.size());
        for (size_t i = 0; i < m_vertices.size(); ++i) {
            (*outVertMap)[i] = (vertRemap[i] >= 0) ? static_cast<uint32_t>(vertRemap[i]) : 0xFFFFFFFF;
        }
    }

    std::vector<MeshFace> newFaces;
    std::vector<int> faceRemap(m_faces.size(), -1);
    for (size_t f = 0; f < m_faces.size(); ++f) {
        if (m_faces[f].deleted) continue;
        MeshFace face = m_faces[f];
        bool valid = true;
        for (auto& v : face.vertices) {
            if (vertRemap[v] == -1) { valid = false; break; }
            v = static_cast<uint32_t>(vertRemap[v]);
        }
        if (valid && face.vertices.size() >= 3) {
            faceRemap[f] = static_cast<int>(newFaces.size());
            newFaces.push_back(face);
        }
    }

    if (outFaceMap) {
        outFaceMap->resize(m_faces.size());
        for (size_t f = 0; f < m_faces.size(); ++f) {
            (*outFaceMap)[f] = (faceRemap[f] >= 0) ? static_cast<uint32_t>(faceRemap[f]) : 0xFFFFFFFF;
        }
    }

    m_vertices = std::move(newVertices);
    m_faces = std::move(newFaces);
    m_edges.clear();
    m_halfEdges.clear();
    m_edgeMap.clear();

    rebuildTopology();
}

void EditableMesh::computeBounds(Engine::Math::Vector3& outMin, Engine::Math::Vector3& outMax) const {
    outMin = { 1e9f,  1e9f,  1e9f};
    outMax = {-1e9f, -1e9f, -1e9f};
    bool anyVert = false;
    for (const auto& v : m_vertices) {
        if (v.deleted) continue;
        anyVert = true;
        outMin.x = std::min(outMin.x, v.position.x);
        outMin.y = std::min(outMin.y, v.position.y);
        outMin.z = std::min(outMin.z, v.position.z);
        outMax.x = std::max(outMax.x, v.position.x);
        outMax.y = std::max(outMax.y, v.position.y);
        outMax.z = std::max(outMax.z, v.position.z);
    }
    if (!anyVert) {
        outMin = {0, 0, 0};
        outMax = {0, 0, 0};
    }
}

int EditableMesh::findEdge(uint32_t v0, uint32_t v1) const {
    uint64_t key = makeEdgeKey(v0, v1);
    auto it = m_edgeMap.find(key);
    return (it != m_edgeMap.end()) ? it->second : -1;
}

std::vector<uint32_t> EditableMesh::getConnectedFaces(uint32_t vertexIndex) const {
    std::vector<uint32_t> result;
    for (size_t f = 0; f < m_faces.size(); ++f) {
        if (m_faces[f].deleted) continue;
        for (uint32_t v : m_faces[f].vertices) {
            if (v == vertexIndex) {
                result.push_back(static_cast<uint32_t>(f));
                break;
            }
        }
    }
    return result;
}

std::vector<uint32_t> EditableMesh::getConnectedEdges(uint32_t vertexIndex) const {
    std::vector<uint32_t> result;
    for (size_t e = 0; e < m_edges.size(); ++e) {
        if (m_edges[e].deleted) continue;
        if (m_edges[e].v0 == vertexIndex || m_edges[e].v1 == vertexIndex) {
            result.push_back(static_cast<uint32_t>(e));
        }
    }
    return result;
}

std::vector<uint32_t> EditableMesh::getAdjacentVertices(uint32_t vertexIndex) const {
    std::vector<uint32_t> result;
    for (size_t e = 0; e < m_edges.size(); ++e) {
        if (m_edges[e].deleted) continue;
        if (m_edges[e].v0 == vertexIndex) result.push_back(m_edges[e].v1);
        else if (m_edges[e].v1 == vertexIndex) result.push_back(m_edges[e].v0);
    }
    return result;
}

std::vector<uint32_t> EditableMesh::getEdgeFaces(uint32_t edgeIndex) const {
    std::vector<uint32_t> result;
    if (edgeIndex >= m_edges.size() || m_edges[edgeIndex].deleted) return result;
    uint32_t v0 = m_edges[edgeIndex].v0;
    uint32_t v1 = m_edges[edgeIndex].v1;
    for (size_t f = 0; f < m_faces.size(); ++f) {
        if (m_faces[f].deleted) continue;
        const auto& verts = m_faces[f].vertices;
        size_t count = verts.size();
        for (size_t i = 0; i < count; ++i) {
            uint32_t fv0 = verts[i];
            uint32_t fv1 = verts[(i + 1) % count];
            if ((fv0 == v0 && fv1 == v1) || (fv0 == v1 && fv1 == v0)) {
                result.push_back(static_cast<uint32_t>(f));
                break;
            }
        }
    }
    return result;
}

void EditableMesh::generateRenderBuffers(
    std::vector<RenderVertex>& outVertices,
    std::vector<uint32_t>& outIndices,
    std::vector<uint32_t>& outLineIndices
) const {
    outVertices.clear();
    outIndices.clear();
    outLineIndices.clear();

    // Flat shaded triangulation with face normals & UVs
    for (size_t f = 0; f < m_faces.size(); ++f) {
        const auto& face = m_faces[f];
        if (face.deleted || face.vertices.size() < 3) continue;

        bool validFace = true;
        for (uint32_t vIdx : face.vertices) {
            if (vIdx >= m_vertices.size() || m_vertices[vIdx].deleted) {
                validFace = false;
                break;
            }
        }
        if (!validFace) continue;

        uint32_t baseVertexIdx = static_cast<uint32_t>(outVertices.size());

        // Emit vertices for this face
        bool hasFaceUVs = (face.uvs.size() == face.vertices.size());
        bool hasCornerNormals = (face.normals.size() == face.vertices.size());

        for (size_t i = 0; i < face.vertices.size(); ++i) {
            uint32_t vIdx = face.vertices[i];
            const auto& v = m_vertices[vIdx];

            RenderVertex rv;
            rv.x = v.position.x;
            rv.y = v.position.y;
            rv.z = v.position.z;
            rv.abgr = v.color;
            rv.nx = hasCornerNormals ? face.normals[i].x : face.normal.x;
            rv.ny = hasCornerNormals ? face.normals[i].y : face.normal.y;
            rv.nz = hasCornerNormals ? face.normals[i].z : face.normal.z;
            rv.u = hasFaceUVs ? face.uvs[i].first : v.u;
            rv.v = hasFaceUVs ? face.uvs[i].second : v.v;
            outVertices.push_back(rv);
        }

        // Robust Ear Clipping / Fan Triangulation
        size_t vCount = face.vertices.size();
        if (vCount == 3) {
            outIndices.push_back(baseVertexIdx);
            outIndices.push_back(baseVertexIdx + 1);
            outIndices.push_back(baseVertexIdx + 2);
        } else if (vCount == 4) {
            outIndices.push_back(baseVertexIdx);
            outIndices.push_back(baseVertexIdx + 1);
            outIndices.push_back(baseVertexIdx + 2);

            outIndices.push_back(baseVertexIdx);
            outIndices.push_back(baseVertexIdx + 2);
            outIndices.push_back(baseVertexIdx + 3);
        } else {
            // Project polygon to 2D for ear clipping
            Engine::Math::Vector3 normal = face.normal;
            Engine::Math::Vector3 axisX = (std::abs(normal.x) > 0.9f) ? Engine::Math::Vector3(0, 1, 0) : Engine::Math::Vector3(1, 0, 0);
            Engine::Math::Vector3 uAxis = normal.cross(axisX).normalized();
            Engine::Math::Vector3 vAxis = normal.cross(uAxis).normalized();

            std::vector<std::pair<float, float>> poly2D(vCount);
            std::vector<int> polyIndices(vCount);
            for (size_t i = 0; i < vCount; ++i) {
                const auto& p = m_vertices[face.vertices[i]].position;
                poly2D[i] = { p.dot(uAxis), p.dot(vAxis) };
                polyIndices[i] = static_cast<int>(i);
            }

            // Normalize winding to CCW
            float signedArea = 0.0f;
            for (size_t i = 0; i < vCount; ++i) {
                size_t next = (i + 1) % vCount;
                signedArea += poly2D[i].first * poly2D[next].second - poly2D[next].first * poly2D[i].second;
            }
            if (signedArea < 0.0f) {
                std::reverse(poly2D.begin(), poly2D.end());
                std::reverse(polyIndices.begin(), polyIndices.end());
            }

            auto isConvex = [&](int prev, int curr, int next) -> bool {
                float x1 = poly2D[curr].first - poly2D[prev].first;
                float y1 = poly2D[curr].second - poly2D[prev].second;
                float x2 = poly2D[next].first - poly2D[curr].first;
                float y2 = poly2D[next].second - poly2D[curr].second;
                return (x1 * y2 - y1 * x2) >= 0.0f;
            };

            auto pointInTriangle2D = [](float px, float py, float ax, float ay, float bx, float by, float cx, float cy) -> bool {
                float d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
                float d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
                float d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
                bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
                bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
                return !(has_neg && has_pos);
            };

            int maxIters = static_cast<int>(vCount * 3);
            while (polyIndices.size() > 2 && maxIters-- > 0) {
                size_t n = polyIndices.size();
                bool earFound = false;

                for (size_t i = 0; i < n; ++i) {
                    int prevIdx = polyIndices[(i + n - 1) % n];
                    int currIdx = polyIndices[i];
                    int nextIdx = polyIndices[(i + 1) % n];

                    if (isConvex(prevIdx, currIdx, nextIdx)) {
                        bool containsOther = false;
                        for (size_t j = 0; j < n; ++j) {
                            int checkIdx = polyIndices[j];
                            if (checkIdx == prevIdx || checkIdx == currIdx || checkIdx == nextIdx) continue;
                            if (pointInTriangle2D(poly2D[checkIdx].first, poly2D[checkIdx].second,
                                                  poly2D[prevIdx].first, poly2D[prevIdx].second,
                                                  poly2D[currIdx].first, poly2D[currIdx].second,
                                                  poly2D[nextIdx].first, poly2D[nextIdx].second)) {
                                containsOther = true;
                                break;
                            }
                        }

                        if (!containsOther) {
                            outIndices.push_back(baseVertexIdx + prevIdx);
                            outIndices.push_back(baseVertexIdx + currIdx);
                            outIndices.push_back(baseVertexIdx + nextIdx);
                            polyIndices.erase(polyIndices.begin() + i);
                            earFound = true;
                            break;
                        }
                    }
                }

                if (!earFound) {
                    // Fallback to fan if ear clipping fails on degenerate geometry
                    for (size_t i = 1; i + 1 < polyIndices.size(); ++i) {
                        outIndices.push_back(baseVertexIdx + polyIndices[0]);
                        outIndices.push_back(baseVertexIdx + polyIndices[i]);
                        outIndices.push_back(baseVertexIdx + polyIndices[i + 1]);
                    }
                    break;
                }
            }
        }
    }

    // Wireframe / Edge line buffer
    for (size_t e = 0; e < m_edges.size(); ++e) {
        const auto& edge = m_edges[e];
        if (edge.deleted || edge.v0 >= m_vertices.size() || edge.v1 >= m_vertices.size()) continue;

        uint32_t baseLineIdx = static_cast<uint32_t>(outVertices.size());

        const auto& v0 = m_vertices[edge.v0];
        const auto& v1 = m_vertices[edge.v1];

        RenderVertex rv0, rv1;
        rv0.x = v0.position.x; rv0.y = v0.position.y; rv0.z = v0.position.z;
        rv0.abgr = edge.selected ? 0xFF00FFFF : 0xFFFFFFFF;
        rv0.nx = 0; rv0.ny = 1; rv0.nz = 0; rv0.u = 0; rv0.v = 0;

        rv1.x = v1.position.x; rv1.y = v1.position.y; rv1.z = v1.position.z;
        rv1.abgr = edge.selected ? 0xFF00FFFF : 0xFFFFFFFF;
        rv1.nx = 0; rv1.ny = 1; rv1.nz = 0; rv1.u = 0; rv1.v = 0;

        outVertices.push_back(rv0);
        outVertices.push_back(rv1);

        outLineIndices.push_back(baseLineIdx);
        outLineIndices.push_back(baseLineIdx + 1);
    }
}

} // namespace Engine::Geometry
