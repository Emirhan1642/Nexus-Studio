#include "MeshPrimitives.h"
#include <cmath>

namespace Engine::Geometry {

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 6.28318530717958647692f;

std::shared_ptr<EditableMesh> MeshPrimitives::createCube(const Engine::Math::Vector3& halfExtents) {
    auto mesh = std::make_shared<EditableMesh>();
    float hx = halfExtents.x;
    float hy = halfExtents.y;
    float hz = halfExtents.z;

    // 8 Vertices
    uint32_t v0 = mesh->addVertex({-hx, -hy, -hz}, 0, 0); // 0: Bottom-Left Back
    uint32_t v1 = mesh->addVertex({ hx, -hy, -hz}, 1, 0); // 1: Bottom-Right Back
    uint32_t v2 = mesh->addVertex({ hx,  hy, -hz}, 1, 1); // 2: Top-Right Back
    uint32_t v3 = mesh->addVertex({-hx,  hy, -hz}, 0, 1); // 3: Top-Left Back
    uint32_t v4 = mesh->addVertex({-hx, -hy,  hz}, 0, 0); // 4: Bottom-Left Front
    uint32_t v5 = mesh->addVertex({ hx, -hy,  hz}, 1, 0); // 5: Bottom-Right Front
    uint32_t v6 = mesh->addVertex({ hx,  hy,  hz}, 1, 1); // 6: Top-Right Front
    uint32_t v7 = mesh->addVertex({-hx,  hy,  hz}, 0, 1); // 7: Top-Left Front

    // 6 Quad Faces (CCW winding)
    mesh->addFace({v4, v5, v6, v7}); // Front (+Z)
    mesh->addFace({v1, v0, v3, v2}); // Back (-Z)
    mesh->addFace({v7, v6, v2, v3}); // Top (+Y)
    mesh->addFace({v0, v1, v5, v4}); // Bottom (-Y)
    mesh->addFace({v5, v1, v2, v6}); // Right (+X)
    mesh->addFace({v0, v4, v7, v3}); // Left (-X)

    mesh->rebuildTopology();
    mesh->recalculateAllNormals(false);
    return mesh;
}

std::shared_ptr<EditableMesh> MeshPrimitives::createPlane(float width, float height, int subX, int subY) {
    auto mesh = std::make_shared<EditableMesh>();
    subX = std::max(1, subX);
    subY = std::max(1, subY);

    float halfW = width * 0.5f;
    float halfH = height * 0.5f;

    std::vector<std::vector<uint32_t>> grid(subY + 1, std::vector<uint32_t>(subX + 1));

    for (int y = 0; y <= subY; ++y) {
        float v = static_cast<float>(y) / static_cast<float>(subY);
        float pz = -halfH + v * height;
        for (int x = 0; x <= subX; ++x) {
            float u = static_cast<float>(x) / static_cast<float>(subX);
            float px = -halfW + u * width;
            grid[y][x] = mesh->addVertex({px, 0.0f, pz}, u, v, {0, 1, 0});
        }
    }

    for (int y = 0; y < subY; ++y) {
        for (int x = 0; x < subX; ++x) {
            uint32_t v0 = grid[y][x];
            uint32_t v1 = grid[y][x + 1];
            uint32_t v2 = grid[y + 1][x + 1];
            uint32_t v3 = grid[y + 1][x];
            mesh->addFace({v0, v1, v2, v3});
        }
    }

    mesh->rebuildTopology();
    mesh->recalculateAllNormals(false);
    return mesh;
}

std::shared_ptr<EditableMesh> MeshPrimitives::createCylinder(float radius, float height, int segments) {
    auto mesh = std::make_shared<EditableMesh>();
    segments = std::max(3, segments);
    float halfH = height * 0.5f;

    std::vector<uint32_t> topVerts(segments);
    std::vector<uint32_t> botVerts(segments);

    for (int i = 0; i < segments; ++i) {
        float theta = (static_cast<float>(i) / static_cast<float>(segments)) * TWO_PI;
        float x = radius * std::cos(theta);
        float z = radius * std::sin(theta);
        float u = static_cast<float>(i) / static_cast<float>(segments);

        topVerts[i] = mesh->addVertex({x,  halfH, z}, u, 1.0f);
        botVerts[i] = mesh->addVertex({x, -halfH, z}, u, 0.0f);
    }

    // Side Quads
    for (int i = 0; i < segments; ++i) {
        int next = (i + 1) % segments;
        mesh->addFace({botVerts[i], botVerts[next], topVerts[next], topVerts[i]});
    }

    // Top Cap N-Gon (in reverse order for outward normal)
    std::vector<uint32_t> topCap(segments);
    for (int i = 0; i < segments; ++i) topCap[i] = topVerts[segments - 1 - i];
    mesh->addFace(topCap);

    // Bottom Cap N-Gon
    mesh->addFace(botVerts);

    mesh->rebuildTopology();
    mesh->recalculateAllNormals(false);
    return mesh;
}

std::shared_ptr<EditableMesh> MeshPrimitives::createSphere(float radius, int rings, int sectors) {
    auto mesh = std::make_shared<EditableMesh>();
    rings = std::max(2, rings);
    sectors = std::max(3, sectors);

    uint32_t topPole = mesh->addVertex({0, radius, 0}, 0.5f, 1.0f);
    uint32_t botPole = mesh->addVertex({0, -radius, 0}, 0.5f, 0.0f);

    std::vector<std::vector<uint32_t>> ringVerts(rings - 1, std::vector<uint32_t>(sectors));

    for (int r = 0; r < rings - 1; ++r) {
        float phi = PI * static_cast<float>(r + 1) / static_cast<float>(rings);
        float y = radius * std::cos(phi);
        float ringR = radius * std::sin(phi);
        float v = 1.0f - static_cast<float>(r + 1) / static_cast<float>(rings);

        for (int s = 0; s < sectors; ++s) {
            float theta = TWO_PI * static_cast<float>(s) / static_cast<float>(sectors);
            float x = ringR * std::cos(theta);
            float z = ringR * std::sin(theta);
            float u = static_cast<float>(s) / static_cast<float>(sectors);
            ringVerts[r][s] = mesh->addVertex({x, y, z}, u, v);
        }
    }

    // Top Triangles
    for (int s = 0; s < sectors; ++s) {
        int next = (s + 1) % sectors;
        mesh->addFace({topPole, ringVerts[0][s], ringVerts[0][next]});
    }

    // Middle Quads
    for (int r = 0; r < rings - 2; ++r) {
        for (int s = 0; s < sectors; ++s) {
            int next = (s + 1) % sectors;
            mesh->addFace({ringVerts[r][s], ringVerts[r + 1][s], ringVerts[r + 1][next], ringVerts[r][next]});
        }
    }

    // Bottom Triangles
    for (int s = 0; s < sectors; ++s) {
        int next = (s + 1) % sectors;
        mesh->addFace({ringVerts[rings - 2][s], botPole, ringVerts[rings - 2][next]});
    }

    mesh->rebuildTopology();
    mesh->recalculateAllNormals(true);
    return mesh;
}

std::shared_ptr<EditableMesh> MeshPrimitives::createCone(float radius, float height, int segments) {
    auto mesh = std::make_shared<EditableMesh>();
    segments = std::max(3, segments);
    float halfH = height * 0.5f;

    uint32_t apex = mesh->addVertex({0, halfH, 0}, 0.5f, 1.0f);
    std::vector<uint32_t> botVerts(segments);

    for (int i = 0; i < segments; ++i) {
        float theta = (static_cast<float>(i) / static_cast<float>(segments)) * TWO_PI;
        float x = radius * std::cos(theta);
        float z = radius * std::sin(theta);
        float u = static_cast<float>(i) / static_cast<float>(segments);
        botVerts[i] = mesh->addVertex({x, -halfH, z}, u, 0.0f);
    }

    // Side Triangles
    for (int i = 0; i < segments; ++i) {
        int next = (i + 1) % segments;
        mesh->addFace({botVerts[i], botVerts[next], apex});
    }

    // Bottom Cap
    mesh->addFace(botVerts);

    mesh->rebuildTopology();
    mesh->recalculateAllNormals(false);
    return mesh;
}

std::shared_ptr<EditableMesh> MeshPrimitives::createTorus(float majorR, float minorR, int majorSeg, int minorSeg) {
    auto mesh = std::make_shared<EditableMesh>();
    majorSeg = std::max(3, majorSeg);
    minorSeg = std::max(3, minorSeg);

    std::vector<std::vector<uint32_t>> verts(majorSeg, std::vector<uint32_t>(minorSeg));

    for (int i = 0; i < majorSeg; ++i) {
        float u = static_cast<float>(i) / static_cast<float>(majorSeg);
        float theta = u * TWO_PI;
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);

        for (int j = 0; j < minorSeg; ++j) {
            float v = static_cast<float>(j) / static_cast<float>(minorSeg);
            float phi = v * TWO_PI;
            float cosPhi = std::cos(phi);
            float sinPhi = std::sin(phi);

            float x = (majorR + minorR * cosPhi) * cosTheta;
            float y = minorR * sinPhi;
            float z = (majorR + minorR * cosPhi) * sinTheta;

            verts[i][j] = mesh->addVertex({x, y, z}, u, v);
        }
    }

    // Quads
    for (int i = 0; i < majorSeg; ++i) {
        int nextI = (i + 1) % majorSeg;
        for (int j = 0; j < minorSeg; ++j) {
            int nextJ = (j + 1) % minorSeg;
            mesh->addFace({verts[i][j], verts[nextI][j], verts[nextI][nextJ], verts[i][nextJ]});
        }
    }

    mesh->rebuildTopology();
    mesh->recalculateAllNormals(true);
    return mesh;
}

} // namespace Engine::Geometry
