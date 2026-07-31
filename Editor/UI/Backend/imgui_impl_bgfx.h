#pragma once
#include <imgui.h>
#include <bgfx/bgfx.h>

// Initializes the bgfx ImGui backend.
// viewId is the bgfx view that will be used to submit the ImGui draw calls.
bool ImGui_ImplBgfx_Init(uint8_t viewId);

// Shuts down the bgfx ImGui backend.
void ImGui_ImplBgfx_Shutdown();

// Prepares the backend for a new frame.
void ImGui_ImplBgfx_NewFrame();

// Renders the ImGui draw data using bgfx.
void ImGui_ImplBgfx_RenderDrawData(ImDrawData* drawData);
