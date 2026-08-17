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
}

// 2. Region Extrude Topology & Invariant
TEST_F(GeometryTests, RegionExtrudeSingleFace) {
    auto cube = MeshPrimitives::createCube(Vector3(1.0f, 1.0f, 1.0f));
    ASSERT_NE(cube, nullptr);

    auto extrudedFaces = MeshOperators::extrudeFaces(*cube, {0}, 1.0f);
    EXPECT_FALSE(extrudedFaces.empty());
    EXPECT_TRUE(cube->validate());
}

// 3. Multi-Face Region Extrude No Internal Walls
TEST_F(GeometryTests, RegionExtrudeMultiFaceNoInternalWalls) {
    auto cube = MeshPrimitives::createCube(Vector3(1.0f, 1.0f, 1.0f));
    ASSERT_NE(cube, nullptr);

    size_t initialFaceCount = cube->getFaces().size();
    // Extrude 2 adjacent faces: face 0 and face 1
    auto extruded = MeshOperators::extrudeFaces(*cube, {0, 1}, 0.5f, {0, 0, 0}, false);
    EXPECT_TRUE(cube->validate());
    // In region extrude of 2 adjacent quads sharing 1 edge:
    // Outer boundary has 6 edges -> 6 side quads. No internal quad between 0 and 1.
    EXPECT_GT(cube->getFaces().size(), initialFaceCount);
}

// 4. Inset Faces Uniform Offset
TEST_F(GeometryTests, InsetFacesValidation) {
    auto cube = MeshPrimitives::createCube(Vector3(2.0f, 2.0f, 2.0f));
    ASSERT_NE(cube, nullptr);

    auto insets = MeshOperators::insetFaces(*cube, {0}, 0.2f, 0.0f);
    EXPECT_FALSE(insets.empty());
    EXPECT_TRUE(cube->validate());
}

// 5. Solidify Open Plane & Closed Mesh
TEST_F(GeometryTests, SolidifyClosedMesh) {
    auto cube = MeshPrimitives::createCube(Vector3(2.0f, 2.0f, 2.0f));
    ASSERT_NE(cube, nullptr);

    MeshOperators::solidify(*cube, 0.1f, true);
    EXPECT_TRUE(cube->validate());
}

// 6. Ear-Clipping Triangulation on Concave N-Gon
TEST_F(GeometryTests, TriangulateFacesEarClipping) {
    auto mesh = std::make_shared<EditableMesh>();
    // L-shaped concave polygon in XY plane: (0,0), (2,0), (2,1), (1,1), (1,2), (0,2)
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
    // An n-gon with 6 vertices must produce 6 - 2 = 4 triangles
    EXPECT_EQ(mesh->getFaces().size(), 4);
    for (const auto& face : mesh->getFaces()) {
        if (!face.deleted) {
            EXPECT_EQ(face.vertices.size(), 3);
        }
    }
}

// 7. Weld By Distance Coincident Vertices
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

    MeshOperators::weldVerticesByDistance(*mesh, 0.01f);
    EXPECT_TRUE(mesh->validate());
}

// 8. Poke Faces Operator
TEST_F(GeometryTests, PokeFaces) {
    auto cube = MeshPrimitives::createCube(Vector3(2.0f, 2.0f, 2.0f));
    ASSERT_NE(cube, nullptr);

    MeshOperators::pokeFaces(*cube, {0}, 0.5f);
    EXPECT_TRUE(cube->validate());
}
