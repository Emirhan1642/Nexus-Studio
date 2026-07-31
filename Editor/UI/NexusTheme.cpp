#include "NexusTheme.h"
#include <imgui.h>

NexusTheme& NexusTheme::instance() {
    static NexusTheme s_instance;
    return s_instance;
}

ImVec4 NexusTheme::HexColor(uint32_t hex) {
    return ImVec4(
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8) & 0xFF) / 255.0f,
        ((hex) & 0xFF) / 255.0f,
        1.0f
    );
}

void NexusTheme::apply() {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    
    s.Colors[ImGuiCol_WindowBg]     = HexColor(0x0e0e0e);
    s.Colors[ImGuiCol_ChildBg]      = HexColor(0x050505);
    s.Colors[ImGuiCol_PopupBg]      = HexColor(0x0e0e0e);
    s.Colors[ImGuiCol_Border]       = HexColor(0x242424);
    s.Colors[ImGuiCol_FrameBg]      = HexColor(0x171717);
    s.Colors[ImGuiCol_TitleBg]      = HexColor(0x0e0e0e);
    s.Colors[ImGuiCol_TitleBgActive]= HexColor(0x0e0e0e);
    s.Colors[ImGuiCol_MenuBarBg]    = HexColor(0x0e0e0e);
    s.Colors[ImGuiCol_Header]       = ImVec4(0, 0.82f, 1, 0.15f);   // accent/15
    s.Colors[ImGuiCol_HeaderHovered]= ImVec4(0, 0.82f, 1, 0.25f);
    s.Colors[ImGuiCol_Tab]          = HexColor(0x0e0e0e);
    s.Colors[ImGuiCol_TabActive]    = HexColor(0x050505);
    s.Colors[ImGuiCol_TabHovered]   = HexColor(0x171717);
    s.Colors[ImGuiCol_Button]       = HexColor(0x171717);
    s.Colors[ImGuiCol_ButtonHovered]= HexColor(0x242424);
    s.Colors[ImGuiCol_CheckMark]    = HexColor(0x00d2ff);
    s.Colors[ImGuiCol_SliderGrab]   = HexColor(0x00d2ff);
    s.Colors[ImGuiCol_DockingPreview] = ImVec4(0, 0.82f, 1, 0.3f);
    s.Colors[ImGuiCol_Text]         = HexColor(0xe5e5e5);
    s.Colors[ImGuiCol_TextDisabled] = HexColor(0x8e8e8e);

    s.WindowRounding   = 0.0f;
    s.FrameRounding    = 3.0f;
    s.GrabRounding     = 3.0f;
    s.TabRounding      = 0.0f;
    s.WindowBorderSize = 1.0f;
    s.FrameBorderSize  = 0.0f;
    s.IndentSpacing    = 12.0f;
    s.ItemSpacing      = ImVec2(4, 3);
    s.FramePadding     = ImVec2(6, 3);
}

void NexusTheme::loadFromJson(const std::string& path) {
    // Left empty for future implementation
}

void NexusTheme::saveToJson(const std::string& path) const {
    // Left empty for future implementation
}
