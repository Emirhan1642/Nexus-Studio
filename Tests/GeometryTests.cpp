#include <gtest/gtest.h>
#include "Engine/Core/Geometry/EditableMesh.h"
#include "Engine/Core/Geometry/MeshPrimitives.h"
#include "Engine/Core/Geometry/MeshOperators.h"
#include "Engine/Core/Geometry/MeshCutOperators.h"
#include "Editor/Modeling/ModelingContext.h"
#include "Editor/Undo/UndoStack.h"
#include <algorithm>

using namespace Engine::Geometry;
using namespace Engine::Math;

class GeometryTests : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 1. Primitive Cube Invariant & Validation
TEST_F(GeometryTests, CubePrimitivesValidation) {
    auto cube = MeshPrimitives::createCube(Vector3(2.0f, 2.0f, 2.0f));
    ASSERT_NE(cube, nullptr);
    EXPECT_TRUE(cube->validate());
    EXPECT_EQ(cube->getVertices().size(), 8);
    EXPECT_EQ(cube->getFaces().size(), 6);

    for (const auto& face : cube->getFaces()) {
        EXPECT_EQ(face.vertices.size(), 4);
    }
}

// 2. Region Extrude Multi-Face: No Internal Faces Created
TEST_F(GeometryTests, RegionExtrudeMultiFaceNoInternalWalls) {
    auto cube = MeshPrimitives::createCube(Vector3(1.0f, 1.0f, 1.0f));
    ASSERT_NE(cube, nullptr);

    // Initial 6 quads on a cube. Extrude adjacent face 0 (+Z) and face 2 (+Y).
    // Face 0 and Face 2 share exactly 1 edge (v6 - v7).
    // Outer boundary has 6 edges -> must create exactly 6 side quads.
    // Total faces after extrude: original 6 + 6 side quads = 12 faces.
    size_t initialFaceCount = cube->getFaces().size();
    auto extruded = MeshOperators::extrudeFaces(*cube, {0, 2}, 1.0f, {0, 0, 0}, false);
    EXPECT_TRUE(cube->validate());
    EXPECT_EQ(cube->getFaces().size(), initialFaceCount + 6);
}

// 3. Inset Faces Uniform Distance
TEST_F(GeometryTests, InsetFacesUniformOffset) {
    auto cube = MeshPrimitives::createCube(Vector3(1.0f, 1.0f, 1.0f));
    ASSERT_NE(cube, nullptr);

    float thickness = 0.2f;
    auto insets = MeshOperators::insetFaces(*cube, {0}, thickness, 0.0f);
    EXPECT_TRUE(cube->validate());
    ASSERT_FALSE(insets.empty());

    uint32_t capFaceIdx = insets[0];
    const auto& capFace = cube->getFaces()[capFaceIdx];
    ASSERT_EQ(capFace.vertices.size(), 4);

    // Verify all 4 edges of the inset cap have exact length 1.600
    for (size_t i = 0; i < 4; ++i) {
        size_t next = (i + 1) % 4;
        const auto& p0 = cube->getVertices()[capFace.vertices[i]].position;
        const auto& p1 = cube->getVertices()[capFace.vertices[next]].position;
        float edgeLen = (p1 - p0).length();
        EXPECT_NEAR(edgeLen, 1.6f, 0.01f);
    }
}

// 4. Solidify Open Sheet with Rim Walls
TEST_F(GeometryTests, SolidifyOpenPlaneWithRim) {
    auto mesh = std::make_shared<EditableMesh>();
    // Single Quad sheet in XY plane: Z = 0
    uint32_t v0 = mesh->addVertex(Vector3(0, 0, 0));
    uint32_t v1 = mesh->addVertex(Vector3(2, 0, 0));
    uint32_t v2 = mesh->addVertex(Vector3(2, 2, 0));
    uint32_t v3 = mesh->addVertex(Vector3(0, 2, 0));
    mesh->addFace({v0, v1, v2, v3});
    mesh->rebuildTopology();
    EXPECT_TRUE(mesh->validate());

    // Solidify with thickness 0.5 along -Z direction:
    MeshOperators::solidify(*mesh, 0.5f, true);
    EXPECT_TRUE(mesh->validate());
    EXPECT_EQ(mesh->getFaces().size(), 6);
    EXPECT_EQ(mesh->getVertices().size(), 8);

    // Verify inner shell vertices are at exact thickness offset Z = -0.5
    for (size_t i = 4; i < 8; ++i) {
        EXPECT_NEAR(mesh->getVertices()[i].position.z, -0.5f, 0.01f);
    }
}

// 5. Ear-Clipping Triangulation on Concave N-Gon
TEST_F(GeometryTests, TriangulateFacesEarClipping) {
    auto mesh = std::make_shared<EditableMesh>();
    // L-shaped concave polygon: (0,0), (2,0), (2,1), (1,1), (1,2), (0,2)
    uint32_t v0 = mesh->addVertex(Vector3(0, 0, 0));
    uint32_t v1 = mesh->addVertex(Vector3(2, 0, 0));
    uint32_t v2 = mesh->addVertex(Vector3(2, 1, 0));
    uint32_t v3 = mesh->addVertex(Vector3(1, 1, 0));
    uint32_t v4 = mesh->addVertex(Vector3(1, 2, 0));
    uint32_t v5 = mesh->addVertex(Vector3(0, 2, 0));

    int fIdx = mesh->addFace({v0, v1, v2, v3, v4, v5});
    mesh->rebuildTopology();
    EXPECT_TRUE(mesh->validate());

    MeshOperators::triangulateFaces(*mesh, {static_cast<uint32_t>(fIdx)});
    EXPECT_TRUE(mesh->validate());
    // An n-gon with 6 vertices must produce exactly 6 - 2 = 4 triangles
    EXPECT_EQ(mesh->getFaces().size(), 4);
    for (const auto& face : mesh->getFaces()) {
        EXPECT_EQ(face.vertices.size(), 3);
    }
}

// 6. Auto-Smooth Corner Normals vs Hard Normals
TEST_F(GeometryTests, AutoSmoothCornerNormals) {
    auto cube = MeshPrimitives::createCube(Vector3(2.0f, 2.0f, 2.0f));
    ASSERT_NE(cube, nullptr);

    // Cube has 90 degree sharp corners.
    // 1. With autoSmoothAngleDeg = 30.0f (threshold < 90 deg), all face corner normals MUST be sharp (equal to face normal)
    cube->recalculateAllNormals(true, 30.0f);
    EXPECT_TRUE(cube->validate());
    for (const auto& face : cube->getFaces()) {
        ASSERT_EQ(face.normals.size(), face.vertices.size());
        for (const auto& cn : face.normals) {
            EXPECT_NEAR(cn.x, face.normal.x, 0.01f);
            EXPECT_NEAR(cn.y, face.normal.y, 0.01f);
            EXPECT_NEAR(cn.z, face.normal.z, 0.01f);
        }
    }

    // 2. With autoSmoothAngleDeg = 120.0f (threshold > 90 deg), face corner normals MUST be smoothed across faces
    cube->recalculateAllNormals(true, 120.0f);
    EXPECT_TRUE(cube->validate());
    for (const auto& face : cube->getFaces()) {
        ASSERT_EQ(face.normals.size(), face.vertices.size());
        for (const auto& cn : face.normals) {
            // At a 3-way cube corner, smoothed normal magnitude in any axis is ~ 1/sqrt(3) ≈ 0.577, NOT 1.0
            float maxComp = std::max({std::abs(cn.x), std::abs(cn.y), std::abs(cn.z)});
            EXPECT_LT(maxComp, 0.9f);
        }
    }
}

// 7. Weld By Distance Coincident Vertices (Union-Find)
TEST_F(GeometryTests, WeldVerticesByDistance) {
    auto mesh = std::make_shared<EditableMesh>();
    uint32_t v0 = mesh->addVertex(Vector3(0, 0, 0));
    uint32_t v1 = mesh->addVertex(Vector3(1, 0, 0));
    uint32_t v2 = mesh->addVertex(Vector3(0, 1, 0));
    mesh->addFace({v0, v1, v2});

    // Add duplicate overlapping vertex at (0, 0, 0)
    uint32_t v3 = mesh->addVertex(Vector3(0.0001f, 0.0f, 0.0f));
    uint32_t v4 = mesh->addVertex(Vector3(1, 1, 0));
    mesh->addFace({v1, v4, v3});

    mesh->rebuildTopology();
    EXPECT_TRUE(mesh->validate());
    EXPECT_EQ(mesh->getVertices().size(), 5);

    MeshOperators::weldVerticesByDistance(*mesh, 0.01f);
    EXPECT_TRUE(mesh->validate());
    // Duplicate vertex merged -> 4 vertices remain
    EXPECT_EQ(mesh->getVertices().size(), 4);
}

// 8. Multi-cut Subdivide on Selected Child Faces Only
TEST_F(GeometryTests, MultiCutSubdivideTargeting) {
    auto cube = MeshPrimitives::createCube(Vector3(2.0f, 2.0f, 2.0f));
    ASSERT_NE(cube, nullptr);

    // 1 face subdivided with 2 cuts -> 1 face becomes 4, then 16.
    // Other 5 faces remain unchanged.
    // Total faces: 5 + 16 = 21.
    MeshOperators::subdivideFaces(*cube, {0}, 2);
    EXPECT_TRUE(cube->validate());
    EXPECT_EQ(cube->getFaces().size(), 21);
}

// 9. Loop Cut Topology Test
TEST_F(GeometryTests, LoopCutTopologySplit) {
    auto cube = MeshPrimitives::createCube(Vector3(1.0f, 1.0f, 1.0f));
    ASSERT_NE(cube, nullptr);

    // Initial 6 quads -> Loop cut across ring adds 4 new quads -> 10 faces
    size_t initialFaces = cube->getFaces().size();
    auto loopEdges = MeshCutOperators::findEdgeLoop(*cube, 0);
    EXPECT_FALSE(loopEdges.empty());
    auto newEdges = MeshCutOperators::applyLoopCut(*cube, loopEdges, 0.0f, 1);
    EXPECT_TRUE(cube->validate());
    EXPECT_FALSE(newEdges.empty());
    for (uint32_t edgeIdx : newEdges) {
        ASSERT_LT(edgeIdx, cube->getEdges().size());
        EXPECT_FALSE(cube->getEdges()[edgeIdx].deleted);
    }
    EXPECT_EQ(cube->getFaces().size(), initialFaces + 4);
}

// 10. Flip Normals Test
TEST_F(GeometryTests, FlipNormalsWinding) {
    auto mesh = std::make_shared<EditableMesh>();
    uint32_t v0 = mesh->addVertex(Vector3(0, 0, 0));
    uint32_t v1 = mesh->addVertex(Vector3(1, 0, 0));
    uint32_t v2 = mesh->addVertex(Vector3(0, 1, 0));
    int f = mesh->addFace({v0, v1, v2});
    mesh->rebuildTopology();
    mesh->calculateFaceNormal(f);
    EXPECT_GT(mesh->getFaces()[f].normal.z, 0.9f); // +Z

    MeshOperators::flipNormals(*mesh, {static_cast<uint32_t>(f)});
    EXPECT_TRUE(mesh->validate());
    EXPECT_LT(mesh->getFaces()[f].normal.z, -0.9f); // Inverted to -Z
}

// 11. Undo / Redo Selection State Restoration Test
TEST_F(GeometryTests, UndoRedoSelectionRestoration) {
    UndoStack::instance().clear();
    auto part = std::make_shared<Part>();
    auto cubeBefore = MeshPrimitives::createCube(Vector3(1.0f, 1.0f, 1.0f));
    auto cubeAfter = MeshPrimitives::createCube(Vector3(2.0f, 2.0f, 2.0f));
    part->setEditableMesh(cubeBefore->clone());

    std::vector<uint32_t> beforeSel = {0, 2};
    std::vector<uint32_t> afterSel = {1, 3, 5};

    auto& ctx = Editor::Modeling::ModelingContext::instance();
    ctx.clearSelection();
    ctx.selectedFaces = beforeSel;
    EXPECT_EQ(ctx.selectedFaces.size(), 2);

    auto cmd = std::make_unique<MeshTopologyCommand>(
        part, cubeBefore, cubeAfter,
        std::vector<uint32_t>{}, std::vector<uint32_t>{}, beforeSel,
        std::vector<uint32_t>{}, std::vector<uint32_t>{}, afterSel
    );

    // Apply command
    cmd->execute();
    EXPECT_EQ(ctx.selectedFaces.size(), 3);
    EXPECT_EQ(ctx.selectedFaces[0], 1);
    EXPECT_EQ(ctx.selectedFaces[1], 3);
    EXPECT_EQ(ctx.selectedFaces[2], 5);
    EXPECT_NEAR(part->getEditableMesh()->getVertices()[1].position.x, 2.0f, 0.01f);

    // Undo command
    cmd->undo();
    EXPECT_EQ(ctx.selectedFaces.size(), 2);
    EXPECT_EQ(ctx.selectedFaces[0], 0);
    EXPECT_EQ(ctx.selectedFaces[1], 2);
    EXPECT_NEAR(part->getEditableMesh()->getVertices()[1].position.x, 1.0f, 0.01f);

    // Push into UndoStack and test full Undo/Redo stack dispatch
    UndoStack::instance().push(std::move(cmd));
    UndoStack::instance().undo();
    EXPECT_EQ(ctx.selectedFaces.size(), 2);
    EXPECT_EQ(ctx.selectedFaces[0], 0);
    EXPECT_EQ(ctx.selectedFaces[1], 2);

    UndoStack::instance().redo();
    EXPECT_EQ(ctx.selectedFaces.size(), 3);
    EXPECT_EQ(ctx.selectedFaces[0], 1);
    EXPECT_EQ(ctx.selectedFaces[1], 3);
    EXPECT_EQ(ctx.selectedFaces[2], 5);
}

// 11. Knife: boundary-to-boundary split must produce two valid polygons.
TEST_F(GeometryTests, KnifeBoundarySplit) {
    auto mesh = std::make_shared<EditableMesh>();
    uint32_t v0 = mesh->addVertex(Vector3(0, 0, 0));
    uint32_t v1 = mesh->addVertex(Vector3(2, 0, 0));
    uint32_t v2 = mesh->addVertex(Vector3(2, 2, 0));
    uint32_t v3 = mesh->addVertex(Vector3(0, 2, 0));
    int face = mesh->addFace({v0, v1, v2, v3});
    mesh->rebuildTopology();

    EXPECT_TRUE(MeshCutOperators::cutFaceWithRaySegment(
        *mesh, static_cast<uint32_t>(face), Vector3(0, 1, 0), Vector3(2, 1, 0)));
    EXPECT_TRUE(mesh->validate());
    EXPECT_EQ(mesh->getFaces().size(), 2);
    for (const auto& f : mesh->getFaces()) EXPECT_EQ(f.vertices.size(), 4);
}

// 12. Convex Boolean CSG must produce valid union/difference/intersection shells.
TEST_F(GeometryTests, BooleanSafetyContract) {
    auto a = MeshPrimitives::createCube(Vector3(1, 1, 1));
    auto b = MeshPrimitives::createCube(Vector3(1, 1, 1));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    for (auto& v : b->getVertices()) v.position.x += 0.5f;
    b->rebuildTopology();
    auto unionMesh = MeshCutOperators::applyBoolean(*a, *b, BooleanOperation::Union);
    auto differenceMesh = MeshCutOperators::applyBoolean(*a, *b, BooleanOperation::Difference);
    auto intersectionMesh = MeshCutOperators::applyBoolean(*a, *b, BooleanOperation::Intersect);
    ASSERT_NE(unionMesh, nullptr);
    ASSERT_NE(differenceMesh, nullptr);
    ASSERT_NE(intersectionMesh, nullptr);
    EXPECT_TRUE(unionMesh->validate());
    EXPECT_TRUE(differenceMesh->validate());
    EXPECT_TRUE(intersectionMesh->validate());
    EXPECT_GT(unionMesh->getFaces().size(), 0u);
    EXPECT_GT(differenceMesh->getFaces().size(), 0u);
    EXPECT_GT(intersectionMesh->getFaces().size(), 0u);
}

// 13. Dissolve an edge while preserving the merged face cycle.
TEST_F(GeometryTests, DissolveEdgePreservesFaceLoop) {
    auto cube = MeshPrimitives::createCube(Vector3(1, 1, 1));
    ASSERT_NE(cube, nullptr);
    const int edge = cube->findEdge(4, 5);
    ASSERT_GE(edge, 0);

    MeshOperators::dissolveEdges(*cube, {static_cast<uint32_t>(edge)});
    EXPECT_TRUE(cube->validate());
    size_t activeFaces = 0;
    size_t maxFaceVerts = 0;
    for (const auto& face : cube->getFaces()) {
        if (face.deleted) continue;
        ++activeFaces;
        maxFaceVerts = std::max(maxFaceVerts, face.vertices.size());
    }
    EXPECT_EQ(activeFaces, 5);
    EXPECT_GE(maxFaceVerts, 6u);
}

// 14. Bevel single edge on closed cube: must remain fully closed manifold with corner caps
TEST_F(GeometryTests, BevelEdgesManifoldIntegrity) {
    auto cube = MeshPrimitives::createCube(Vector3(2, 2, 2));
    ASSERT_NE(cube, nullptr);
    EXPECT_TRUE(cube->validate());

    // Find a valid edge on the cube (e.g. edge between v0 and v1)
    int eIdx = cube->findEdge(0, 1);
    ASSERT_GE(eIdx, 0);

    MeshOperators::bevelEdges(*cube, {static_cast<uint32_t>(eIdx)}, 0.2f, 1, 0.5f);
    EXPECT_TRUE(cube->validate());

    // Should have created chamfer quad + corner cap triangles without boundary tears
    EXPECT_GT(cube->getFaces().size(), 6u);
    for (const auto& f : cube->getFaces()) {
        if (!f.deleted) {
            EXPECT_GE(f.vertices.size(), 3u);
        }
    }
    size_t boundaryEdges = 0;
    for (size_t e = 0; e < cube->getEdges().size(); ++e) {
        if (!cube->getEdges()[e].deleted && cube->getEdgeFaces(static_cast<uint32_t>(e)).size() == 1) {
            ++boundaryEdges;
        }
    }
    EXPECT_EQ(boundaryEdges, 0u);
}

// 15. Dual-Mode Inset: Region Inset vs Individual Inset
TEST_F(GeometryTests, RegionInsetVsIndividualInset) {
    // Test Individual Inset: 2 adjacent faces on a cube get individual quad rims
    auto cubeIndiv = MeshPrimitives::createCube(Vector3(2, 2, 2));
    ASSERT_NE(cubeIndiv, nullptr);
    auto insetsIndiv = MeshOperators::insetFaces(*cubeIndiv, {0, 2}, 0.2f, 0.0f, true);
    EXPECT_TRUE(cubeIndiv->validate());
    EXPECT_EQ(insetsIndiv.size(), 2u);
    // 6 original + 4 quad rims per face * 2 = 14 total faces
    EXPECT_EQ(cubeIndiv->getFaces().size(), 14u);

    // Test Region Inset: 2 adjacent faces on a cube share internal edge without internal dividing wall
    auto cubeRegion = MeshPrimitives::createCube(Vector3(2, 2, 2));
    ASSERT_NE(cubeRegion, nullptr);
    auto insetsRegion = MeshOperators::insetFaces(*cubeRegion, {0, 2}, 0.2f, 0.0f, false);
    EXPECT_TRUE(cubeRegion->validate());
    EXPECT_EQ(insetsRegion.size(), 2u);
    // Region boundary has 6 edges -> creates exactly 6 rim quads -> 6 + 6 = 12 faces
    EXPECT_EQ(cubeRegion->getFaces().size(), 12u);
    size_t regionBoundaryEdges = 0;
    for (size_t e = 0; e < cubeRegion->getEdges().size(); ++e) {
        if (!cubeRegion->getEdges()[e].deleted && cubeRegion->getEdgeFaces(static_cast<uint32_t>(e)).size() == 1) {
            ++regionBoundaryEdges;
        }
    }
    EXPECT_EQ(regionBoundaryEdges, 0u);
}

// 16. Extrude connected edge chain: must produce continuous quad strip sharing vertices
TEST_F(GeometryTests, ExtrudeEdgesConnectedPolyline) {
    auto mesh = std::make_shared<EditableMesh>();
    uint32_t v0 = mesh->addVertex(Vector3(0, 0, 0));
    uint32_t v1 = mesh->addVertex(Vector3(1, 0, 0));
    uint32_t v2 = mesh->addVertex(Vector3(1, 1, 0));
    uint32_t v3 = mesh->addVertex(Vector3(0, 1, 0));
    mesh->addFace({v0, v1, v2, v3});
    mesh->rebuildTopology();

    int e0 = mesh->findEdge(v0, v1);
    int e1 = mesh->findEdge(v1, v2);
    ASSERT_GE(e0, 0);
    ASSERT_GE(e1, 0);

    auto newFaces = MeshOperators::extrudeEdges(*mesh, {static_cast<uint32_t>(e0), static_cast<uint32_t>(e1)}, 1.0f, Vector3(0, 0, 1));
    EXPECT_TRUE(mesh->validate());
    EXPECT_EQ(newFaces.size(), 2u);

    // 4 initial + 3 unique extruded = 7 total vertices (shared corner vertex v1 extruded only once)
    EXPECT_EQ(mesh->getVertices().size(), 7u);
}

// 17. Multi-segment Knife Polyline Cut across faces
TEST_F(GeometryTests, KnifeMultiSegmentPolylineCut) {
    auto cube = MeshPrimitives::createCube(Vector3(1, 1, 1));
    ASSERT_NE(cube, nullptr);

    // Cut straight across face 0 (+Z face with X from -1 to 1, Y from -1 to 1, Z = 1)
    std::vector<Vector3> knifePath = {
        Vector3(-1.0f, 0.0f, 1.0f),
        Vector3(0.0f, 1.0f, 1.0f),
        Vector3(1.0f, 0.0f, 1.0f)
    };

    bool cutSuccess = MeshCutOperators::cutMeshWithKnifePolyline(*cube, knifePath, {0}, false);
    EXPECT_TRUE(cutSuccess);
    EXPECT_TRUE(cube->validate());
    // Face was split by the knife cut
    EXPECT_GE(cube->getFaces().size(), 8u);
}

// 18. Operator Panel Parameter Sync with Undo/Redo
TEST_F(GeometryTests, OperatorPanelUndoRedoSync) {
    UndoStack::instance().clear();
    auto& ctx = Editor::Modeling::ModelingContext::instance();
    ctx.clearSelection();

    auto part = std::make_shared<Part>();
    auto baseMesh = MeshPrimitives::createCube(Vector3(1, 1, 1));
    part->setEditableMesh(baseMesh);

    // Simulate Subdivide with 1 cut (each of 6 quads split into 4 -> 24 faces)
    ctx.executeSubdivide(part, 1, 0.0f);
    EXPECT_EQ(part->getEditableMesh()->getFaces().size(), 24u);

    // User tweaks panel parameter: cuts = 2 (each quad split into 4x4 -> 96 faces)
    ctx.opCuts = 2;
    ctx.reapplyLastOperation();
    EXPECT_EQ(part->getEditableMesh()->getFaces().size(), 96u);
    EXPECT_FALSE(ctx.selectedFaces.empty());
    for (uint32_t f : ctx.selectedFaces) {
        ASSERT_LT(f, part->getEditableMesh()->getFaces().size());
        EXPECT_FALSE(part->getEditableMesh()->getFaces()[f].deleted);
    }

    // Undo should restore initial 6-face cube
    UndoStack::instance().undo();
    EXPECT_EQ(part->getEditableMesh()->getFaces().size(), 6u);

    // Redo should restore the tweaked 96-face mesh
    UndoStack::instance().redo();
    EXPECT_EQ(part->getEditableMesh()->getFaces().size(), 96u);
}

// 19. UV and MaterialId Preservation on Operations
TEST_F(GeometryTests, UVAndMaterialPreservation) {
    auto cube = MeshPrimitives::createCube(Vector3(2, 2, 2));
    ASSERT_NE(cube, nullptr);
    // Assign material ID 42 to face 0
    cube->getFaces()[0].materialId = 42;

    // Loop Cut on the front bottom edge
    const int edge = cube->findEdge(4, 5);
    ASSERT_GE(edge, 0);
    auto newEdges = MeshCutOperators::applyLoopCut(*cube, {static_cast<uint32_t>(edge)}, 0.0f, 1);
    EXPECT_TRUE(cube->validate());
    ASSERT_FALSE(newEdges.empty());

    // Check that split faces retained materialId 42
    bool foundMat = false;
    for (const auto& f : cube->getFaces()) {
        if (!f.deleted && f.materialId == 42) foundMat = true;
    }
    EXPECT_TRUE(foundMat);

    bool foundInterpolatedUV = false;
    for (const auto& v : cube->getVertices()) {
        if (std::abs(v.position.x) < 0.01f &&
            std::abs(v.position.y + 2.0f) < 0.01f &&
            std::abs(v.position.z - 2.0f) < 0.01f &&
            std::abs(v.u - 0.5f) < 0.01f) {
            foundInterpolatedUV = true;
        }
    }
    EXPECT_TRUE(foundInterpolatedUV);
}
