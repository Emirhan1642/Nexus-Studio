#pragma once
#include <imgui.h>
#include <string>

class EditorLayout {
public:
    static EditorLayout& instance();

    bool showLeftToolbar     = true;
    bool showMaterialEditor  = true;
    bool showAssetBrowser    = true;
    bool showAICopilot       = true;
    
    std::string activeBottomTab = "Asset Browser";
    bool isAssetBrowserTornOff = false;
    bool isMaterialEditorTornOff = false;
    bool isConsoleTornOff = false;
    
    bool justTornOffAssetBrowser = false;
    bool justTornOffMaterialEditor = false;
    
    // Flags for different drag sources
    bool wantStartMovingAssetBrowserFromTab = false;
    bool wantStartMovingMaterialEditorFromTab = false;
    
    bool wantStartMovingAssetBrowser = false;
    bool wantStartMovingMaterialEditor = false;
    ImVec2 tearOffPos = ImVec2(0,0);

    void savePreset(const char* name);
    void loadPreset(const char* name);
};
