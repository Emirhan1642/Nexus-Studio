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
    ImVec4 accentGlow   = HexColorAlpha(0x00d2ff, 0.2f);
    ImVec4 toggleOn     = HexColor(0x22c55e);
    ImVec4 toggleOff    = HexColor(0x242424);
    
    // Physics & Nodes
    ImVec4 orange       = HexColor(0xf97316);
    ImVec4 amber        = HexColor(0xf59e0b);
    ImVec4 green        = HexColor(0x22c55e);
    ImVec4 teal         = HexColor(0x14b8a6);
    ImVec4 purple       = HexColor(0xa855f7);
    ImVec4 blue         = HexColor(0x3b82f6);
    ImVec4 cyan         = HexColor(0x22d3ee);
    ImVec4 yellow       = HexColor(0xfacc15);
    
    // Axis colors
    ImVec4 axisRed      = HexColor(0xef4444);
    ImVec4 axisGreen    = HexColor(0x4ade80);
    ImVec4 axisBlue     = HexColor(0x60a5fa);

    enum class Preset { Dark, Light, Custom };

    static NexusTheme& instance();
    void apply();
    void loadFromJson(const std::string& path);
    void saveToJson(const std::string& path) const;
    static ImVec4 HexColor(uint32_t hex);
    static ImVec4 HexColorAlpha(uint32_t hex, float alpha);
};
