#pragma once
#include "EditableMesh.h"
#include <vector>
#include <cstdint>

namespace Engine::Geometry {

enum class MirrorAxis {
    X,
    Y,
    Z
};

enum class BooleanOperation {
    Union,
    Difference,
    Intersect
};

class MeshCutOperators {
public:
    // ── Loop Cut & Slide ────────────────────────────────────────────────────
    static std::vector<uint32_t> findEdgeLoop(
        const EditableMesh& mesh,
        uint32_t startEdgeIdx
    );

    static std::vector<uint32_t> applyLoopCut(
        EditableMesh& mesh,
        const std::vector<uint32_t>& edgeLoop,
        float slideFactor = 0.0f, // -1.0 (towards v0) to +1.0 (towards v1)
        int numCuts = 1
    );

    // ── Knife Tool (Polyline Slice) ──────────────────────────────────────────
    static bool cutMeshWithKnifePolyline(
        EditableMesh& mesh,
        const std::vector<Engine::Math::Vector3>& localPoints,
        const std::vector<uint32_t>& targetFaces = {},
        bool cutThrough = false
    );

    static bool cutFaceWithRaySegment(
        EditableMesh& mesh,
        uint32_t faceIndex,
        const Engine::Math::Vector3& p0,
        const Engine::Math::Vector3& p1,
        bool compactResult = true
    );

    // ── Mirror & Symmetry ───────────────────────────────────────────────────
    static void applyMirror(
        EditableMesh& mesh,
        MirrorAxis axis = MirrorAxis::X,
        bool mergeCenter = true,
        float mergeThreshold = 0.01f
    );

    // ── Boolean CSG (Union, Difference, Intersect) ──────────────────────────
    // BSP polygon CSG for closed, consistently wound polygon meshes.
    static std::shared_ptr<EditableMesh> applyBoolean(
        const EditableMesh& meshA,
        const EditableMesh& meshB,
        BooleanOperation op
    );
};

} // namespace Engine::Geometry
