#include "EditorLayout.h"
#include <imgui.h>
#include <filesystem>

namespace fs = std::filesystem;

EditorLayout& EditorLayout::instance() {
    static EditorLayout s_instance;
    return s_instance;
}

void EditorLayout::savePreset(const char* name) {
    if (!fs::exists("layouts")) {
        fs::create_directory("layouts");
    }
    std::string path = std::string("layouts/") + name + ".ini";
    ImGui::SaveIniSettingsToDisk(path.c_str());
}

void EditorLayout::loadPreset(const char* name) {
    std::string path = std::string("layouts/") + name + ".ini";
    if (fs::exists(path)) {
        ImGui::LoadIniSettingsFromDisk(path.c_str());
    }
}
