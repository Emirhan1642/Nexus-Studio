#pragma once
#include "EditableMesh.h"
#include <vector>
#include <cstdint>

namespace Engine::Geometry {

enum class MergeMode {
    Center,
    Cursor,
    First,
    Last,
    Collapse,
    Distance
};

enum class SubElementType {
    Vertex,
    Edge,
    Face
};

class MeshOperators {
public:
    // ── 1. Extrude ──────────────────────────────────────────────────────────
    static std::vector<uint32_t> extrudeFaces(
        EditableMesh& mesh,
        const std::vector<uint32_t>& faceIndices,
        float distance = 1.0f,
        const Engine::Math::Vector3& customDirection = {0, 0, 0},
        bool individual = false
    );

    static std::vector<uint32_t> extrudeEdges(
        EditableMesh& mesh,
        const std::vector<uint32_t>& edgeIndices,
        float distance = 1.0f,
        const Engine::Math::Vector3& direction = {0, 1, 0}
    );

    // ── 2. Inset Faces ──────────────────────────────────────────────────────
    static std::vector<uint32_t> insetFaces(
        EditableMesh& mesh,
        const std::vector<uint32_t>& faceIndices,
        float thickness = 0.2f,
        float depth = 0.0f,
        bool individual = false
    );

    // ── 3. Bevel / Chamfer ──────────────────────────────────────────────────
    static void bevelFaces(
        EditableMesh& mesh,
        const std::vector<uint32_t>& faceIndices,
        float width = 0.2f,
        int segments = 1,
        float profile = 0.5f,
        float depth = 0.0f
    );

    static void bevelEdges(
        EditableMesh& mesh,
        const std::vector<uint32_t>& edgeIndices,
        float width = 0.2f,
        int segments = 1,
        float profile = 0.5f // 0.0 = concave, 0.5 = round, 1.0 = straight/chamfer
    );

    static void bevelVertices(
        EditableMesh& mesh,
        const std::vector<uint32_t>& vertIndices,
        float width = 0.2f,
        int segments = 1
    );

    // ── 4. Subdivide ────────────────────────────────────────────────────────
    static void subdivideFaces(
        EditableMesh& mesh,
        const std::vector<uint32_t>& faceIndices,
        int cuts = 1,
        float smoothness = 0.0f
    );

    // ── 5. Merge / Weld ─────────────────────────────────────────────────────
    static void mergeVertices(
        EditableMesh& mesh,
        const std::vector<uint32_t>& vertIndices,
        MergeMode mode = MergeMode::Center,
        const Engine::Math::Vector3& targetPos = {0, 0, 0}
    );

    static void weldVerticesByDistance(
        EditableMesh& mesh,
        float distanceThreshold = 0.001f
    );

    // ── 6. Delete & Dissolve ────────────────────────────────────────────────
    static void deleteElements(
        EditableMesh& mesh,
        const std::vector<uint32_t>& indices,
        SubElementType type
    );

    static void dissolveEdges(
        EditableMesh& mesh,
        const std::vector<uint32_t>& edgeIndices
    );

    static void dissolveVertices(
        EditableMesh& mesh,
        const std::vector<uint32_t>& vertIndices
    );

    // ── 7. Fill & Bridge ────────────────────────────────────────────────────
    static int fillFace(
        EditableMesh& mesh,
        const std::vector<uint32_t>& vertIndices
    );

    static void bridgeEdgeLoops(
        EditableMesh& mesh,
        const std::vector<uint32_t>& loop1Verts,
        const std::vector<uint32_t>& loop2Verts
    );

    // ── 8. Advanced Topology & Utility Operators ────────────────────────────
    static void pokeFaces(
        EditableMesh& mesh,
        const std::vector<uint32_t>& faceIndices,
        float offset = 0.0f
    );

    static void triangulateFaces(
        EditableMesh& mesh,
        const std::vector<uint32_t>& faceIndices
    );

    static void trisToQuads(
        EditableMesh& mesh,
        const std::vector<uint32_t>& faceIndices
    );

    static void slideVertices(
        EditableMesh& mesh,
        const std::vector<uint32_t>& vertIndices,
        float factor
    );

    static void edgeSplit(
        EditableMesh& mesh,
        const std::vector<uint32_t>& edgeIndices
    );

    static void flipNormals(
        EditableMesh& mesh,
        const std::vector<uint32_t>& faceIndices
    );
};

} // namespace Engine::Geometry
