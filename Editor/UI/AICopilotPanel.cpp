#include "AICopilotPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "EditorLayout.h"
#include "NexusTheme.h"
#include "IconRegistry.h"
#include "SharedTabBar.h"
#include "SelectionManager.h"
#include "Engine/Core/DataModel/Instance.h"
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <thread>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>
#include <stdio.h>
#include <algorithm>

// ─── Renk kısayolları ───────────────────────────────────────────────────────
static ImU32 COL(const ImVec4& v) { return ImGui::ColorConvertFloat4ToU32(v); }
static ImU32 COLA(uint32_t hex, float a) {
    return IM_COL32((hex>>16)&0xFF,(hex>>8)&0xFF,hex&0xFF,(uint8_t)(a*255));
}

// ─── Mesaj yapısı ───────────────────────────────────────────────────────────
struct ChatMessage {
    bool   isUser;
    std::string text;
    std::string timeLabel;

    // AI / Asistan detayları
    bool   hasThought = false;
    std::string thoughtTime = "Thought for 34sec";
    std::string thoughtDetail = "• Analyzing material properties of PBR_Gold.mat\n"
                                "• Computed micro-surface scatter roughness coefficient\n"
                                "• Adjusted micro-surface Anisotropy parameter to match specular highlight gradient\n"
                                "• Optimized metallic reflectance spectrum for PBR shading node";
    bool   thoughtOpen = false;

    bool   hasTools = false;
    std::string toolSummary = "2 Tool called";
    std::string toolDetail = "• Tool 1: MaterialGraph::QueryParam(\"Roughness\") -> current: 0.65\n"
                            "• Tool 2: MaterialGraph::SetMicroSurface(\"Anisotropy\", 0.18f) -> OK";
    bool   toolOpen = false;

    bool   hasCodeBlock = false;
    std::vector<std::string> codeLines;

    bool   hasApplyButton = false;
    std::string applyButtonText = "Apply Changes to Node Graph";
    bool   isApplied = false;
};

static std::vector<ChatMessage> s_messages = {
    // 1. User Message (Figma örneği)
    {
        true,
        "Optimize roughness for realistic brushed gold.",
        "12:35",
        false, "", "", false,
        false, "", "", false,
        false, {},
        false, "", false
    },
    // 2. Assistant Message (Figma örneği)
    {
        false,
        "I adjusted Roughness to 0.18 and injected micro-surface Anisotropy into the PBR graph.",
        "12:39",
        true, "Thought for 34sec",
        "• Analyzed active material 'PBR_Gold.mat'\n"
        "• Evaluated roughness response on directional illumination\n"
        "• Calibrated anisotropic micro-facet roughness: 0.18\n"
        "• Tweaked metallic specular blend parameter: 0.02",
        false,
        true, "2 Tool called",
        "• Tool 1: MaterialGraph::QueryParam(\"Roughness\") -> 0.65\n"
        "• Tool 2: MaterialGraph::SetMicroSurface(\"Anisotropy\", 0.18f) -> OK",
        false,
        true, { "+ Roughness = 0.18", "+ Metalic = 0.02" },
        true, "Apply Changes to Node Graph", false
    }
};

static char  s_inputBuf[512] = "";
static char  s_currentChatTitle[128] = "Optimize Roughness";
static char  s_searchFilter[128] = "";
static bool  s_showSearch = false;
static bool  s_showSettings = false;
static char  s_apiKeyBuf[256] = "";

static const char* s_providers[] = { "OpenRouter", "OpenAI", "Anthropic", "Nexus Local AI" };
static int s_selectedProvider = 0; // "OpenRouter"

static const char* s_models[] = { "Claude Sonnet 5", "Claude 3.5 Sonnet", "GPT-4o", "DeepSeek-V3", "Llama 3.3 70B" };
static int s_selectedModel = 0; // "Claude Sonnet 5"

static std::mutex s_chatMutex;
static std::atomic<bool> s_isWaitingForAI{false};

// Basit _popen wrapper
static std::string execCmd(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    if (!pipe) return "Error running curl";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

static void fetchAIResponse(std::string prompt) {
    s_isWaitingForAI = true;

    std::string escapedPrompt;
    for (char c : prompt) {
        if (c == '"') escapedPrompt += "\\\"";
        else if (c == '\\') escapedPrompt += "\\\\";
        else if (c == '\n') escapedPrompt += "\\n";
        else escapedPrompt += c;
    }

    std::string apiKey = s_apiKeyBuf;
    if (apiKey.empty()) {
        if (const char* env_p = std::getenv("OPENROUTER_API_KEY")) {
            apiKey = env_p;
        }
    }

    auto now = std::time(nullptr);
    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M", std::localtime(&now));

    if (!apiKey.empty()) {
        std::string modelTag = "anthropic/claude-3.5-sonnet";
        if (s_selectedModel == 0 || s_selectedModel == 1) modelTag = "anthropic/claude-3.5-sonnet";
        else if (s_selectedModel == 2) modelTag = "openai/gpt-4o";
        else if (s_selectedModel == 3) modelTag = "deepseek/deepseek-chat";
        else if (s_selectedModel == 4) modelTag = "meta-llama/llama-3.3-70b-instruct";

        std::string cmd = "curl.exe -s -X POST https://openrouter.ai/api/v1/chat/completions ";
        cmd += "-H \"Authorization: Bearer " + apiKey + "\" ";
        cmd += "-H \"Content-Type: application/json\" ";
        cmd += "-d \"{\\\"model\\\": \\\"" + modelTag + "\\\", \\\"messages\\\": [{\\\"role\\\": \\\"user\\\", \\\"content\\\": \\\"" + escapedPrompt + "\\\"}]}\"";

        std::string rawJson = execCmd(cmd.c_str());
        std::string responseText = "";
        try {
            auto j = nlohmann::json::parse(rawJson);
            if (j.contains("choices") && j["choices"].size() > 0) {
                responseText = j["choices"][0]["message"]["content"].get<std::string>();
            } else {
                responseText = "API Response: " + rawJson;
            }
        } catch(...) {
            responseText = "Failed to parse API response.";
        }

        std::lock_guard<std::mutex> lock(s_chatMutex);
        s_messages.push_back({
            false,
            responseText,
            timeBuf,
            true, "Thought for 12sec",
            "Processed prompt via OpenRouter model: " + modelTag,
            false,
            false, "", "", false,
            false, {},
            false, "", false
        });
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(900));

        std::string lowerPrompt = prompt;
        std::transform(lowerPrompt.begin(), lowerPrompt.end(), lowerPrompt.begin(), ::tolower);

        ChatMessage reply;
        reply.isUser = false;
        reply.timeLabel = timeBuf;
        reply.hasThought = true;
        reply.thoughtTime = "Thought for 18sec";
        reply.thoughtDetail = "• Parsed scene graph and selection context\n"
                              "• Formulated parameter delta and graph connections\n"
                              "• Validated lighting and shader graph dependencies";
        reply.thoughtOpen = false;

        if (lowerPrompt.find("roughness") != std::string::npos || lowerPrompt.find("gold") != std::string::npos || lowerPrompt.find("material") != std::string::npos) {
            reply.text = "I adjusted Roughness to 0.18 and injected micro-surface Anisotropy into the PBR graph.";
            reply.hasTools = true;
            reply.toolSummary = "2 Tool called";
            reply.toolDetail = "• Tool 1: MaterialGraph::QueryParam(\"Roughness\")\n"
                               "• Tool 2: MaterialGraph::SetMicroSurface(\"Anisotropy\", 0.18f)";
            reply.hasCodeBlock = true;
            reply.codeLines = { "+ Roughness = 0.18", "+ Metalic = 0.02" };
            reply.hasApplyButton = true;
            reply.applyButtonText = "Apply Changes to Node Graph";
        } else if (lowerPrompt.find("light") != std::string::npos || lowerPrompt.find("shadow") != std::string::npos) {
            reply.text = "I enhanced the directional light intensity and enabled soft shadow bias for realistic ambient occlusion.";
            reply.hasTools = true;
            reply.toolSummary = "1 Tool called";
            reply.toolDetail = "• Tool 1: LightingService::SetShadowBias(0.04f)";
            reply.hasCodeBlock = true;
            reply.codeLines = { "+ LightIntensity = 2.40", "+ ShadowSoftness = 0.85" };
            reply.hasApplyButton = true;
            reply.applyButtonText = "Apply Lighting Preset";
        } else if (lowerPrompt.find("script") != std::string::npos || lowerPrompt.find("code") != std::string::npos || lowerPrompt.find("rotate") != std::string::npos) {
            reply.text = "Generated a Luau rotation animator script with delta time damping.";
            reply.hasTools = true;
            reply.toolSummary = "2 Tool called";
            reply.toolDetail = "• Tool 1: ScriptEngine::CreateScript(\"Animator.luau\")\n"
                               "• Tool 2: ScriptEngine::BindToRenderStep()";
            reply.hasCodeBlock = true;
            reply.codeLines = { "+ local speed = 1.25", "+ part.CFrame = part.CFrame * CFrame.Angles(0, math.rad(speed), 0)" };
            reply.hasApplyButton = true;
            reply.applyButtonText = "Insert Script to Selection";
        } else {
            reply.text = "I analyzed the scene context and prepared the modifications for your request.";
            reply.hasTools = true;
            reply.toolSummary = "1 Tool called";
            reply.toolDetail = "• Tool 1: SceneManager::QueryContext()";
            reply.hasCodeBlock = true;
            reply.codeLines = { "+ SceneOptimization = true", "+ CacheState = OK" };
            reply.hasApplyButton = true;
            reply.applyButtonText = "Apply Scene Updates";
        }

        std::lock_guard<std::mutex> lock(s_chatMutex);
        s_messages.push_back(reply);
    }

    s_isWaitingForAI = false;
}

void AICopilotPanel::draw() {
    if (!EditorLayout::instance().showAICopilot) return;

    auto& T = NexusTheme::instance();
    ImGuiIO& io = ImGui::GetIO();
    ImGuiContext& g = *GImGui;

    bool pushedFont = false;

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_HiddenTabBar;
    ImGui::SetNextWindowClass(&window_class);
    float minW = Editor::UI::CalculateNodeMinTabWidth("AI Copilot");
    ImGui::SetNextWindowSizeConstraints(ImVec2(minW, 120.0f), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, T.bgDeep); // #0A0A0A
    ImGui::Begin("AI Copilot", &EditorLayout::instance().showAICopilot, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiDockNode* node = window->DockNode;

    // Hard-lock outer window scroll so panel shell never scrolls
    window->Scroll.y = 0.0f;
    window->Scroll.x = 0.0f;

    ImDrawList* parentDl = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    float winW = winSize.x;
    float winH = winSize.y;

    float topH = 30.0f;
    float searchH = s_showSearch ? 28.0f : 0.0f;
    float detH = 30.0f;
    float provH = 25.0f;
    float inH = 45.0f;

    // ─────────────────────────────────────────────────────────────────────────
    // 1. TOPBAR (Height = 30px, Fixed at Top)
    // ─────────────────────────────────────────────────────────────────────────
    ImVec2 topPos = winPos;

    if (node && node->Windows.Size > 1) {
        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
        Editor::UI::DrawSingleTabHeader("AI Copilot", "icon_ai_bold", 150.0f, ImGui::ColorConvertFloat4ToU32(T.accent), false);
    } else {
        parentDl->AddRectFilled(topPos, ImVec2(topPos.x + winW, topPos.y + topH), COL(T.bgPanel));
        parentDl->AddLine(ImVec2(topPos.x, topPos.y + topH - 1.0f), ImVec2(topPos.x + winW, topPos.y + topH - 1.0f), COL(T.border));

        // Invisible drag button for the top header
        float dragAreaW = std::max(winW - 80.0f, 40.0f);
        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
        ImGui::InvisibleButton("##TopBarDragArea", ImVec2(dragAreaW, topH));

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            if (node) {
                ImGui::DockContextQueueUndockWindow(GImGui, window);
            }
            ImGui::StartMouseMovingWindow(window);
            g.MovingWindow = window;
            float offsetX = g.IO.MouseClickedPos[0].x - window->Pos.x;
            g.ActiveIdClickOffset = ImVec2(offsetX, 0.0f);
        }

        // Left icon: icon_ai_bold (20x20 at x=10, y=5)
        ImTextureID aiIcon = IconRegistry::instance().get("icon_ai_bold");
        if (!aiIcon) aiIcon = IconRegistry::instance().get("icon_ai");
        if (aiIcon) {
            parentDl->AddImage(aiIcon, ImVec2(topPos.x + 10.0f, topPos.y + 5.0f), ImVec2(topPos.x + 30.0f, topPos.y + 25.0f), ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE);
        }

        // Left text: "Nexus Assistant" (12px, weight 600)
        pushedFont = false;
        if (io.Fonts->Fonts.Size > 1) {
            ImGui::PushFont(io.Fonts->Fonts[1]); // 12px font
            pushedFont = true;
        }
        ImVec2 titleSize = ImGui::CalcTextSize("Nexus Assistant");
        parentDl->AddText(ImVec2(topPos.x + 38.0f, topPos.y + (topH - titleSize.y)*0.5f), COL(T.textPrimary), "Nexus Assistant");
        if (pushedFont) ImGui::PopFont();

        // Right Action Buttons (Size 18x18, Y=6)
        float btnSize = 18.0f;
        float btnLocalY = 6.0f;

        // 1. More Button (icon_more)
        float moreLocalX = winW - 24.0f;
        ImGui::SetCursorPos(ImVec2(moreLocalX, btnLocalY));
        ImGui::InvisibleButton("##copilotMoreBtn", ImVec2(btnSize, btnSize));
        bool moreHov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) ImGui::OpenPopup("##CopilotOptionsPopup");
        ImTextureID moreIcon = IconRegistry::instance().get("icon_more");
        if (moreIcon) {
            parentDl->AddImage(moreIcon, ImVec2(winPos.x + moreLocalX, topPos.y + btnLocalY), ImVec2(winPos.x + moreLocalX + btnSize, topPos.y + btnLocalY + btnSize), ImVec2(0,0), ImVec2(1,1), moreHov ? COL(T.textPrimary) : COL(T.textMuted));
        }

        // 2. Search Button (icon_search)
        float searchLocalX = winW - 48.0f;
        ImGui::SetCursorPos(ImVec2(searchLocalX, btnLocalY));
        ImGui::InvisibleButton("##copilotSearchBtn", ImVec2(btnSize, btnSize));
        bool searchHov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) s_showSearch = !s_showSearch;
        ImTextureID searchIcon = IconRegistry::instance().get("icon_search");
        if (searchIcon) {
            parentDl->AddImage(searchIcon, ImVec2(winPos.x + searchLocalX, topPos.y + btnLocalY), ImVec2(winPos.x + searchLocalX + btnSize, topPos.y + btnLocalY + btnSize), ImVec2(0,0), ImVec2(1,1), (searchHov || s_showSearch) ? COL(T.textPrimary) : COL(T.textMuted));
        }

        // 3. Add / New Chat Button (icon_add)
        float addLocalX = winW - 72.0f;
        ImGui::SetCursorPos(ImVec2(addLocalX, btnLocalY));
        ImGui::InvisibleButton("##copilotAddBtn", ImVec2(btnSize, btnSize));
        bool addHov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            std::lock_guard<std::mutex> lock(s_chatMutex);
            s_messages.clear();
            strcpy_s(s_currentChatTitle, "New Session");
            s_messages.push_back({
                false,
                "Hello! I am Nexus Assistant. How can I help you optimize your scene or build shaders today?",
                "Now",
                false, "", "", false,
                false, "", "", false,
                false, {},
                false, "", false
            });
        }
        ImTextureID addIcon = IconRegistry::instance().get("icon_add");
        if (!addIcon) addIcon = IconRegistry::instance().get("icon_plus");
        if (addIcon) {
            parentDl->AddImage(addIcon, ImVec2(winPos.x + addLocalX, topPos.y + btnLocalY), ImVec2(winPos.x + addLocalX + btnSize, topPos.y + btnLocalY + btnSize), ImVec2(0,0), ImVec2(1,1), addHov ? COL(T.textPrimary) : COL(T.textMuted));
        }

        // Options Popup
        if (ImGui::BeginPopup("##CopilotOptionsPopup")) {
            if (ImGui::MenuItem("Clear Chat History")) {
                std::lock_guard<std::mutex> lock(s_chatMutex);
                s_messages.clear();
            }
            if (ImGui::MenuItem("API & Provider Settings")) {
                s_showSettings = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Attach Active Scene Context")) {
                auto sel = SelectionManager::instance().getSelected();
                if (sel) {
                    std::string attachNote = "Selected: " + sel->name + " (" + sel->getClassName() + ")";
                    strcpy_s(s_inputBuf, attachNote.c_str());
                }
            }
            ImGui::EndPopup();
        }
    }

    // Optional Search Row
    if (s_showSearch) {
        ImVec2 sPos = ImVec2(winPos.x, winPos.y + topH);
        parentDl->AddRectFilled(sPos, ImVec2(sPos.x + winW, sPos.y + searchH), COL(T.bgPanel));
        parentDl->AddLine(ImVec2(sPos.x, sPos.y + searchH - 1.0f), ImVec2(sPos.x + winW, sPos.y + searchH - 1.0f), COL(T.border));

        ImGui::SetCursorPos(ImVec2(8.0f, topH + 4.0f));
        ImGui::PushItemWidth(winW - 16.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, T.bgDeep);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::InputTextWithHint("##ChatFilterInput", "Search in conversation...", s_searchFilter, sizeof(s_searchFilter));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 2. CHATDETAILS STRIP (Height = 30px, Fixed below TopBar)
    // ─────────────────────────────────────────────────────────────────────────
    float detLocalY = topH + searchH;
    ImVec2 detPos = ImVec2(winPos.x, winPos.y + detLocalY);

    parentDl->AddRectFilled(detPos, ImVec2(detPos.x + winW, detPos.y + detH), COL(T.bgDeep));
    parentDl->AddLine(ImVec2(detPos.x, detPos.y + detH - 1.0f), ImVec2(detPos.x + winW, detPos.y + detH - 1.0f), COL(T.border));

    // Drag support on ChatDetails strip
    ImGui::SetCursorPos(ImVec2(0.0f, detLocalY));
    ImGui::InvisibleButton("##ChatDetailsDragArea", ImVec2(winW, detH));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        if (node) {
            ImGui::DockContextQueueUndockWindow(GImGui, window);
        }
        ImGui::StartMouseMovingWindow(window);
        g.MovingWindow = window;
        float offsetX = g.IO.MouseClickedPos[0].x - window->Pos.x;
        g.ActiveIdClickOffset = ImVec2(offsetX, 0.0f);
    }

    // Message Icon (18x18)
    ImTextureID chatIcon = IconRegistry::instance().get("icon_chat");
    if (!chatIcon) chatIcon = IconRegistry::instance().get("icon_chat_bold");
    if (chatIcon) {
        parentDl->AddImage(chatIcon, ImVec2(detPos.x + 10.0f, detPos.y + 6.0f), ImVec2(detPos.x + 28.0f, detPos.y + 24.0f), ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE);
    }

    // Chat Title Text ("Optimize Roughness")
    pushedFont = false;
    if (io.Fonts->Fonts.Size > 1) {
        ImGui::PushFont(io.Fonts->Fonts[1]); // 12px
        pushedFont = true;
    }
    ImVec2 topicSize = ImGui::CalcTextSize(s_currentChatTitle);
    parentDl->AddText(ImVec2(detPos.x + 38.0f, detPos.y + (detH - topicSize.y)*0.5f - 1.0f), COL(T.textPrimary), s_currentChatTitle);

    // Underline indicator: w=101px, h=2px, white 20% alpha, radius=1px
    float underW = std::max(topicSize.x, 101.0f);
    parentDl->AddRectFilled(ImVec2(detPos.x + 38.0f, detPos.y + 28.0f), ImVec2(detPos.x + 38.0f + underW, detPos.y + 30.0f), COLA(0xFFFFFF, 0.20f), 1.0f);

    if (pushedFont) ImGui::PopFont();

    // ─────────────────────────────────────────────────────────────────────────
    // 3. MESSAGES CONTAINER (Scrollable Body - Scissor-Clipped to Middle Area)
    // ─────────────────────────────────────────────────────────────────────────
    float topTotalH = topH + searchH + detH;
    float bottomTotalH = provH + inH;
    float scrollAreaH = winH - topTotalH - bottomTotalH;
    if (scrollAreaH < 20.0f) scrollAreaH = 20.0f;

    ImGui::SetCursorPos(ImVec2(0.0f, topTotalH));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, T.bgDeep);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 12.0f));

    ImGui::BeginChild("##MessagesScrollArea", ImVec2(winW, scrollAreaH), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
        // Use the CHILD window's own draw list so that all contents are properly clipped to this scroll area!
        ImDrawList* dl = ImGui::GetWindowDrawList();

        float contentW = ImGui::GetContentRegionAvail().x;
        // Horizontal padding for message bubbles from container edges
        float horizPad = 6.0f;
        float bubbleW = std::max(contentW - horizPad * 2.0f, 100.0f);

        std::lock_guard<std::mutex> lock(s_chatMutex);

        for (size_t msgIdx = 0; msgIdx < s_messages.size(); msgIdx++) {
            auto& msg = s_messages[msgIdx];

            // Search filtering if enabled
            if (s_searchFilter[0] != '\0') {
                std::string lMsg = msg.text;
                std::string lFlt = s_searchFilter;
                std::transform(lMsg.begin(), lMsg.end(), lMsg.begin(), ::tolower);
                std::transform(lFlt.begin(), lFlt.end(), lFlt.begin(), ::tolower);
                if (lMsg.find(lFlt) == std::string::npos) continue;
            }

            ImVec2 startItemPos = ImGui::GetCursorScreenPos();
            float bubbleX = startItemPos.x + horizPad;

            if (msg.isUser) {
                // ── USER MESSAGE (Message Container 1) ──────────────────────────
                pushedFont = false;
                if (io.Fonts->Fonts.Size > 2) {
                    ImGui::PushFont(io.Fonts->Fonts[2]); // 10px font
                    pushedFont = true;
                }

                float pad = 10.0f;
                float textMaxW = bubbleW - pad * 2.0f;
                ImVec2 textSize = ImGui::CalcTextSize(msg.text.c_str(), nullptr, false, textMaxW);
                float bubH = textSize.y + pad * 2.0f;

                ImDrawFlags uCorners = ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersBottomLeft | ImDrawFlags_RoundCornersBottomRight;
                dl->AddRectFilled(ImVec2(bubbleX, startItemPos.y), ImVec2(bubbleX + bubbleW, startItemPos.y + bubH), COL(T.bgCard), 10.0f, uCorners);
                dl->AddRect(ImVec2(bubbleX, startItemPos.y), ImVec2(bubbleX + bubbleW, startItemPos.y + bubH), COL(T.border), 10.0f, uCorners, 1.0f);

                // Render multiline wrapped user text directly via DrawList
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(bubbleX + pad, startItemPos.y + pad), COL(T.textPrimary), msg.text.c_str(), nullptr, textMaxW);

                if (pushedFont) ImGui::PopFont();

                // Details Row below user bubble (height 20px, right-aligned)
                ImVec2 detRowPos = ImVec2(startItemPos.x, startItemPos.y + bubH + 3.0f);
                float detRowH = 20.0f;
                float curX = bubbleX + bubbleW;

                // 1. Edit Button
                curX -= 16.0f;
                ImGui::SetCursorScreenPos(ImVec2(curX, detRowPos.y + 2.0f));
                ImGui::PushID((int)msgIdx * 10 + 3);
                ImGui::InvisibleButton("##editMsg", ImVec2(16.0f, 16.0f));
                bool editHov = ImGui::IsItemHovered();
                if (ImGui::IsItemClicked()) {
                    strcpy_s(s_inputBuf, msg.text.c_str());
                }
                ImGui::PopID();
                ImTextureID editIcon = IconRegistry::instance().get("icon_edit");
                if (editIcon) {
                    dl->AddImage(editIcon, ImVec2(curX, detRowPos.y + 2.0f), ImVec2(curX + 16.0f, detRowPos.y + 18.0f), ImVec2(0,0), ImVec2(1,1), editHov ? IM_COL32_WHITE : COL(T.textMuted));
                }

                // 2. Copy Button
                curX -= (16.0f + 5.0f);
                ImGui::SetCursorScreenPos(ImVec2(curX, detRowPos.y + 2.0f));
                ImGui::PushID((int)msgIdx * 10 + 2);
                ImGui::InvisibleButton("##copyMsg", ImVec2(16.0f, 16.0f));
                bool copyHov = ImGui::IsItemHovered();
                if (ImGui::IsItemClicked()) {
                    ImGui::SetClipboardText(msg.text.c_str());
                }
                ImGui::PopID();
                ImTextureID copyIcon = IconRegistry::instance().get("icon_clipboard");
                if (copyIcon) {
                    dl->AddImage(copyIcon, ImVec2(curX, detRowPos.y + 2.0f), ImVec2(curX + 16.0f, detRowPos.y + 18.0f), ImVec2(0,0), ImVec2(1,1), copyHov ? IM_COL32_WHITE : COL(T.textMuted));
                }

                // 3. Retry / Rotate Button
                curX -= (16.0f + 5.0f);
                ImGui::SetCursorScreenPos(ImVec2(curX, detRowPos.y + 2.0f));
                ImGui::PushID((int)msgIdx * 10 + 1);
                ImGui::InvisibleButton("##retryMsg", ImVec2(16.0f, 16.0f));
                bool retryHov = ImGui::IsItemHovered();
                if (ImGui::IsItemClicked() && !s_isWaitingForAI) {
                    std::thread(fetchAIResponse, msg.text).detach();
                }
                ImGui::PopID();
                ImTextureID retryIcon = IconRegistry::instance().get("icon_reverse_changes");
                if (!retryIcon) retryIcon = IconRegistry::instance().get("icon_rotate");
                if (retryIcon) {
                    dl->AddImage(retryIcon, ImVec2(curX, detRowPos.y + 2.0f), ImVec2(curX + 16.0f, detRowPos.y + 18.0f), ImVec2(0,0), ImVec2(1,1), retryHov ? IM_COL32_WHITE : COL(T.textMuted));
                }

                // Timestamp (e.g. "12:35")
                pushedFont = false;
                if (io.Fonts->Fonts.Size > 2) {
                    ImGui::PushFont(io.Fonts->Fonts[2]); // 10px font
                    pushedFont = true;
                }
                ImVec2 timeSize = ImGui::CalcTextSize(msg.timeLabel.c_str());
                curX -= (timeSize.x + 10.0f);
                dl->AddText(ImVec2(curX, detRowPos.y + (detRowH - timeSize.y)*0.5f), COL(T.textMuted), msg.timeLabel.c_str());
                if (pushedFont) ImGui::PopFont();

                // Submit entire user message container to ImGui layout
                float totalUserItemH = bubH + 3.0f + detRowH;
                ImGui::SetCursorScreenPos(startItemPos);
                ImGui::Dummy(ImVec2(contentW, totalUserItemH));

            } else {
                // ── ASSISTANT MESSAGE (Message Container 2) ─────────────────────
                float innerPad = 10.0f;
                float contentInsideW = bubbleW - innerPad * 2.0f;

                pushedFont = false;
                if (io.Fonts->Fonts.Size > 2) {
                    ImGui::PushFont(io.Fonts->Fonts[2]); // 10px
                    pushedFont = true;
                }

                // Yükseklik hesaplaması
                float totalBubH = innerPad;

                float thoughtRowH = msg.hasThought ? 20.0f : 0.0f;
                float thoughtExpandH = (msg.hasThought && msg.thoughtOpen) ? (ImGui::CalcTextSize(msg.thoughtDetail.c_str(), nullptr, false, contentInsideW - 12.0f).y + 14.0f) : 0.0f;
                totalBubH += thoughtRowH + thoughtExpandH;

                float toolRowH = msg.hasTools ? 20.0f : 0.0f;
                float toolExpandH = (msg.hasTools && msg.toolOpen) ? (ImGui::CalcTextSize(msg.toolDetail.c_str(), nullptr, false, contentInsideW - 12.0f).y + 14.0f) : 0.0f;
                totalBubH += toolRowH + toolExpandH;

                ImVec2 msgTextSz = ImGui::CalcTextSize(msg.text.c_str(), nullptr, false, contentInsideW);
                totalBubH += msgTextSz.y + 6.0f;

                float codeBlockH = 0.0f;
                if (msg.hasCodeBlock && !msg.codeLines.empty()) {
                    codeBlockH = 8.0f + (float)msg.codeLines.size() * 16.0f + 6.0f;
                    totalBubH += codeBlockH + 6.0f;
                }

                float applyBtnH = msg.hasApplyButton ? 26.0f : 0.0f;
                if (msg.hasApplyButton) {
                    totalBubH += applyBtnH + 4.0f;
                }
                totalBubH += innerPad;

                if (pushedFont) ImGui::PopFont();

                // 1. Glow Effect (box-shadow: 0px 0px 16px rgba(130, 217, 255, 0.25))
                ImDrawFlags aCorners = ImDrawFlags_RoundCornersTopRight | ImDrawFlags_RoundCornersBottomLeft | ImDrawFlags_RoundCornersBottomRight;
                dl->AddRectFilled(ImVec2(bubbleX - 3.0f, startItemPos.y - 3.0f), ImVec2(bubbleX + bubbleW + 3.0f, startItemPos.y + totalBubH + 3.0f), COLA(0x82D9FF, 0.08f), 12.0f, aCorners);
                dl->AddRectFilled(ImVec2(bubbleX - 1.0f, startItemPos.y - 1.0f), ImVec2(bubbleX + bubbleW + 1.0f, startItemPos.y + totalBubH + 1.0f), COLA(0x82D9FF, 0.20f), 11.0f, aCorners);

                // 2. Bubble Fill (#171717) & Outline (#82D9FF)
                dl->AddRectFilled(ImVec2(bubbleX, startItemPos.y), ImVec2(bubbleX + bubbleW, startItemPos.y + totalBubH), COL(T.bgCard), 10.0f, aCorners);
                dl->AddRect(ImVec2(bubbleX, startItemPos.y), ImVec2(bubbleX + bubbleW, startItemPos.y + totalBubH), COL(T.accent), 10.0f, aCorners, 1.0f);

                float curY = startItemPos.y + innerPad;

                // Render Thought Row
                if (msg.hasThought) {
                    ImGui::SetCursorScreenPos(ImVec2(bubbleX + innerPad, curY));
                    ImGui::PushID((int)msgIdx * 100 + 1);
                    ImGui::InvisibleButton("##thoughtToggle", ImVec2(contentInsideW, 18.0f));
                    bool tHov = ImGui::IsItemHovered();
                    if (ImGui::IsItemClicked()) msg.thoughtOpen = !msg.thoughtOpen;
                    ImGui::PopID();

                    ImTextureID chevIcon = IconRegistry::instance().get(msg.thoughtOpen ? "icon_chevron_down_bold" : "icon_chevron_right_bold");
                    if (!chevIcon) chevIcon = IconRegistry::instance().get("icon_chevron_down");
                    if (chevIcon) {
                        dl->AddImage(chevIcon, ImVec2(bubbleX + innerPad, curY + 3.0f), ImVec2(bubbleX + innerPad + 12.0f, curY + 15.0f), ImVec2(0,0), ImVec2(1,1), tHov ? IM_COL32_WHITE : COL(T.textMuted));
                    }

                    pushedFont = false;
                    if (io.Fonts->Fonts.Size > 2) {
                        ImGui::PushFont(io.Fonts->Fonts[2]); // 10px
                        pushedFont = true;
                    }
                    dl->AddText(ImVec2(bubbleX + innerPad + 16.0f, curY + 2.0f), tHov ? COL(T.textPrimary) : COL(T.textMuted), msg.thoughtTime.c_str());
                    if (pushedFont) ImGui::PopFont();

                    curY += 20.0f;

                    if (msg.thoughtOpen) {
                        float expH = thoughtExpandH - 4.0f;
                        dl->AddRectFilled(ImVec2(bubbleX + innerPad + 2.0f, curY), ImVec2(bubbleX + innerPad + contentInsideW, curY + expH), COL(T.bgDeep), 4.0f);
                        dl->AddRect(ImVec2(bubbleX + innerPad + 2.0f, curY), ImVec2(bubbleX + innerPad + contentInsideW, curY + expH), COL(T.border), 4.0f);

                        pushedFont = false;
                        if (io.Fonts->Fonts.Size > 2) {
                            ImGui::PushFont(io.Fonts->Fonts[2]);
                            pushedFont = true;
                        }
                        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(bubbleX + innerPad + 6.0f, curY + 4.0f), COL(T.textMuted), msg.thoughtDetail.c_str(), nullptr, contentInsideW - 12.0f);
                        if (pushedFont) ImGui::PopFont();

                        curY += thoughtExpandH;
                    }
                }

                // Render Tool Called Row
                if (msg.hasTools) {
                    ImGui::SetCursorScreenPos(ImVec2(bubbleX + innerPad, curY));
                    ImGui::PushID((int)msgIdx * 100 + 2);
                    ImGui::InvisibleButton("##toolToggle", ImVec2(contentInsideW, 18.0f));
                    bool toolHov = ImGui::IsItemHovered();
                    if (ImGui::IsItemClicked()) msg.toolOpen = !msg.toolOpen;
                    ImGui::PopID();

                    ImTextureID chevIcon = IconRegistry::instance().get(msg.toolOpen ? "icon_chevron_down_bold" : "icon_chevron_right_bold");
                    if (!chevIcon) chevIcon = IconRegistry::instance().get("icon_chevron_down");
                    if (chevIcon) {
                        dl->AddImage(chevIcon, ImVec2(bubbleX + innerPad, curY + 3.0f), ImVec2(bubbleX + innerPad + 12.0f, curY + 15.0f), ImVec2(0,0), ImVec2(1,1), toolHov ? IM_COL32_WHITE : COL(T.textMuted));
                    }

                    pushedFont = false;
                    if (io.Fonts->Fonts.Size > 2) {
                        ImGui::PushFont(io.Fonts->Fonts[2]); // 10px
                        pushedFont = true;
                    }
                    dl->AddText(ImVec2(bubbleX + innerPad + 16.0f, curY + 2.0f), toolHov ? COL(T.textPrimary) : COL(T.textMuted), msg.toolSummary.c_str());
                    if (pushedFont) ImGui::PopFont();

                    curY += 20.0f;

                    if (msg.toolOpen) {
                        float expH = toolExpandH - 4.0f;
                        dl->AddRectFilled(ImVec2(bubbleX + innerPad + 2.0f, curY), ImVec2(bubbleX + innerPad + contentInsideW, curY + expH), COL(T.bgDeep), 4.0f);
                        dl->AddRect(ImVec2(bubbleX + innerPad + 2.0f, curY), ImVec2(bubbleX + innerPad + contentInsideW, curY + expH), COL(T.border), 4.0f);

                        pushedFont = false;
                        if (io.Fonts->Fonts.Size > 2) {
                            ImGui::PushFont(io.Fonts->Fonts[2]);
                            pushedFont = true;
                        }
                        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(bubbleX + innerPad + 6.0f, curY + 4.0f), COL(T.textMuted), msg.toolDetail.c_str(), nullptr, contentInsideW - 12.0f);
                        if (pushedFont) ImGui::PopFont();

                        curY += toolExpandH;
                    }
                }

                // Render Message Body (multiline wrapped)
                pushedFont = false;
                if (io.Fonts->Fonts.Size > 2) {
                    ImGui::PushFont(io.Fonts->Fonts[2]); // 10px
                    pushedFont = true;
                }
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(bubbleX + innerPad, curY), COL(T.textPrimary), msg.text.c_str(), nullptr, contentInsideW);
                if (pushedFont) ImGui::PopFont();

                curY += msgTextSz.y + 6.0f;

                // Render CodeBlock
                if (msg.hasCodeBlock && !msg.codeLines.empty()) {
                    ImVec2 codeMin = ImVec2(bubbleX + innerPad, curY);
                    ImVec2 codeMax = ImVec2(bubbleX + innerPad + contentInsideW, curY + codeBlockH);

                    dl->AddRectFilled(codeMin, codeMax, COL(T.bgDeep), 5.0f);
                    dl->AddRect(codeMin, codeMax, COL(T.border), 5.0f, 0, 1.0f);

                    pushedFont = false;
                    if (io.Fonts->Fonts.Size > 2) {
                        ImGui::PushFont(io.Fonts->Fonts[2]); // 10px monospace
                        pushedFont = true;
                    }

                    float lineY = codeMin.y + 6.0f;
                    for (const auto& line : msg.codeLines) {
                        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(codeMin.x + 8.0f, lineY), COL(T.accentGreen), line.c_str());
                        lineY += 16.0f;
                    }

                    if (pushedFont) ImGui::PopFont();
                    curY += codeBlockH + 6.0f;
                }

                // Render Apply Button
                if (msg.hasApplyButton) {
                    ImVec2 btnMin = ImVec2(bubbleX + innerPad, curY);
                    ImVec2 btnMax = ImVec2(bubbleX + innerPad + contentInsideW, curY + 24.0f);

                    ImGui::SetCursorScreenPos(btnMin);
                    ImGui::PushID((int)msgIdx * 100 + 3);
                    ImGui::InvisibleButton("##applyBtn", ImVec2(contentInsideW, 24.0f));
                    bool btnHov = ImGui::IsItemHovered();
                    bool btnClk = ImGui::IsItemClicked();
                    if (btnClk) {
                        msg.isApplied = true;
                    }
                    ImGui::PopID();

                    ImU32 btnBgCol = msg.isApplied ? COL(T.accentGreen) : (btnHov ? COLA(0x82D9FF, 0.90f) : COL(T.accent));
                    dl->AddRectFilled(btnMin, btnMax, btnBgCol, 5.0f);

                    pushedFont = false;
                    if (io.Fonts->Fonts.Size > 2) {
                        ImGui::PushFont(io.Fonts->Fonts[2]);
                        pushedFont = true;
                    }

                    const char* bText = msg.isApplied ? "Applied Changes to Node Graph \xe2\x9c\x93" : msg.applyButtonText.c_str();
                    ImVec2 bTextSz = ImGui::CalcTextSize(bText);
                    dl->AddText(ImVec2(btnMin.x + (contentInsideW - bTextSz.x)*0.5f, btnMin.y + (24.0f - bTextSz.y)*0.5f), IM_COL32(23, 23, 23, 255), bText);

                    if (pushedFont) ImGui::PopFont();
                    curY += 28.0f;
                }

                // Details Row below Assistant Bubble (height 20px, left-aligned)
                ImVec2 detRowPos = ImVec2(startItemPos.x, startItemPos.y + totalBubH + 3.0f);
                float detRowH = 20.0f;
                float cX = bubbleX + 4.0f;

                // Copy button
                ImGui::SetCursorScreenPos(ImVec2(cX, detRowPos.y + 2.0f));
                ImGui::PushID((int)msgIdx * 100 + 4);
                ImGui::InvisibleButton("##copyAiMsg", ImVec2(16.0f, 16.0f));
                bool cAiHov = ImGui::IsItemHovered();
                if (ImGui::IsItemClicked()) {
                    ImGui::SetClipboardText(msg.text.c_str());
                }
                ImGui::PopID();
                ImTextureID copyIcon = IconRegistry::instance().get("icon_clipboard");
                if (copyIcon) {
                    dl->AddImage(copyIcon, ImVec2(cX, detRowPos.y + 2.0f), ImVec2(cX + 16.0f, detRowPos.y + 18.0f), ImVec2(0,0), ImVec2(1,1), cAiHov ? IM_COL32_WHITE : COL(T.textMuted));
                }
                cX += 16.0f + 8.0f;

                // Timestamp (e.g. "12:39")
                pushedFont = false;
                if (io.Fonts->Fonts.Size > 2) {
                    ImGui::PushFont(io.Fonts->Fonts[2]);
                    pushedFont = true;
                }
                dl->AddText(ImVec2(cX, detRowPos.y + (detRowH - 12.0f)*0.5f), COL(T.textMuted), msg.timeLabel.c_str());
                if (pushedFont) ImGui::PopFont();

                // Submit entire assistant message container to ImGui layout
                float totalAiItemH = totalBubH + 3.0f + detRowH;
                ImGui::SetCursorScreenPos(startItemPos);
                ImGui::Dummy(ImVec2(contentW, totalAiItemH));
            }
        }

        // Auto scroll to bottom
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 15.0f) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    // ─────────────────────────────────────────────────────────────────────────
    // 4. PROVIDERBAR (Height = 25px - Fixed at bottom)
    // ─────────────────────────────────────────────────────────────────────────
    float provLocalY = winH - bottomTotalH;
    ImVec2 provPos = ImVec2(winPos.x, winPos.y + provLocalY);

    parentDl->AddRectFilled(provPos, ImVec2(provPos.x + winW, provPos.y + provH), COL(T.bgDeep));
    parentDl->AddLine(ImVec2(provPos.x, provPos.y), ImVec2(provPos.x + winW, provPos.y), COL(T.border));

    pushedFont = false;
    if (io.Fonts->Fonts.Size > 2) {
        ImGui::PushFont(io.Fonts->Fonts[2]); // 10px
        pushedFont = true;
    }

    // Left Pill: "Provider: OpenRouter"
    std::string provStr = std::string("Provider: ") + s_providers[s_selectedProvider];
    ImVec2 provTextSz = ImGui::CalcTextSize(provStr.c_str());
    float provPillW = provTextSz.x + 14.0f;
    float provPillH = 16.0f;
    float provPillLocalX = 10.0f;
    float provPillLocalY = provLocalY + (provH - provPillH)*0.5f;
    ImVec2 provPillMin = ImVec2(winPos.x + provPillLocalX, winPos.y + provPillLocalY);
    ImVec2 provPillMax = ImVec2(winPos.x + provPillLocalX + provPillW, winPos.y + provPillLocalY + provPillH);

    ImGui::SetCursorPos(ImVec2(provPillLocalX, provPillLocalY));
    ImGui::InvisibleButton("##provPillBtn", ImVec2(provPillW, provPillH));
    bool provHov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) ImGui::OpenPopup("##ProviderSelectPopup");

    parentDl->AddRectFilled(provPillMin, provPillMax, COL(T.bgPanel), 3.0f);
    parentDl->AddRect(provPillMin, provPillMax, provHov ? COL(T.accent) : COL(T.border), 3.0f);
    parentDl->AddText(ImVec2(provPillMin.x + 7.0f, provPillMin.y + (provPillH - provTextSz.y)*0.5f), COL(T.textPrimary), provStr.c_str());

    if (ImGui::BeginPopup("##ProviderSelectPopup")) {
        for (int p = 0; p < IM_ARRAYSIZE(s_providers); p++) {
            if (ImGui::MenuItem(s_providers[p], nullptr, s_selectedProvider == p)) {
                s_selectedProvider = p;
            }
        }
        ImGui::EndPopup();
    }

    // Right Pill: "Model: Claude Sonnet 5"
    std::string modelStr = std::string("Model: ") + s_models[s_selectedModel];
    ImVec2 modelTextSz = ImGui::CalcTextSize(modelStr.c_str());
    float modelPillW = modelTextSz.x + 14.0f;
    float modelPillH = 16.0f;
    float modelPillLocalX = winW - 10.0f - modelPillW;
    float modelPillLocalY = provLocalY + (provH - modelPillH)*0.5f;
    ImVec2 modelPillMin = ImVec2(winPos.x + modelPillLocalX, winPos.y + modelPillLocalY);
    ImVec2 modelPillMax = ImVec2(winPos.x + modelPillLocalX + modelPillW, winPos.y + modelPillLocalY + modelPillH);

    ImGui::SetCursorPos(ImVec2(modelPillLocalX, modelPillLocalY));
    ImGui::InvisibleButton("##modelPillBtn", ImVec2(modelPillW, modelPillH));
    bool modelHov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) ImGui::OpenPopup("##ModelSelectPopup");

    parentDl->AddRectFilled(modelPillMin, modelPillMax, COL(T.bgPanel), 3.0f);
    parentDl->AddRect(modelPillMin, modelPillMax, modelHov ? COL(T.accent) : COL(T.border), 3.0f);
    parentDl->AddText(ImVec2(modelPillMin.x + 7.0f, modelPillMin.y + (modelPillH - modelTextSz.y)*0.5f), COL(T.textPrimary), modelStr.c_str());

    if (ImGui::BeginPopup("##ModelSelectPopup")) {
        for (int m = 0; m < IM_ARRAYSIZE(s_models); m++) {
            if (ImGui::MenuItem(s_models[m], nullptr, s_selectedModel == m)) {
                s_selectedModel = m;
            }
        }
        ImGui::EndPopup();
    }

    if (pushedFont) ImGui::PopFont();

    // ─────────────────────────────────────────────────────────────────────────
    // 5. INPUTBAR (Height = 45px - Fixed at bottom)
    // ─────────────────────────────────────────────────────────────────────────
    float inLocalY = winH - inH;
    ImVec2 inPos = ImVec2(winPos.x, winPos.y + inLocalY);

    parentDl->AddRectFilled(inPos, ImVec2(inPos.x + winW, inPos.y + inH), COL(T.bgPanel));
    parentDl->AddLine(ImVec2(inPos.x, inPos.y), ImVec2(inPos.x + winW, inPos.y), COL(T.border));

    float boxLocalX = 8.0f;
    float boxLocalY = inLocalY + 7.5f;
    float boxW = winW - 16.0f;
    float boxH = 30.0f;
    ImVec2 boxMin = ImVec2(winPos.x + boxLocalX, winPos.y + boxLocalY);
    ImVec2 boxMax = ImVec2(boxMin.x + boxW, boxMin.y + boxH);

    parentDl->AddRectFilled(boxMin, boxMax, COL(T.bgDeep), 10.0f);
    parentDl->AddRect(boxMin, boxMax, COL(T.border), 10.0f, 0, 1.0f);

    // Left button: icon_plus (14x14)
    float plusSize = 14.0f;
    float plusLocalX = boxLocalX + 8.0f;
    float plusLocalY = boxLocalY + (boxH - plusSize)*0.5f;

    ImGui::SetCursorPos(ImVec2(plusLocalX, plusLocalY));
    ImGui::InvisibleButton("##aiPlusAttach", ImVec2(plusSize, plusSize));
    bool plusHov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) ImGui::OpenPopup("##AttachContextPopup");

    ImTextureID plusIcon = IconRegistry::instance().get("icon_plus");
    if (!plusIcon) plusIcon = IconRegistry::instance().get("icon_add");
    if (plusIcon) {
        parentDl->AddImage(plusIcon, ImVec2(winPos.x + plusLocalX, winPos.y + plusLocalY), ImVec2(winPos.x + plusLocalX + plusSize, winPos.y + plusLocalY + plusSize), ImVec2(0,0), ImVec2(1,1), plusHov ? IM_COL32_WHITE : COL(T.textMuted));
    }

    if (ImGui::BeginPopup("##AttachContextPopup")) {
        if (ImGui::MenuItem("Attach Selected Object")) {
            auto sel = SelectionManager::instance().getSelected();
            if (sel) {
                std::string prompt = "Modify " + sel->name + ": ";
                strcpy_s(s_inputBuf, prompt.c_str());
            }
        }
        if (ImGui::MenuItem("Attach Active Material")) {
            strcpy_s(s_inputBuf, "Optimize active PBR shader parameters: ");
        }
        if (ImGui::MenuItem("Attach Luau Script Context")) {
            strcpy_s(s_inputBuf, "Generate Luau script for: ");
        }
        ImGui::EndPopup();
    }

    // Right send button: icon_play_bold (20x20)
    float playSize = 20.0f;
    float playLocalX = boxLocalX + boxW - playSize - 6.0f;
    float playLocalY = boxLocalY + (boxH - playSize)*0.5f;

    ImGui::SetCursorPos(ImVec2(playLocalX, playLocalY));
    ImGui::InvisibleButton("##aiSendPlay", ImVec2(playSize, playSize));
    bool playHov = ImGui::IsItemHovered();
    bool playClk = ImGui::IsItemClicked();

    ImTextureID playIcon = IconRegistry::instance().get("icon_play_bold");
    if (!playIcon) playIcon = IconRegistry::instance().get("icon_send_bold");
    if (playIcon) {
        parentDl->AddImage(playIcon, ImVec2(winPos.x + playLocalX, winPos.y + playLocalY), ImVec2(winPos.x + playLocalX + playSize, winPos.y + playLocalY + playSize), ImVec2(0,0), ImVec2(1,1), playHov ? COL(T.accent) : IM_COL32_WHITE);
    }

    // Center input field
    float inputLocalX = plusLocalX + plusSize + 6.0f;
    float inputW = playLocalX - 6.0f - inputLocalX;

    ImGui::SetCursorPos(ImVec2(inputLocalX, boxLocalY + 5.0f));
    ImGui::PushItemWidth(inputW);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, COL(T.textPrimary));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(1,1,1,0.40f)); // 40% placeholder

    pushedFont = false;
    if (io.Fonts->Fonts.Size > 2) {
        ImGui::PushFont(io.Fonts->Fonts[2]); // 10px
        pushedFont = true;
    }

    ImGui::InputTextWithHint("##AIChatInputMain", "Ask Nexus Assistant to modify scene or write script", s_inputBuf, sizeof(s_inputBuf));
    bool enterPressed = ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter);

    if (pushedFont) ImGui::PopFont();
    ImGui::PopStyleColor(5);
    ImGui::PopItemWidth();

    // Trigger send action
    if ((playClk || enterPressed) && s_inputBuf[0] != '\0' && !s_isWaitingForAI) {
        std::string userPrompt = s_inputBuf;
        auto now = std::time(nullptr);
        char timeBuf[32];
        std::strftime(timeBuf, sizeof(timeBuf), "%H:%M", std::localtime(&now));

        {
            std::lock_guard<std::mutex> lock(s_chatMutex);
            s_messages.push_back({
                true,
                userPrompt,
                timeBuf,
                false, "", "", false,
                false, "", "", false,
                false, {},
                false, "", false
            });
        }
        s_inputBuf[0] = '\0';
        std::thread(fetchAIResponse, userPrompt).detach();
    }

    // Settings Modal
    if (s_showSettings) {
        ImGui::OpenPopup("API & Provider Settings");
        s_showSettings = false;
    }
    if (ImGui::BeginPopupModal("API & Provider Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Configure OpenRouter API Key:");
        ImGui::InputText("##ApiKeyIn", s_apiKeyBuf, sizeof(s_apiKeyBuf), ImGuiInputTextFlags_Password);
        ImGui::Spacing();
        if (ImGui::Button("Save & Close", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Allow dragging the panel background to show docking targets
    if (g.MovingWindow == window && g.ActiveId == window->MoveId) {
        if (!g.DragDropActive) {
            ImVec2 old_offset = g.ActiveIdClickOffset;
            g.ActiveIdClickOffset = ImVec2(0.0f, 0.0f);
            ImGui::BeginDockableDragDropSource(window);
            g.ActiveIdClickOffset = old_offset;
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
