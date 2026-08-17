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

        auto& face = faces[fIdx];
        face.uvs.resize(face.vertices.size());

        for (size_t i = 0; i < face.vertices.size(); ++i) {
            uint32_t vIdx = face.vertices[i];
            if (vIdx >= vertices.size() || vertices[vIdx].deleted) continue;
            const auto& p = vertices[vIdx].position;

            float u = 0.0f, v = 0.0f;
            if (absX >= absY && absX >= absZ) {
                // Project onto YZ plane (Side X)
                u = (norm.x > 0 ? p.z : -p.z) * scale;
                v = p.y * scale;
            } else if (absY >= absX && absY >= absZ) {
                // Project onto XZ plane (Top/Bottom Y)
                u = p.x * scale;
                v = (norm.y > 0 ? -p.z : p.z) * scale;
            } else {
                // Project onto XY plane (Front/Back Z)
                u = (norm.z > 0 ? p.x : -p.x) * scale;
                v = p.y * scale;
            }

            face.uvs[i] = {u, v};
            vertices[vIdx].u = u;
            vertices[vIdx].v = v;
        }
    }
}

void UVUnwrapper::planarProject(EditableMesh& mesh, const Engine::Math::Vector3& axis, float scale) {
    if (scale <= 0.0001f) scale = 1.0f;
    auto& vertices = mesh.getVertices();
    auto& faces = mesh.getFaces();
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

    for (auto& face : faces) {
        if (face.deleted) continue;
        face.uvs.resize(face.vertices.size());
        for (size_t i = 0; i < face.vertices.size(); ++i) {
            uint32_t vIdx = face.vertices[i];
            if (vIdx < vertices.size()) {
                face.uvs[i] = {vertices[vIdx].u, vertices[vIdx].v};
            }
        }
    }
}

void UVUnwrapper::smartUVProject(EditableMesh& mesh, float angleThresholdDeg, float margin) {
    boxProject(mesh, 1.0f);

    auto& faces = mesh.getFaces();
    auto& vertices = mesh.getVertices();
    if (vertices.empty()) return;

    float minU = 1e9f, maxU = -1e9f;
    float minV = 1e9f, maxV = -1e9f;

    for (const auto& f : faces) {
        if (f.deleted) continue;
        for (const auto& uv : f.uvs) {
            minU = std::min(minU, uv.first);
            maxU = std::max(maxU, uv.first);
            minV = std::min(minV, uv.second);
            maxV = std::max(maxV, uv.second);
        }
    }

    float rangeU = std::max(0.001f, maxU - minU);
    float rangeV = std::max(0.001f, maxV - minV);
    float scale = (1.0f - margin * 2.0f);

    for (auto& f : faces) {
        if (f.deleted) continue;
        for (auto& uv : f.uvs) {
            uv.first = margin + ((uv.first - minU) / rangeU) * scale;
            uv.second = margin + ((uv.second - minV) / rangeV) * scale;
        }
    }
}

} // namespace Engine::Geometry
