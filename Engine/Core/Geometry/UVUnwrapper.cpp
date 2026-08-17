#include "UVUnwrapper.h"
#include <cmath>
#include <algorithm>

namespace Engine::Geometry {

void UVUnwrapper::boxProject(EditableMesh& mesh, float scale) {
    if (scale <= 0.0001f) scale = 1.0f;
    auto& faces = mesh.getFaces();
    auto& vertices = mesh.getVertices();

    for (size_t fIdx = 0; fIdx < faces.size(); ++fIdx) {
        if (faces[fIdx].deleted) continue;
        mesh.calculateFaceNormal(static_cast<uint32_t>(fIdx));
        const auto& norm = faces[fIdx].normal;

        float absX = std::abs(norm.x);
        float absY = std::abs(norm.y);
        float absZ = std::abs(norm.z);

        for (uint32_t vIdx : faces[fIdx].vertices) {
            if (vIdx >= vertices.size() || vertices[vIdx].deleted) continue;
            const auto& p = vertices[vIdx].position;

            if (absX >= absY && absX >= absZ) {
                // Project onto YZ plane (Side X)
                vertices[vIdx].u = (norm.x > 0 ? p.z : -p.z) * scale;
                vertices[vIdx].v = p.y * scale;
            } else if (absY >= absX && absY >= absZ) {
                // Project onto XZ plane (Top/Bottom Y)
                vertices[vIdx].u = p.x * scale;
                vertices[vIdx].v = (norm.y > 0 ? -p.z : p.z) * scale;
            } else {
                // Project onto XY plane (Front/Back Z)
                vertices[vIdx].u = (norm.z > 0 ? p.x : -p.x) * scale;
                vertices[vIdx].v = p.y * scale;
            }
        }
    }
}

void UVUnwrapper::planarProject(EditableMesh& mesh, const Engine::Math::Vector3& axis, float scale) {
    if (scale <= 0.0001f) scale = 1.0f;
    auto& vertices = mesh.getVertices();
    Engine::Math::Vector3 normAxis = axis.normalized();

    // Determine orthogonal basis
    Engine::Math::Vector3 up = (std::abs(normAxis.y) > 0.9f) ? Engine::Math::Vector3(0, 0, 1) : Engine::Math::Vector3(0, 1, 0);
    Engine::Math::Vector3 right = normAxis.cross(up).normalized();
    up = right.cross(normAxis).normalized();

    for (auto& v : vertices) {
        if (v.deleted) continue;
        v.u = v.position.dot(right) * scale;
        v.v = v.position.dot(up) * scale;
    }
}

void UVUnwrapper::smartUVProject(EditableMesh& mesh, float angleThresholdDeg, float margin) {
    // Smart project decomposes by dominant face angle and normalizes to [margin, 1-margin]
    boxProject(mesh, 1.0f);

    auto& vertices = mesh.getVertices();
    if (vertices.empty()) return;

    float minU = 1e9f, maxU = -1e9f;
    float minV = 1e9f, maxV = -1e9f;

    for (const auto& v : vertices) {
        if (v.deleted) continue;
        minU = std::min(minU, v.u);
        maxU = std::max(maxU, v.u);
        minV = std::min(minV, v.v);
        maxV = std::max(maxV, v.v);
    }

    float rangeU = std::max(0.001f, maxU - minU);
    float rangeV = std::max(0.001f, maxV - minV);
    float scale = (1.0f - margin * 2.0f);

    for (auto& v : vertices) {
        if (v.deleted) continue;
        v.u = margin + ((v.u - minU) / rangeU) * scale;
        v.v = margin + ((v.v - minV) / rangeV) * scale;
    }
}

} // namespace Engine::Geometry
