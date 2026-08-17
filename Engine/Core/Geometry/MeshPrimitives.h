#pragma once
#include "EditableMesh.h"
#include <memory>

namespace Engine::Geometry {

class MeshPrimitives {
public:
    static std::shared_ptr<EditableMesh> createCube(const Engine::Math::Vector3& halfExtents = {1.0f, 1.0f, 1.0f});
    static std::shared_ptr<EditableMesh> createPlane(float width = 2.0f, float height = 2.0f, int subX = 1, int subY = 1);
    static std::shared_ptr<EditableMesh> createCylinder(float radius = 1.0f, float height = 2.0f, int segments = 16);
    static std::shared_ptr<EditableMesh> createSphere(float radius = 1.0f, int rings = 12, int sectors = 24);
    static std::shared_ptr<EditableMesh> createCone(float radius = 1.0f, float height = 2.0f, int segments = 16);
    static std::shared_ptr<EditableMesh> createTorus(float majorR = 1.0f, float minorR = 0.3f, int majorSeg = 16, int minorSeg = 8);
};

} // namespace Engine::Geometry
