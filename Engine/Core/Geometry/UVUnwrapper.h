#pragma once
#include "EditableMesh.h"

namespace Engine::Geometry {

class UVUnwrapper {
public:
    // Box / Triplanar Mapping (Standard for architecture / greyboxing)
    static void boxProject(EditableMesh& mesh, float scale = 1.0f);

    // Planar UV projection along a specified axis
    static void planarProject(EditableMesh& mesh, const Engine::Math::Vector3& axis = {0, 1, 0}, float scale = 1.0f);

    // Chart-based unwrap: splits by normal angle, parameterizes each chart in
    // a shared tangent frame, then packs charts into the 0..1 tile.
    static void smartUVProject(EditableMesh& mesh, float angleThresholdDeg = 66.0f, float margin = 0.02f);
};

} // namespace Engine::Geometry
