#pragma once
#include <imgui.h>
#include <string>

struct NexusTheme {
    // ── Background hierarchy (darkest → lightest) ──────────────────
    ImVec4 bgDeepest    = HexColor(0x080808);  // PropertyTree, AssetManager body
    ImVec4 bgDeep       = HexColor(0x0A0A0A);  // Copilot body, CodeBlock
    ImVec4 bgPanel      = HexColor(0x0E0E0E);  // All top-bars, LeftBar, FileListBar
    ImVec4 bgCard       = HexColor(0x171717);  // Message bubbles, hover rows
    ImVec4 border       = HexColor(0x242424);  // All borders / separators

    // ── Text ─────────────────────────────────────────────────────────
    ImVec4 textPrimary  = HexColor(0xFFFFFF);
    ImVec4 textSecondary= HexColorAlpha(0xFFFFFF, 0.50f);  // Property labels
    ImVec4 textMuted    = HexColor(0x8E8E8E);              // Timestamps, icon color
    ImVec4 textPlaceholder = HexColorAlpha(0xFFFFFF, 0.40f);

    // ── Backward-compatible aliases (do not remove — used by existing panels) ──
    ImVec4& bg        = bgDeepest;   // was: HexColor(0x050505)
    ImVec4& panel     = bgPanel;     // was: HexColor(0x0e0e0e)
    ImVec4& panelHover= bgCard;      // was: HexColor(0x171717)
    ImVec4  toggleOn  = HexColor(0x66FF99);  // active toggle — maps to accentGreen
    ImVec4  toggleOff = HexColor(0x242424);  // inactive toggle — same as border

    // ── Accent palette ───────────────────────────────────────────────
    ImVec4 accent       = HexColor(0x82D9FF);  // Primary: active tab, AI outline, Z-axis
    ImVec4 accentDim    = HexColorAlpha(0x82D9FF, 0.20f);
    ImVec4 accentGlow   = HexColorAlpha(0x82D9FF, 0.25f);
    ImVec4 accentGreen  = HexColor(0x66FF99);  // OK / Y-axis / code diff add
    ImVec4 accentRed    = HexColor(0xDA6464);  // Error / X-axis
    ImVec4 accentYellow = HexColor(0xFFE47B);  // Warning
    ImVec4 accentOrange = HexColor(0xFF7700);  // Camera node type
    ImVec4 accentLime   = HexColor(0xA7FF71);  // Snap / GameModels folder
    ImVec4 accentLimeY  = HexColor(0xBBFF00);  // DirectionalLight node
    ImVec4 accentGold   = HexColor(0xFFC64B);  // Albedo tint example

    // ── Axis colors ──────────────────────────────────────────────────
    ImVec4 axisX        = HexColor(0xDA6464);  // X — Red
    ImVec4 axisY        = HexColor(0x66FF99);  // Y — Green
    ImVec4 axisZ        = HexColor(0x82D9FF);  // Z — Blue

    // ── Semantic ─────────────────────────────────────────────────────
    ImVec4 ok           = HexColor(0x66FF99);
    ImVec4 warning      = HexColor(0xFFE47B);
    ImVec4 error        = HexColor(0xDA6464);
    ImVec4 info         = HexColor(0x82D9FF);

    enum class Preset { Dark, Light, Custom };

    static NexusTheme& instance();
    void apply();
    void loadFromJson(const std::string& path);
    void saveToJson(const std::string& path) const;
    static ImVec4 HexColor(uint32_t hex);
    static ImVec4 HexColorAlpha(uint32_t hex, float alpha);
};
