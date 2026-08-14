#pragma once
#include <imgui.h>
#include <string>

#include "Engine/Core/Math/Vector3.h"

enum class EditorTool { Select, Move, Rotate, Scale };
enum class CameraViewMode { Free, Isometric, Degree90 };
enum class EditorShadingMode { Object, Face, Edge, Vertex };

class EditorLayout {
public:
    static EditorLayout& instance();

    bool showLeftToolbar     = true;
    bool showMaterialEditor  = true;
    bool showAssetBrowser    = true;
    bool showConsole         = true;
    bool showAICopilot       = true;
    bool showExplorer        = true;
    bool showProperties      = true;

    // Active transform tool and grid snapping
    EditorTool currentTool   = EditorTool::Move;
    bool gridSnap            = true;

    // Active camera view mode and shading mode
    CameraViewMode cameraMode = CameraViewMode::Free;
    int degree90Index         = 0; // 0=Front, 1=Right, 2=Back, 3=Left, 4=Top, 5=Bottom
    bool isOrthographic       = false;
    EditorShadingMode shadingMode = EditorShadingMode::Object;

    // 3D Cursor Position
    Engine::Math::Vector3 cursor3DPosition{0.0f, 0.0f, 0.0f};

    bool wantStartMovingAssetBrowser = false;
    bool wantStartMovingMaterialEditor = false;

    void savePreset(const char* name);
    void loadPreset(const char* name);
};
