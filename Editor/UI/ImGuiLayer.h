#pragma once
#include <GLFW/glfw3.h>

class ImGuiLayer {
public:
    static ImGuiLayer& instance() {
        static ImGuiLayer s_instance;
        return s_instance;
    }

    void init(GLFWwindow* window);
    void shutdown();
    void beginFrame();
    void endFrame();

    // Callbacks to capture input
    void onScroll(double yoffset);
    void onChar(unsigned int codepoint);

private:
    ImGuiLayer() = default;
    GLFWwindow* m_window = nullptr;

    int32_t m_scroll = 0;
    int m_inputChar = -1;
};
