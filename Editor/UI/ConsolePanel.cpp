#include "ConsolePanel.h"
#include "SharedTabBar.h"
#include "NexusTheme.h"
#include "IconRegistry.h"
#include "EditorLayout.h"
#include <imgui_internal.h>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace Editor::UI {

static ImU32 COL(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a) {
    return IM_COL32((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF, (uint8_t)(a * 255));
}

static std::string getCurrentTimeString() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << tm.tm_hour << ":"
        << std::setfill('0') << std::setw(2) << tm.tm_min << ":"
        << std::setfill('0') << std::setw(2) << tm.tm_sec;
    return oss.str();
}

ConsolePanel::ConsolePanel() {
    initialize();
}

void ConsolePanel::initialize() {
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (m_logs.empty()) {
        m_logs.push_back({ConsoleLogEntry::Level::Info, "Nexus Studio Engine initialized.", getCurrentTimeString()});
        m_logs.push_back({ConsoleLogEntry::Level::Info, "Luau Scripting Runtime ready.", getCurrentTimeString()});
        m_logs.push_back({ConsoleLogEntry::Level::Info, "Asset Import Pipeline active.", getCurrentTimeString()});
    }
}

void ConsolePanel::addLog(ConsoleLogEntry::Level level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    m_logs.push_back({level, message, getCurrentTimeString()});
    if (m_logs.size() > 1000) {
        m_logs.erase(m_logs.begin(), m_logs.begin() + 200);
    }
}

void ConsolePanel::clearLogs() {
    std::lock_guard<std::mutex> lock(m_logMutex);
    m_logs.clear();
}

void ConsolePanel::draw() {
    if (!EditorLayout::instance().showConsole) return;

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_HiddenTabBar;
    ImGui::SetNextWindowClass(&window_class);
    float minW = Editor::UI::CalculateNodeMinTabWidth("Console");
    ImGui::SetNextWindowSizeConstraints(ImVec2(minW, 80.0f), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("Console", &EditorLayout::instance().showConsole, flags)) {
        DrawSingleTabHeader("Console", "icon_script_bold", 140.0f, ImGui::ColorConvertFloat4ToU32(NexusTheme::instance().accent));
        drawContents();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void ConsolePanel::drawContents() {
    auto& T = NexusTheme::instance();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float width = ImGui::GetWindowWidth();
    float height = ImGui::GetContentRegionAvail().y;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.bgDeepest);
    ImGui::BeginChild("##ConsoleRootContainer", ImVec2(width, height), false, 0);

    // ─────────────────────────────────────────────────────────────────────────
    // Console Filter Toolbar (h = 28px)
    // ─────────────────────────────────────────────────────────────────────────
    ImVec2 tbPos = ImGui::GetCursorScreenPos();
    float tbH = 28.0f;
    dl->AddRectFilled(tbPos, ImVec2(tbPos.x + width, tbPos.y + tbH), COL(T.bgDeep), 0.0f);
    dl->AddLine(ImVec2(tbPos.x, tbPos.y + tbH - 1.0f), ImVec2(tbPos.x + width, tbPos.y + tbH - 1.0f), COL(T.border));

    ImGui::SetCursorScreenPos(ImVec2(tbPos.x + 8.0f, tbPos.y + 3.0f));

    auto drawFilterChip = [&](const char* label, int filterId, ImU32 activeColor) {
        bool active = (m_consoleFilter == filterId);
        ImVec2 ts = ImGui::CalcTextSize(label);
        float chipW = ts.x + 16.0f;
        ImVec2 p = ImGui::GetCursorScreenPos();

        if (ImGui::InvisibleButton(label, ImVec2(chipW, 22.0f))) {
            m_consoleFilter = filterId;
        }
        bool hov = ImGui::IsItemHovered();

        dl->AddRectFilled(p, ImVec2(p.x + chipW, p.y + 22.0f), active ? activeColor : (hov ? COLA(0xFFFFFF, 0.08f) : COL(T.bgCard)), 4.0f);
        dl->AddText(ImVec2(p.x + 8.0f, p.y + 3.0f), active ? IM_COL32_BLACK : COL(T.textPrimary), label);
        ImGui::SameLine(0, 6.0f);
    };

    drawFilterChip("All", 0, COL(T.accent));
    drawFilterChip("Info", 1, COL(T.accent));
    drawFilterChip("Warnings", 2, COL(T.accentYellow));
    drawFilterChip("Errors", 3, COL(T.accentRed));

    // Clear Button on right
    float clearBtnW = 60.0f;
    ImGui::SetCursorScreenPos(ImVec2(tbPos.x + width - clearBtnW - 10.0f, tbPos.y + 3.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, COL(T.bgCard));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL(T.panelHover));
    if (ImGui::Button("Clear", ImVec2(clearBtnW, 22.0f))) {
        clearLogs();
    }
    ImGui::PopStyleColor(2);

    ImGui::SetCursorPos(ImVec2(8, tbH + 6.0f));

    // ─────────────────────────────────────────────────────────────────────────
    // Console Logs List View
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::BeginChild("##ConsoleLogScrollPane", ImVec2(width - 16.0f, height - tbH - 12.0f), false, 0);

    {
        std::lock_guard<std::mutex> lock(m_logMutex);
        for (const auto& log : m_logs) {
            if (m_consoleFilter == 1 && log.level != ConsoleLogEntry::Level::Info) continue;
            if (m_consoleFilter == 2 && log.level != ConsoleLogEntry::Level::Warning) continue;
            if (m_consoleFilter == 3 && log.level != ConsoleLogEntry::Level::Error) continue;

            ImU32 lvlColor = COL(T.accent);
            const char* lvlTag = "[INFO]";
            if (log.level == ConsoleLogEntry::Level::Warning) {
                lvlColor = COL(T.accentYellow);
                lvlTag = "[WARN]";
            } else if (log.level == ConsoleLogEntry::Level::Error) {
                lvlColor = COL(T.accentRed);
                lvlTag = "[ERROR]";
            }

            ImGui::TextColored(T.textMuted, "%s", log.timestamp.c_str());
            ImGui::SameLine(0, 8.0f);
            ImGui::TextColored(ImColor(lvlColor), "%s", lvlTag);
            ImGui::SameLine(0, 8.0f);
            ImGui::TextUnformatted(log.message.c_str());
        }
    }

    if (m_autoScrollConsole && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace Editor::UI
