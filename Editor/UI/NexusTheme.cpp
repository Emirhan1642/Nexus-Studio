#include "NexusTheme.h"
#include <imgui.h>

NexusTheme& NexusTheme::instance() {
    static NexusTheme s_instance;
    return s_instance;
}

ImVec4 NexusTheme::HexColor(uint32_t hex) {
    return HexColorAlpha(hex, 1.0f);
}

ImVec4 NexusTheme::HexColorAlpha(uint32_t hex, float alpha) {
    return ImVec4(
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >>  8) & 0xFF) / 255.0f,
        ((hex      ) & 0xFF) / 255.0f,
        alpha
    );
}

void NexusTheme::apply() {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    
    s.Colors[ImGuiCol_WindowBg]          = HexColor(0x080808);   // bgDeepest
    s.Colors[ImGuiCol_ChildBg]           = HexColor(0x0E0E0E);   // bgPanel
    s.Colors[ImGuiCol_PopupBg]           = HexColor(0x0E0E0E);
    s.Colors[ImGuiCol_Border]            = HexColor(0x242424);
    s.Colors[ImGuiCol_BorderShadow]      = ImVec4(0, 0, 0, 0);

    // Frames (input fields, dropdowns)
    s.Colors[ImGuiCol_FrameBg]           = HexColor(0x0A0A0A);
    s.Colors[ImGuiCol_FrameBgHovered]    = HexColor(0x171717);
    s.Colors[ImGuiCol_FrameBgActive]     = HexColor(0x242424);

    // Title bar (panel header)
    s.Colors[ImGuiCol_TitleBg]           = HexColor(0x0E0E0E);
    s.Colors[ImGuiCol_TitleBgActive]     = HexColor(0x0E0E0E);
    s.Colors[ImGuiCol_TitleBgCollapsed]  = HexColor(0x0E0E0E);

    // Scrollbar
    s.Colors[ImGuiCol_ScrollbarBg]       = HexColor(0x0A0A0A);
    s.Colors[ImGuiCol_ScrollbarGrab]     = HexColor(0x242424);
    s.Colors[ImGuiCol_ScrollbarGrabHovered]= HexColor(0x82D9FF);
    s.Colors[ImGuiCol_ScrollbarGrabActive] = HexColor(0x82D9FF);

    // Checkmark, slider
    s.Colors[ImGuiCol_CheckMark]         = HexColor(0x66FF99);   // accentGreen
    s.Colors[ImGuiCol_SliderGrab]        = HexColor(0x82D9FF);
    s.Colors[ImGuiCol_SliderGrabActive]  = HexColor(0x82D9FF);

    // Buttons
    s.Colors[ImGuiCol_Button]            = HexColor(0x0E0E0E);
    s.Colors[ImGuiCol_ButtonHovered]     = HexColor(0x171717);
    s.Colors[ImGuiCol_ButtonActive]      = HexColor(0x242424);

    // Headers (tree nodes, selectables)
    s.Colors[ImGuiCol_Header]            = HexColor(0x171717);
    s.Colors[ImGuiCol_HeaderHovered]     = HexColor(0x242424);
    s.Colors[ImGuiCol_HeaderActive]      = HexColorAlpha(0x82D9FF, 0.20f);

    // Separator
    s.Colors[ImGuiCol_Separator]         = HexColor(0x242424);
    s.Colors[ImGuiCol_SeparatorHovered]  = HexColor(0x82D9FF);
    s.Colors[ImGuiCol_SeparatorActive]   = HexColor(0x82D9FF);

    // Resize grip
    s.Colors[ImGuiCol_ResizeGrip]        = HexColorAlpha(0x82D9FF, 0.10f);
    s.Colors[ImGuiCol_ResizeGripHovered] = HexColorAlpha(0x82D9FF, 0.40f);
    s.Colors[ImGuiCol_ResizeGripActive]  = HexColor(0x82D9FF);

    // Tabs  — active tab: darker bg, top accent line drawn by panel code
    s.Colors[ImGuiCol_Tab]               = HexColor(0x0A0A0A);
    s.Colors[ImGuiCol_TabHovered]        = HexColor(0x171717);
    s.Colors[ImGuiCol_TabActive]         = HexColor(0x0E0E0E);
    s.Colors[ImGuiCol_TabUnfocused]      = HexColor(0x0A0A0A);
    s.Colors[ImGuiCol_TabUnfocusedActive]= HexColor(0x0E0E0E);

    // Docking
    s.Colors[ImGuiCol_DockingPreview]    = HexColorAlpha(0x82D9FF, 0.30f);
    s.Colors[ImGuiCol_DockingEmptyBg]    = HexColor(0x080808);

    // Text
    s.Colors[ImGuiCol_Text]              = HexColor(0xFFFFFF);
    s.Colors[ImGuiCol_TextDisabled]      = HexColor(0x8E8E8E);

    // NavHighlight
    s.Colors[ImGuiCol_NavHighlight]      = HexColor(0x82D9FF);

    // ── Geometry ───────────────────────────────────────────────────────
    s.WindowRounding    = 0.0f;  // Panels are sharp-cornered per design
    s.ChildRounding     = 0.0f;
    s.FrameRounding     = 3.0f;
    s.GrabRounding      = 3.0f;
    s.PopupRounding     = 4.0f;
    s.ScrollbarRounding = 3.0f;
    s.TabRounding       = 0.0f;  // Tabs are square per design

    s.WindowBorderSize  = 1.0f;
    s.ChildBorderSize   = 1.0f;
    s.FrameBorderSize   = 1.0f;
    s.PopupBorderSize   = 1.0f;

    s.IndentSpacing     = 16.0f;
    s.ItemSpacing       = ImVec2(6, 3);
    s.FramePadding      = ImVec2(8, 4);
    s.WindowPadding     = ImVec2(10, 10);
    s.ScrollbarSize     = 6.0f;   // Thin scrollbar
}

void NexusTheme::loadFromJson(const std::string& path) {
    // Left empty for future implementation
}

void NexusTheme::saveToJson(const std::string& path) const {
    // Left empty for future implementation
}
