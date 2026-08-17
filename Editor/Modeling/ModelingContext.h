#pragma once
#include "Engine/Core/DataModel/Part.h"
#include "Engine/Core/Geometry/EditableMesh.h"
#include "Engine/Core/Geometry/MeshOperators.h"
#include "Engine/Core/Geometry/MeshCutOperators.h"
#include "Editor/Undo/UndoStack.h"
#include "Editor/Undo/MeshTopologyCommand.h"
#include <imgui.h>
#include <memory>
#include <vector>

namespace Editor::Modeling {

enum class ModalTool {
    None,
    Extrude,
    Inset,
    Bevel,
    LoopCut,
    Knife
};

enum class LastOpType {
    None,
    Extrude,
    Inset,
    Bevel,
    LoopCut,
    Subdivide,
    Merge
};

class ModelingContext {
public:
    static ModelingContext& instance() {
        static ModelingContext s_instance;
        return s_instance;
    }

    // Active Selection in Edit Mode
    std::vector<uint32_t> selectedVertices;
    std::vector<uint32_t> selectedEdges;
    std::vector<uint32_t> selectedFaces;
    std::weak_ptr<Part> activePart;

    // Modal Tool States
    ModalTool activeModal = ModalTool::None;
    ImVec2 modalStartMouse{0, 0};
    std::shared_ptr<Engine::Geometry::EditableMesh> preModalMesh;

    // Loop Cut Preview Edge Loop
    std::vector<uint32_t> previewLoopEdges;
    int hoveredEdgeForLoop = -1;

    // Knife Cut Points
    std::vector<Engine::Math::Vector3> knifePoints;
    std::vector<uint32_t> knifeTargetFaces;
    bool opCutThrough = false; // When false, cuts only clicked front faces; when true, cuts through whole model

    // Last Operation Parameters (For Floating Operator Panel)
    LastOpType lastOp = LastOpType::None;
    std::shared_ptr<Engine::Geometry::EditableMesh> baseSnapshotMesh;
    std::vector<uint32_t> opTargetVertices;
    std::vector<uint32_t> opTargetEdges;
    std::vector<uint32_t> opTargetFaces;

    float opDistance = 1.0f;
    float opThickness = 0.2f;
    float opDepth = 0.0f;
    float opWidth = 0.2f;
    int opSegments = 1;
    float opProfile = 0.5f;
    int opCuts = 1;
    float opSlide = 0.0f;
    float opSmoothness = 0.0f;

    // Actions & Modal triggers
    void clearSelection();
    void selectAll(const std::shared_ptr<Part>& part, int mode);

    bool startExtrude(std::shared_ptr<Part> part);
    bool startInset(std::shared_ptr<Part> part);
    bool startBevel(std::shared_ptr<Part> part);
    bool startLoopCut(std::shared_ptr<Part> part);
    bool startKnife(std::shared_ptr<Part> part);

    void updateModal(const ImVec2& currentMousePos, bool shiftHeld, bool ctrlHeld);
    void confirmModal();
    void cancelModal();

    void executeSubdivide(std::shared_ptr<Part> part, int cuts = 1, float smoothness = 0.0f);
    void executeMerge(std::shared_ptr<Part> part, Engine::Geometry::MergeMode mode);
    void executeDelete(std::shared_ptr<Part> part, Engine::Geometry::SubElementType type);
    void executeDissolve(std::shared_ptr<Part> part, Engine::Geometry::SubElementType type);
    void executeFill(std::shared_ptr<Part> part);
    void executeMirror(std::shared_ptr<Part> part, Engine::Geometry::MirrorAxis axis);
    void executeSmartUV(std::shared_ptr<Part> part);
    void executeBoxUV(std::shared_ptr<Part> part);

    void executePoke(std::shared_ptr<Part> part, float offset = 0.0f);
    void executeTriangulate(std::shared_ptr<Part> part);
    void executeTrisToQuads(std::shared_ptr<Part> part);
    void executeFlipNormals(std::shared_ptr<Part> part);
    void executeEdgeSplit(std::shared_ptr<Part> part);
    void executeWeldByDistance(std::shared_ptr<Part> part, float threshold = 0.001f);

    // Advanced Selection Actions
    void selectLinked(const std::shared_ptr<Part>& part, int mode);
    void selectMore(const std::shared_ptr<Part>& part, int mode);
    void selectLess(const std::shared_ptr<Part>& part, int mode);
    void selectInvert(const std::shared_ptr<Part>& part, int mode);
    void selectBoundaryLoop(const std::shared_ptr<Part>& part);

    void executeSolidify(std::shared_ptr<Part> part, float thickness = 0.2f, bool rimFill = true);
    void executeSeparate(std::shared_ptr<Part> part);
    void executeJoin(std::vector<std::shared_ptr<Instance>> selectedInstances);

    void reapplyLastOperation();

private:
    ModelingContext() = default;
};

} // namespace Editor::Modeling
