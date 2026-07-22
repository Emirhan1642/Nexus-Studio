#include "ImGuiLayer.h"
#include <imgui/imgui.h> // from example-common
#include <dear-imgui/imgui.h>

void ImGuiLayer::init(GLFWwindow* window) {
    m_window = window;
    
    // Initialize bgfx imgui wrapper
    imguiCreate(18.0f, nullptr);
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
}

void ImGuiLayer::shutdown() {
    imguiDestroy();
}

void ImGuiLayer::onScroll(double yoffset) {
    m_scroll += (int32_t)yoffset;
}

void ImGuiLayer::onChar(unsigned int codepoint) {
    m_inputChar = (int)codepoint;
}

void ImGuiLayer::beginFrame() {
    int width, height;
    glfwGetWindowSize(m_window, &width, &height);
    
    double mx, my;
    glfwGetCursorPos(m_window, &mx, &my);
    
    uint8_t button = 0;
    if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) button |= IMGUI_MBUT_LEFT;
    if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) button |= IMGUI_MBUT_RIGHT;
    if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) button |= IMGUI_MBUT_MIDDLE;
    
    imguiBeginFrame((int32_t)mx, (int32_t)my, button, m_scroll, (uint16_t)width, (uint16_t)height, m_inputChar);
    
    // Reset inputs for next frame
    m_scroll = 0;
    m_inputChar = -1;
    
    // Window positions will be handled directly by their respective panels.

    ImGuizmo::BeginFrame();
}

void ImGuiLayer::endFrame() {
    imguiEndFrame();
}
