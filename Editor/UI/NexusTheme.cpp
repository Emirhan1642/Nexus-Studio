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
        ((hex >> 8) & 0xFF) / 255.0f,
        ((hex) & 0xFF) / 255.0f,
        alpha
    );
}

void NexusTheme::apply() {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    
    s.Colors[ImGuiCol_WindowBg]     = HexColor(0x050505);
    s.Colors[ImGuiCol_ChildBg]      = HexColor(0x0e0e0e);
    s.Colors[ImGuiCol_PopupBg]      = HexColor(0x0e0e0e);
    s.Colors[ImGuiCol_Border]       = HexColor(0x242424);
    s.Colors[ImGuiCol_FrameBg]      = HexColor(0x0e0e0e);
    s.Colors[ImGuiCol_FrameBgHovered]=HexColor(0x171717);
    s.Colors[ImGuiCol_FrameBgActive]= HexColor(0x242424);
    s.Colors[ImGuiCol_TitleBg]      = HexColor(0x0e0e0e);
    s.Colors[ImGuiCol_TitleBgActive]= HexColor(0x0e0e0e);
    s.Colors[ImGuiCol_MenuBarBg]    = HexColor(0x050505);
    s.Colors[ImGuiCol_Header]       = HexColor(0x171717);
    s.Colors[ImGuiCol_HeaderHovered]= HexColor(0x242424);
    s.Colors[ImGuiCol_HeaderActive] = HexColor(0x242424);
    s.Colors[ImGuiCol_Tab]          = HexColor(0x050505);
    s.Colors[ImGuiCol_TabActive]    = HexColor(0x0e0e0e);
    s.Colors[ImGuiCol_TabHovered]   = HexColor(0x171717);
    s.Colors[ImGuiCol_TabUnfocused] = HexColor(0x050505);
    s.Colors[ImGuiCol_TabUnfocusedActive]= HexColor(0x0e0e0e);
    s.Colors[ImGuiCol_Button]       = HexColor(0x0e0e0e);
    s.Colors[ImGuiCol_ButtonHovered]= HexColor(0x171717);
    s.Colors[ImGuiCol_ButtonActive] = HexColor(0x242424);
    s.Colors[ImGuiCol_CheckMark]    = HexColor(0x00d2ff);
    s.Colors[ImGuiCol_SliderGrab]   = HexColor(0x00d2ff);
    s.Colors[ImGuiCol_SliderGrabActive] = HexColor(0x00d2ff);
    s.Colors[ImGuiCol_DockingPreview] = HexColorAlpha(0x00d2ff, 0.3f);
    s.Colors[ImGuiCol_Text]         = HexColor(0xe5e5e5);
    s.Colors[ImGuiCol_TextDisabled] = HexColor(0x8e8e8e);
    s.Colors[ImGuiCol_Separator]    = HexColor(0x242424);
    s.Colors[ImGuiCol_SeparatorHovered] = HexColor(0x00d2ff);
    s.Colors[ImGuiCol_SeparatorActive]  = HexColor(0x00d2ff);

    s.WindowRounding   = 6.0f;
    s.ChildRounding    = 4.0f;
    s.FrameRounding    = 4.0f;
    s.GrabRounding     = 4.0f;
    s.PopupRounding    = 6.0f;
    s.ScrollbarRounding = 4.0f;
    s.TabRounding      = 4.0f;
    s.WindowBorderSize = 1.0f;
    s.ChildBorderSize  = 1.0f;
    s.FrameBorderSize  = 1.0f;
    s.PopupBorderSize  = 1.0f;
    s.IndentSpacing    = 16.0f;
    s.ItemSpacing      = ImVec2(6, 4);
    s.FramePadding     = ImVec2(8, 4);
}

void NexusTheme::loadFromJson(const std::string& path) {
    // Left empty for future implementation
}

void NexusTheme::saveToJson(const std::string& path) const {
    // Left empty for future implementation
}
