#include "console_panel.hpp"

#include <iostream>

#include "imgui.h"

namespace gcep
{
    namespace panel
    {
        ConsolePanel::ConsolePanel()
        {
            initConsole();
        }

        ConsolePanel::~ConsolePanel()
        {
            shutDownConsole();
        }

        void ConsolePanel::initConsole()
        {
            m_oldCout       = std::cout.rdbuf();
            m_consoleBuffer = std::make_unique<ImGuiConsoleBuffer>(m_oldCout, m_consoleItems);
            std::cout.rdbuf(m_consoleBuffer.get());
            std::cerr.rdbuf(m_consoleBuffer.get());
        }

        void ConsolePanel::shutDownConsole()
        {
            if (m_oldCout)
            {
                std::cout.rdbuf(m_oldCout);
                std::cerr.rdbuf(m_oldCout);
                m_oldCout = nullptr;
            }
            m_consoleBuffer.reset();
        }

        void ConsolePanel::draw()
        {
            static char inputBuffer[256];
            static bool autoScroll     = true;
            static bool scrollToBottom = false;

            ImGui::Begin("Console");
            {
                if (ImGui::SmallButton("Clear"))  m_consoleItems.clear();
                ImGui::SameLine();
                    bool copyToClipboard = ImGui::SmallButton("Copy");

                const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
                if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeight),
                    ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_HorizontalScrollbar))
                {
                    if (ImGui::BeginPopupContextWindow())
                    {
                        if (ImGui::Selectable("Clear")) m_consoleItems.clear();
                        ImGui::EndPopup();
                        }

                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
                        if (copyToClipboard) ImGui::LogToClipboard();

                    for (const auto& item : m_consoleItems)
                            ImGui::TextUnformatted(item.c_str());

                    if (copyToClipboard)    ImGui::LogFinish();
                    if (scrollToBottom || (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
                        ImGui::SetScrollHereY(1.0f);
                    scrollToBottom = false;
                    ImGui::PopStyleVar();
                }
                ImGui::EndChild();
                    ImGui::Separator();

                bool reclaimFocus = false;
                ImGuiInputTextFlags inputTextFlags =
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll;
                if (ImGui::InputText("Input", inputBuffer, IM_COUNTOF(inputBuffer), inputTextFlags))
                {
                    m_consoleItems.emplace_back(inputBuffer);
                    inputBuffer[0] = '\0';
                    reclaimFocus   = true;
                }
                ImGui::SetItemDefaultFocus();
                if (reclaimFocus) ImGui::SetKeyboardFocusHere(-1);
            }
            ImGui::End();
        }





    } // panel
} // gcep