#pragma once

// Internals
#include <Editor/UI_Panels/ipanel.hpp>

// Externals
#include <imgui.h>

// STL
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace gcep::panel
{
    /// @class ImGuiConsoleBuffer
    /// @brief Custom @c std::streambuf that tees all output into an ImGui console log.
    ///
    /// Wraps an existing stream buffer (typically @c std::cout's) and intercepts
    /// every character written to it. Characters are accumulated into a line buffer;
    /// on each newline the completed line is pushed into @c m_log and the original
    /// buffer receives the character unchanged, preserving normal terminal output.
    ///
    /// @par Usage
    /// Replace @c std::cout.rdbuf() with an instance of this class.
    /// Restore the original buffer on teardown via @c UiManager::shutdownConsole().
    class ImGuiConsoleBuffer : public std::streambuf
    {
    public:
        /// @brief Constructs the tee buffer.
        ///
        /// @param original  The stream buffer being wrapped. Must outlive this object.
        /// @param log       The string vector that receives completed lines.
        ImGuiConsoleBuffer(std::streambuf* original, std::vector<std::string>& log)
            : m_original(original), m_log(log)
        {}

    protected:
        /// @brief Intercepts a single character.
        ///
        /// On @c '\\n', flushes @c m_currentLine into @c m_log and resets it.
        /// All characters are forwarded to @c m_original unchanged.
        ///
        /// @param c  The character to process (as an @c int to match the @c streambuf contract).
        /// @returns  The result of forwarding @p c to the original buffer.
        int overflow(int c) override
        {
            if (c == '\n')
            {
                m_log.push_back(m_currentLine);
                m_currentLine.clear();
            }
            else
            {
                m_currentLine += static_cast<char>(c);
            }

            return m_original->sputc(static_cast<char>(c));
        }

    private:
        std::streambuf*           m_original;
        std::vector<std::string>& m_log;
        std::string               m_currentLine;
    };

    class ConsolePanel : public IPanel
    {
    public:
        ConsolePanel();
        ~ConsolePanel();

        void initConsole();

        void shutDownConsole();

        void draw() override;

    private:
        std::vector<std::string>               m_consoleItems;
        std::unique_ptr<ImGuiConsoleBuffer>   m_consoleBuffer;
        std::streambuf*                             m_oldCout = nullptr;
    };

} // namespace gcep::panel
