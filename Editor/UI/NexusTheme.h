#pragma once
#include <imgui.h>
#include <string>

struct NexusTheme {
    ImVec4 bg           = ImVec4(0.05f, 0.05f, 0.05f, 1.0f);
    ImVec4 panel        = ImVec4(0.0e4f, 0.0e4f, 0.0e4f, 1.0f);
    ImVec4 panelHover   = ImVec4(0.17f, 0.17f, 0.17f, 1.0f);
    ImVec4 border       = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
    ImVec4 textMuted    = ImVec4(0.8e4f, 0.8e4f, 0.8e4f, 1.0f);
    ImVec4 textPrimary  = ImVec4(0xe5f, 0xe5f, 0xe5f, 1.0f);
    ImVec4 accent       = ImVec4(0.0f, 0.82f, 1.0f, 1.0f);
    ImVec4 accentDim    = ImVec4(0.0f, 0.82f, 1.0f, 0.2f);
    ImVec4 toggleOn     = ImVec4(0.13f, 0.77f, 0.36f, 1.0f);

    enum class Preset { Dark, Light, Custom };

    static NexusTheme& instance();
    void apply();
    void loadFromJson(const std::string& path);
    void saveToJson(const std::string& path) const;
    static ImVec4 HexColor(uint32_t hex);
};
