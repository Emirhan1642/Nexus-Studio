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
        if (face.vertices.size() < 3) return false;

        std::unordered_set<uint32_t> seen;
        for (size_t i = 0; i < face.vertices.size(); ++i) {
            uint32_t v = face.vertices[i];
            if (v >= m_vertices.size() || m_vertices[v].deleted) return false;
            if (seen.count(v)) return false; // Duplicate vertex in same face
            seen.insert(v);
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

    if (!smooth) {
        for (auto& v : m_vertices) {
            v.normal = {0.0f, 1.0f, 0.0f};
        }
        for (const auto& f : m_faces) {
            if (f.deleted) continue;
            for (uint32_t v : f.vertices) {
                m_vertices[v].normal = f.normal;
            }
        }
    } else {
        float cosThreshold = std::cos(autoSmoothAngleDeg * (3.14159265f / 180.0f));
        std::vector<std::vector<Engine::Math::Vector3>> vertFaceNormals(m_vertices.size());
        for (const auto& f : m_faces) {
            if (f.deleted) continue;
            for (uint32_t v : f.vertices) {
                vertFaceNormals[v].push_back(f.normal);
            }
        }

        for (size_t v = 0; v < m_vertices.size(); ++v) {
            if (m_vertices[v].deleted || vertFaceNormals[v].empty()) continue;
            Engine::Math::Vector3 avgNormal(0, 0, 0);
            for (const auto& fn : vertFaceNormals[v]) {
                avgNormal += fn;
            }
            m_vertices[v].normal = avgNormal.normalized();
        }
    }
}

void EditableMesh::rebuildTopology() {
    m_halfEdges.clear();
    m_edgeMap.clear();

    // Rebuild edge map and edge list if necessary
    for (size_t e = 0; e < m_edges.size(); ++e) {
        if (m_edges[e].deleted) continue;
        m_edges[e].halfEdge0 = -1;
        m_edges[e].halfEdge1 = -1;
        uint64_t key = makeEdgeKey(m_edges[e].v0, m_edges[e].v1);
        m_edgeMap[key] = static_cast<int>(e);
    }

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

        uint32_t baseVertexIdx = static_cast<uint32_t>(outVertices.size());

        // Emit vertices for this face
        bool hasFaceUVs = (face.uvs.size() == face.vertices.size());
        for (size_t i = 0; i < face.vertices.size(); ++i) {
            uint32_t vIdx = face.vertices[i];
            const auto& v = m_vertices[vIdx];

            RenderVertex rv;
            rv.x = v.position.x;
            rv.y = v.position.y;
            rv.z = v.position.z;
            rv.abgr = v.color;
            rv.nx = face.normal.x;
            rv.ny = face.normal.y;
            rv.nz = face.normal.z;
            rv.u = hasFaceUVs ? face.uvs[i].first : v.u;
            rv.v = hasFaceUVs ? face.uvs[i].second : v.v;
            outVertices.push_back(rv);
        }

        // Fan triangulation for n-gon/quad
        for (size_t i = 1; i + 1 < face.vertices.size(); ++i) {
            outIndices.push_back(baseVertexIdx);
            outIndices.push_back(baseVertexIdx + static_cast<uint32_t>(i));
            outIndices.push_back(baseVertexIdx + static_cast<uint32_t>(i + 1));
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
