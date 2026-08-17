#pragma once
#include "Engine/Core/Geometry/EditableMesh.h"
#include <string>

namespace Engine::Asset {

class MeshAssetExporter {
public:
    static bool exportToObj(const Geometry::EditableMesh& mesh, const std::string& absoluteFilePath);
};

} // namespace Engine::Asset
