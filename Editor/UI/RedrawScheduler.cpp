#include "RedrawScheduler.h"
#include <imgui.h>

RedrawMode RedrawScheduler::resolveMode(double now, bool windowFocused) {
    if (m_isPlaying) return RedrawMode::Playing;

    // Pencere odakta degilse dogrudan Idle'a gec (Unity'nin "uygulama aktif
    // degilse 2 FPS", Roblox Studio'nun "tab-out'ta ~30 FPS" davranisiyla
    // ayni desen -- odak kaybi = throttle tetikleyicisi).
    if (!windowFocused) return RedrawMode::Idle;

    bool idleTimedOut = (now - m_lastActivityTime) > m_idleGraceSeconds;
    if (!m_dirty && idleTimedOut) return RedrawMode::Idle;

    return RedrawMode::Active;
}

void RedrawScheduler::requestRedraw() {
    m_dirty = true;
    m_lastActivityTime = glfwGetTime();
}

void RedrawScheduler::pumpEvents(GLFWwindow* window) {
    double now = glfwGetTime();
    bool focused = glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;
    m_currentMode = resolveMode(now, focused);

    if (m_currentMode != RedrawMode::Idle) {
        // Active / Playing: throttle'siz poll. VSync (BGFX_RESET_VSYNC) zaten
        // frame hizini monitor Hz'ine kilitliyor, ek Sleep gereksiz.
        glfwPollEvents();
    } else {
        // Idle: event gelene kadar bekle, ama en fazla 1/idleTargetFps saniye.
        // Godot'nun "Low Processor Mode Sleep" parametresinin GLFW karsiligi:
        // VRR monitor SABIT bir dusuk-FPS sinyali gorur, degisken degil.
        glfwWaitEventsTimeout(1.0 / (double)m_idleTargetFps);
    }

    // ImGui'nin bu frame'de girdi algilayip algilamadigini kontrol et. Ayri
    // GLFW callback kurmaya gerek yok; ImGui_ImplGlfw (install_callbacks=true
    // ile baslatilmis) IO state'i zaten GLFW'den dolduruyor.
    ImGuiIO& io = ImGui::GetIO();
    bool inputActivity =
        io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f ||
        io.MouseWheel   != 0.0f || io.MouseWheelH  != 0.0f ||
        ImGui::IsAnyMouseDown();

    if (!inputActivity) {
        for (int i = 0; i < ImGuiKey_NamedKey_COUNT; ++i) {
            ImGuiKey key = (ImGuiKey)(ImGuiKey_NamedKey_BEGIN + i);
            if (ImGui::IsKeyPressed(key, false)) { inputActivity = true; break; }
        }
    }

    if (inputActivity) {
        requestRedraw();
    } else if (m_dirty && (now - m_lastActivityTime) > 0.05) {
        // Dirty flag'i bir frame sonra tuket: bir property degisti, bir frame
        // cizdik, sonraki turde tekrar Idle'a geri donebiliriz.
        m_dirty = false;
    }
}