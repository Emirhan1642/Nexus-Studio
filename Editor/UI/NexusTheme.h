#pragma once
#include <imgui.h>
#include <string>

struct NexusTheme {
    ImVec4 bg           = HexColor(0x050505);
    ImVec4 panel        = HexColor(0x0e0e0e);
    ImVec4 panelHover   = HexColor(0x171717);
    ImVec4 border       = HexColor(0x242424);
    ImVec4 textMuted    = HexColor(0x8e8e8e);
    ImVec4 textPrimary  = HexColor(0xe5e5e5);
    ImVec4 accent       = HexColor(0x00d2ff);
    ImVec4 accentDim    = HexColorAlpha(0x00d2ff, 0.2f);
    ImVec4 toggleOn     = HexColor(0x22c55e);

    enum class Preset { Dark, Light, Custom };

    static NexusTheme& instance();
    void apply();
    void loadFromJson(const std::string& path);
    void saveToJson(const std::string& path) const;
    static ImVec4 HexColor(uint32_t hex);
    static ImVec4 HexColorAlpha(uint32_t hex, float alpha);
};
