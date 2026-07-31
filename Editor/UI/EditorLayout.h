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

    void savePreset(const char* name);
    void loadPreset(const char* name);
};
