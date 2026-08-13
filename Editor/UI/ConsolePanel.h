#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <imgui.h>

namespace Editor::UI {

struct ConsoleLogEntry {
    enum class Level {
        Info,
        Warning,
        Error
    };
    Level level;
    std::string message;
    std::string timestamp;
};

class ConsolePanel {
public:
    static ConsolePanel& instance() {
        static ConsolePanel s_instance;
        return s_instance;
    }

    void initialize();
    void addLog(ConsoleLogEntry::Level level, const std::string& message);
    void clearLogs();

    void draw();
    void drawContents();

private:
    ConsolePanel();
    ~ConsolePanel() = default;

    std::vector<ConsoleLogEntry> m_logs;
    std::mutex m_logMutex;
    int m_consoleFilter = 0; // 0 = All, 1 = Info, 2 = Warnings, 3 = Errors
    bool m_autoScrollConsole = true;
};

} // namespace Editor::UI
