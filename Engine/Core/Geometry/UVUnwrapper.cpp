#include "UVUnwrapper.h"
#include <cmath>
#include <algorithm>
#include <map>
#include <set>

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
    auto& faces = mesh.getFaces();
    auto& vertices = mesh.getVertices();
    for (uint32_t i = 0; i < faces.size(); ++i) {
        if (!faces[i].deleted) mesh.calculateFaceNormal(i);
    }
    margin = std::clamp(margin, 0.0f, 0.49f);
    angleThresholdDeg = std::clamp(angleThresholdDeg, 0.0f, 180.0f);
    const float cosThreshold = std::cos(angleThresholdDeg * 3.14159265358979323846f / 180.0f);

    std::vector<std::vector<uint32_t>> adjacency(faces.size());
    const auto& edges = mesh.getEdges();
    for (uint32_t e = 0; e < edges.size(); ++e) {
        if (edges[e].deleted) continue;
        const auto edgeFaces = mesh.getEdgeFaces(e);
        if (edgeFaces.size() != 2) continue;
        const uint32_t a = edgeFaces[0], b = edgeFaces[1];
        if (a >= faces.size() || b >= faces.size() || faces[a].deleted || faces[b].deleted) continue;
        const float dot = faces[a].normal.dot(faces[b].normal);
        if (dot >= cosThreshold) {
            adjacency[a].push_back(b);
            adjacency[b].push_back(a);
        }
    }

    std::vector<uint8_t> visited(faces.size(), 0);
    std::vector<std::vector<uint32_t>> islands;
    for (uint32_t seed = 0; seed < faces.size(); ++seed) {
        if (faces[seed].deleted || visited[seed]) continue;
        std::vector<uint32_t> island;
        std::vector<uint32_t> stack{seed};
        visited[seed] = 1;
        while (!stack.empty()) {
            uint32_t f = stack.back(); stack.pop_back();
            island.push_back(f);
            for (uint32_t n : adjacency[f]) {
                if (!visited[n]) { visited[n] = 1; stack.push_back(n); }
            }
        }
        islands.push_back(std::move(island));
    }

    if (islands.empty()) return;

    // Normalize and pack each chart independently. This avoids the old global
    // normalization which collapsed every hard-surface chart into one overlap.
    const float usable = std::max(0.001f, 1.0f - 2.0f * margin);
    const float cell = std::max(0.001f, usable / std::ceil(std::sqrt(static_cast<float>(islands.size()))));
    float cursorX = margin, cursorY = margin, rowHeight = 0.0f;
    for (const auto& island : islands) {
        // Build a common tangent frame for this smooth chart. Unlike the old
        // global box projection, this is a genuine chart parameterization:
        // connected faces share a continuous 2D basis and hard-angle seams
        // remain separate islands.
        Engine::Math::Vector3 islandNormal(0.0f, 0.0f, 0.0f);
        for (uint32_t fIdx : island) islandNormal += faces[fIdx].normal;
        islandNormal = islandNormal.normalized();
        Engine::Math::Vector3 helper = std::abs(islandNormal.y) < 0.9f
            ? Engine::Math::Vector3(0, 1, 0) : Engine::Math::Vector3(1, 0, 0);
        Engine::Math::Vector3 tangent = helper.cross(islandNormal).normalized();
        Engine::Math::Vector3 bitangent = islandNormal.cross(tangent).normalized();
        for (uint32_t fIdx : island) {
            auto& face = faces[fIdx];
            face.uvs.resize(face.vertices.size());
            for (size_t i = 0; i < face.vertices.size(); ++i) {
                uint32_t vIdx = face.vertices[i];
                if (vIdx >= vertices.size() || vertices[vIdx].deleted) continue;
                const auto& p = vertices[vIdx].position;
                face.uvs[i] = {p.dot(tangent), p.dot(bitangent)};
            }
        }

        // Harmonic (Tutte) relaxation with a fixed boundary gives curved
        // charts a real surface parameterization instead of merely projecting
        // every face independently. Boundary loops are placed on a circle;
        // interior vertices are solved by iterative cotangent-free Laplacian
        // relaxation, which is stable for the editor's low-poly meshes.
        std::set<uint32_t> islandVerts, boundaryVerts;
        std::map<uint32_t, std::vector<uint32_t>> boundaryGraph, graph;
        std::set<uint32_t> islandSet(island.begin(), island.end());
        for (uint32_t fIdx : island) {
            const auto& face = faces[fIdx];
            for (uint32_t v : face.vertices) islandVerts.insert(v);
            for (uint32_t e : face.edges) {
                auto ef = mesh.getEdgeFaces(e);
                size_t inside = 0; for (uint32_t n : ef) if (islandSet.count(n)) ++inside;
                if (inside != 1 || face.vertices.empty()) continue;
                const auto& edge = mesh.getEdges()[e];
                boundaryVerts.insert(edge.v0); boundaryVerts.insert(edge.v1);
                boundaryGraph[edge.v0].push_back(edge.v1);
                boundaryGraph[edge.v1].push_back(edge.v0);
            }
            for (size_t i = 0; i < face.vertices.size(); ++i) {
                uint32_t a = face.vertices[i], b = face.vertices[(i + 1) % face.vertices.size()];
                graph[a].push_back(b); graph[b].push_back(a);
            }
        }
        if (boundaryVerts.size() >= 3) {
            std::vector<uint32_t> loop;
            uint32_t start = *boundaryVerts.begin(), prev = UINT32_MAX, cur = start;
            for (size_t guard = 0; guard <= boundaryVerts.size() + 1; ++guard) {
                loop.push_back(cur);
                uint32_t next = UINT32_MAX;
                for (uint32_t candidate : boundaryGraph[cur]) if (candidate != prev) { next = candidate; break; }
                if (next == UINT32_MAX || next == start) break;
                prev = cur; cur = next;
            }
            if (loop.size() >= 3) {
                float perimeter = 0.0f;
                for (size_t i = 0; i < loop.size(); ++i) perimeter += (vertices[loop[i]].position - vertices[loop[(i + 1) % loop.size()]].position).length();
                float travelled = 0.0f;
                std::map<uint32_t, std::pair<float, float>> solved;
                for (size_t i = 0; i < loop.size(); ++i) {
                    float t = perimeter > 1e-5f ? travelled / perimeter : static_cast<float>(i) / loop.size();
                    solved[loop[i]] = {std::cos(t * 6.28318530718f), std::sin(t * 6.28318530718f)};
                    travelled += (vertices[loop[i]].position - vertices[loop[(i + 1) % loop.size()]].position).length();
                }
                for (uint32_t v : islandVerts) if (!solved.count(v)) solved[v] = {vertices[v].position.dot(tangent), vertices[v].position.dot(bitangent)};
                for (int iteration = 0; iteration < 64; ++iteration) {
                    auto nextSolved = solved;
                    for (uint32_t v : islandVerts) if (!boundaryVerts.count(v)) {
                        auto it = graph.find(v); if (it == graph.end() || it->second.empty()) continue;
                        std::pair<float, float> avg{0.0f, 0.0f};
                        for (uint32_t n : it->second) { avg.first += solved[n].first; avg.second += solved[n].second; }
                        nextSolved[v] = {avg.first / it->second.size(), avg.second / it->second.size()};
                    }
                    solved.swap(nextSolved);
                }
                for (uint32_t fIdx : island) for (size_t i = 0; i < faces[fIdx].vertices.size(); ++i) faces[fIdx].uvs[i] = solved[faces[fIdx].vertices[i]];
            }
        }
        float minU = 1e30f, maxU = -1e30f, minV = 1e30f, maxV = -1e30f;
        for (uint32_t fIdx : island) {
            for (const auto& uv : faces[fIdx].uvs) {
                minU = std::min(minU, uv.first); maxU = std::max(maxU, uv.first);
                minV = std::min(minV, uv.second); maxV = std::max(maxV, uv.second);
            }
        }
        const float rangeU = std::max(0.001f, maxU - minU);
        const float rangeV = std::max(0.001f, maxV - minV);
        const float chartScale = std::min((cell * 0.96f) / rangeU, (cell * 0.96f) / rangeV);
        const float chartW = rangeU * chartScale, chartH = rangeV * chartScale;
        if (cursorX + chartW > 1.0f - margin) {
            cursorX = margin; cursorY += rowHeight + margin; rowHeight = 0.0f;
        }
        if (cursorY + chartH > 1.0f - margin) {
            // More charts than the grid can hold: keep them inside the tile and
            // let the final row use the remaining space rather than emit NaNs.
            cursorY = margin;
        }
        for (uint32_t fIdx : island) {
            for (auto& uv : faces[fIdx].uvs) {
                uv.first = cursorX + (uv.first - minU) * chartScale;
                uv.second = cursorY + (uv.second - minV) * chartScale;
            }
        }
        cursorX += chartW + margin;
        rowHeight = std::max(rowHeight, chartH);
    }
}

} // namespace Engine::Geometry
