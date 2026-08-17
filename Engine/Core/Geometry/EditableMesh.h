#pragma once
#include "../Math/Vector3.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Engine::Geometry {

struct MeshVertex {
    Engine::Math::Vector3 position{0.0f, 0.0f, 0.0f};
    Engine::Math::Vector3 normal{0.0f, 1.0f, 0.0f};
    float u = 0.0f;
    float v = 0.0f;
    uint32_t color = 0xFFFFFFFF;
    int firstHalfEdge = -1;
    bool deleted = false;
    bool selected = false;
};

struct MeshHalfEdge {
    uint32_t origin = 0;       // Starting vertex index
    int twin = -1;             // Opposite half-edge index (-1 if boundary)
    int next = -1;             // Next half-edge in face loop
    int prev = -1;             // Previous half-edge in face loop
    int edge = -1;             // Index into edge array
    int face = -1;             // Index of face it belongs to
};

struct MeshEdge {
    uint32_t v0 = 0;
    uint32_t v1 = 0;
    int halfEdge0 = -1;
    int halfEdge1 = -1;
    bool isSharp = false;
    bool deleted = false;
    bool selected = false;
};

struct MeshFace {
    std::vector<uint32_t> vertices; // Vertex indices in CCW order
    std::vector<uint32_t> edges;    // Edge indices
    Engine::Math::Vector3 normal{0.0f, 1.0f, 0.0f};
    Engine::Math::Vector3 center{0.0f, 0.0f, 0.0f};
    int firstHalfEdge = -1;
    bool deleted = false;
    bool selected = false;
};

struct RenderVertex {
    float x, y, z;
    uint32_t abgr;
    float nx, ny, nz;
    float u, v;
};

class EditableMesh {
public:
    EditableMesh() = default;
    EditableMesh(const EditableMesh& other) = default;
    EditableMesh& operator=(const EditableMesh& other) = default;

    // Direct element access
    std::vector<MeshVertex>& getVertices() { return m_vertices; }
    const std::vector<MeshVertex>& getVertices() const { return m_vertices; }

    std::vector<MeshEdge>& getEdges() { return m_edges; }
    const std::vector<MeshEdge>& getEdges() const { return m_edges; }

    std::vector<MeshFace>& getFaces() { return m_faces; }
    const std::vector<MeshFace>& getFaces() const { return m_faces; }

    std::vector<MeshHalfEdge>& getHalfEdges() { return m_halfEdges; }
    const std::vector<MeshHalfEdge>& getHalfEdges() const { return m_halfEdges; }

    // Topology builders
    uint32_t addVertex(const Engine::Math::Vector3& pos, float u = 0.0f, float v = 0.0f, const Engine::Math::Vector3& normal = {0, 1, 0});
    int addEdge(uint32_t v0, uint32_t v1);
    int addFace(const std::vector<uint32_t>& vertIndices);

    // Deletion
    void removeVertex(uint32_t index);
    void removeEdge(uint32_t index);
    void removeFace(uint32_t index);

    // Maintenance & Topology Analysis
    void rebuildTopology();
    void packAndCompact(); // Removes flagged 'deleted' elements and re-indexes
    void clear();

    // Normals & Centers
    void calculateFaceNormal(uint32_t faceIndex);
    void recalculateAllNormals(bool smooth = false, float autoSmoothAngleDeg = 30.0f);
    void computeBounds(Engine::Math::Vector3& outMin, Engine::Math::Vector3& outMax) const;

    // Queries
    int findEdge(uint32_t v0, uint32_t v1) const;
    std::vector<uint32_t> getConnectedFaces(uint32_t vertexIndex) const;
    std::vector<uint32_t> getConnectedEdges(uint32_t vertexIndex) const;
    std::vector<uint32_t> getAdjacentVertices(uint32_t vertexIndex) const;
    std::vector<uint32_t> getEdgeFaces(uint32_t edgeIndex) const;

    // Triangulation & GPU Buffer generation
    void generateRenderBuffers(
        std::vector<RenderVertex>& outVertices,
        std::vector<uint32_t>& outIndices,
        std::vector<uint32_t>& outLineIndices
    ) const;

    std::shared_ptr<EditableMesh> clone() const {
        return std::make_shared<EditableMesh>(*this);
    }

private:
    std::vector<MeshVertex> m_vertices;
    std::vector<MeshEdge> m_edges;
    std::vector<MeshFace> m_faces;
    std::vector<MeshHalfEdge> m_halfEdges;

    // Helper for fast edge lookup (min(v0, v1), max(v0, v1)) -> edgeIndex
    mutable std::unordered_map<uint64_t, int> m_edgeMap;
    static uint64_t makeEdgeKey(uint32_t v0, uint32_t v1) {
        uint32_t minV = std::min(v0, v1);
        uint32_t maxV = std::max(v0, v1);
        return (static_cast<uint64_t>(minV) << 32) | static_cast<uint64_t>(maxV);
    }
};

} // namespace Engine::Geometry
