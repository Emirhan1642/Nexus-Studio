#include <gtest/gtest.h>
#include "Engine/Core/Geometry/EditableMesh.h"
#include "Engine/Core/Geometry/MeshPrimitives.h"
#include "Engine/Core/Geometry/MeshOperators.h"
#include "Engine/Core/Geometry/MeshCutOperators.h"

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
    EXPECT_FALSE(insets.empty());

    uint32_t capFaceIdx = insets[0];
    const auto& capFace = cube->getFaces()[capFaceIdx];
    // Inset cap on 2x2 cube with thickness 0.2 must have size (2 - 0.4) = 1.6
    float dx = std::abs(cube->getVertices()[capFace.vertices[0]].position.x - cube->getVertices()[capFace.vertices[1]].position.x);
    float dy = std::abs(cube->getVertices()[capFace.vertices[0]].position.y - cube->getVertices()[capFace.vertices[1]].position.y);
    float edgeLen = std::max(dx, dy);
    EXPECT_NEAR(edgeLen, 1.6f, 0.01f);
}

// 4. Solidify Open Sheet with Rim Walls
TEST_F(GeometryTests, SolidifyOpenPlaneWithRim) {
    auto mesh = std::make_shared<EditableMesh>();
    // Single Quad sheet in XY plane
    uint32_t v0 = mesh->addVertex(Vector3(0, 0, 0));
    uint32_t v1 = mesh->addVertex(Vector3(2, 0, 0));
    uint32_t v2 = mesh->addVertex(Vector3(2, 2, 0));
    uint32_t v3 = mesh->addVertex(Vector3(0, 2, 0));
    mesh->addFace({v0, v1, v2, v3});
    mesh->rebuildTopology();
    EXPECT_TRUE(mesh->validate());

    // Solidify: 1 front face + 1 back face + 4 rim quad walls = 6 faces (a watertight closed box)
    MeshOperators::solidify(*mesh, 0.5f, true);
    EXPECT_TRUE(mesh->validate());
    EXPECT_EQ(mesh->getFaces().size(), 6);
    EXPECT_EQ(mesh->getVertices().size(), 8);
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
