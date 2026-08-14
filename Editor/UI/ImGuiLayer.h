#pragma once
#include <GLFW/glfw3.h>
#include <imgui.h>

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

    ImFont* getMonoFont() const { return m_fontMono; }

private:
    ImGuiLayer() = default;
    void buildDefaultLayout(ImGuiID dockspaceId, ImVec2 size);

    GLFWwindow* m_window = nullptr;
    ImGuiID m_dockspaceId = 0;
    ImFont* m_fontMono = nullptr;
};
