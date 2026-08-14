#pragma once
#include <imgui.h>
#include <string>

enum class EditorTool { Select, Move, Rotate, Scale };

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

    bool wantStartMovingAssetBrowser = false;
    bool wantStartMovingMaterialEditor = false;

    void savePreset(const char* name);
    void loadPreset(const char* name);
};
