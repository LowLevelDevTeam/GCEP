#include "project_browser_panel.hpp"

#include <Editor/ProjectLoader/project_loader.hpp>
#include <Editor/UI_Panels/editor_context.hpp>
#include <font_awesome.hpp>
#include <imgui.h>

#include <algorithm>

namespace gcep::panel
{
    // ─────────────────────────────────────────────────────────────────────────
    // Constants
    // ─────────────────────────────────────────────────────────────────────────

    static constexpr float CARD_W      = 160.0f;
    static constexpr float CARD_H      = 150.0f;
    static constexpr float THUMB_H     = 90.0f;
    static constexpr float CARD_PAD    = 12.0f;
    static constexpr float CARD_RADIUS = 6.0f;

    // ─────────────────────────────────────────────────────────────────────────
    // Constructor / refresh
    // ─────────────────────────────────────────────────────────────────────────

    ProjectBrowserPanel::ProjectBrowserPanel(std::filesystem::path rootPath)
        : m_rootPath(std::move(rootPath))
        , m_browserPath(std::filesystem::current_path())
    {
        refresh();
        refreshBrowser();
    }

    void ProjectBrowserPanel::refresh()
    {
        m_projects.clear();
        m_selectedIndex = -1;

        auto scanDir = [&](const std::filesystem::path& dir)
        {
            if (!std::filesystem::is_directory(dir)) return;
            for (const auto& entry : std::filesystem::directory_iterator(dir))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".gcepproj")
                {
                    m_projects.push_back({
                        entry.path().stem().string(),
                        entry.path()
                    });
                }
            }
        };

        scanDir(m_rootPath);

        if (std::filesystem::is_directory(m_rootPath))
        {
            for (const auto& sub : std::filesystem::directory_iterator(m_rootPath))
            {
                if (sub.is_directory())
                    scanDir(sub.path());
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // draw
    // ─────────────────────────────────────────────────────────────────────────

    void ProjectBrowserPanel::draw()
    {
        ImGui::Begin("Projects");

        // ── Top bar ───────────────────────────────────────────────────────────
        if (ImGui::Button((std::string(ICON_FA_REFRESH) + " Refresh").c_str()))
            refresh();
        ImGui::SameLine();
        ImGui::TextDisabled("%zu project(s)", m_projects.size());

        ImGui::Separator();
        ImGui::Spacing();

        if (m_projects.empty())
        {
            ImGui::TextDisabled("No .gcepproj files found in:\n%s", m_rootPath.string().c_str());
            ImGui::End();
            return;
        }

        // ── Grid ──────────────────────────────────────────────────────────────
        const ImGuiIO& io       = ImGui::GetIO();
        ImFont*        bigFont  = (io.Fonts->Fonts.Size > 1) ? io.Fonts->Fonts[1] : io.Fonts->Fonts[0];
        ImFont*        textFont = io.Fonts->Fonts[0];

        const float  availW  = ImGui::GetContentRegionAvail().x;
        const int    columns = std::max(1, (int)((availW + CARD_PAD) / (CARD_W + CARD_PAD)));
        ImDrawList*  dl      = ImGui::GetWindowDrawList();

        for (int i = 0; i < (int)m_projects.size(); ++i)
        {
            const int col = i % columns;
            if (col > 0)
                ImGui::SameLine(0.0f, CARD_PAD);

            ImGui::PushID(i);

            const ImVec2 p0      = ImGui::GetCursorScreenPos();
            const bool   selected = (m_selectedIndex == i);
            const bool   hovered  = ImGui::IsMouseHoveringRect(p0, { p0.x + CARD_W, p0.y + CARD_H });

            // ── Background ────────────────────────────────────────────────────
            const ImU32 bgThumb = IM_COL32(30,  30,  45,  255);
            const ImU32 bgCard  = hovered  ? IM_COL32(55,  55,  75,  255)
                                           : IM_COL32(42,  42,  58,  255);
            const ImU32 border  = selected ? IM_COL32(90, 140, 240, 255)
                                : hovered  ? IM_COL32(80,  80, 110, 255)
                                           : IM_COL32(60,  60,  80, 200);

            dl->AddRectFilled(p0, { p0.x + CARD_W, p0.y + CARD_H },  bgCard,  CARD_RADIUS);
            dl->AddRectFilled(p0, { p0.x + CARD_W, p0.y + THUMB_H }, bgThumb, CARD_RADIUS);
            // Square bottom corners for thumb so it blends with card body
            dl->AddRectFilled({ p0.x, p0.y + THUMB_H - CARD_RADIUS },
                              { p0.x + CARD_W, p0.y + THUMB_H },
                              bgThumb);
            dl->AddRect(p0, { p0.x + CARD_W, p0.y + CARD_H }, border, CARD_RADIUS, 0, selected ? 2.0f : 1.0f);

            // ── Big icon centred in thumbnail ─────────────────────────────────
            const char*  icon     = ICON_FA_GAMEPAD;
            const float  iconSize = 32.0f;
            const ImVec2 iconDim  = bigFont->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, icon);
            const ImVec2 iconPos  = {
                p0.x + (CARD_W  - iconDim.x) * 0.5f,
                p0.y + (THUMB_H - iconDim.y) * 0.5f
            };
            const ImU32 iconColor = selected ? IM_COL32(130, 170, 255, 230)
                                             : IM_COL32( 90, 120, 200, 180);
            dl->AddText(bigFont, iconSize, iconPos, iconColor, icon);

            // ── Project name ──────────────────────────────────────────────────
            const float  fontSize = ImGui::GetFontSize();
            const ImVec2 namePos  = { p0.x + 8.0f, p0.y + THUMB_H + 7.0f };
            dl->AddText(textFont, fontSize, namePos,
                        IM_COL32(225, 225, 225, 255),
                        m_projects[i].name.c_str());

            // ── Parent folder (small, gray) ───────────────────────────────────
            const std::string parentStr = m_projects[i].path.parent_path().filename().string();
            const ImVec2 pathPos = { namePos.x, namePos.y + fontSize + 3.0f };
            dl->AddText(textFont, fontSize * 0.80f, pathPos,
                        IM_COL32(120, 120, 140, 200),
                        parentStr.c_str());

            // ── Hit area ──────────────────────────────────────────────────────
            ImGui::InvisibleButton("##card", { CARD_W, CARD_H });

            if (ImGui::IsItemClicked())
                m_selectedIndex = i;

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                pl::ProjectLoader::instance().openProject(m_projects[i].path);
                editor::EditorContext::get().reloadRequested = true;
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", m_projects[i].path.string().c_str());

            ImGui::PopID();

            // New row gap
            if (col == columns - 1)
                ImGui::Dummy({ 0.0f, CARD_PAD });
        }

        // ── File browser ──────────────────────────────────────────────────────
        ImGui::Spacing();
        ImGui::SeparatorText("Browse for a project");
        drawBrowser();

        ImGui::End();
    }

    // ─────────────────────────────────────────────────────────────────────────
    // File browser helpers
    // ─────────────────────────────────────────────────────────────────────────

    void ProjectBrowserPanel::refreshBrowser()
    {
        m_browserDirs.clear();
        m_browserProjFiles.clear();

        if (!std::filesystem::is_directory(m_browserPath))
            m_browserPath = std::filesystem::current_path();

        for (const auto& entry : std::filesystem::directory_iterator(m_browserPath))
        {
            if (entry.is_directory())
                m_browserDirs.push_back(entry.path());
            else if (entry.path().extension() == ".gcepproj")
                m_browserProjFiles.push_back(entry.path());
        }

        std::ranges::sort(m_browserDirs,      [](const auto& a, const auto& b){ return a.filename() < b.filename(); });
        std::ranges::sort(m_browserProjFiles, [](const auto& a, const auto& b){ return a.filename() < b.filename(); });
    }

    void ProjectBrowserPanel::drawBrowserBackButton()
    {
        if (m_browserPath.has_parent_path() && m_browserPath != m_browserPath.root_path())
        {
            ImGui::TableNextColumn();
            const ImVec2 pos = ImGui::GetCursorPos();
            if (ImGui::Selectable("##BrowserBack", false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(64, 80)))
            {
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    m_browserPath = m_browserPath.parent_path();
                    m_browserSelected.clear();
                    refreshBrowser();
                }
            }
            ImGui::SetCursorPos(pos);

            float textWidth = ImGui::CalcTextSize(ICON_FA_BACKWARD).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (64.0f - textWidth) / 2.0f);
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
            ImGui::Text("%s", ICON_FA_BACKWARD);
            ImGui::PopFont();

            float labelWidth = ImGui::CalcTextSize("..").x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (64.0f - labelWidth) / 2.0f);
            ImGui::Text("..");
        }
    }

    void ProjectBrowserPanel::drawBrowserEntry(const std::filesystem::path& entry, bool isDir)
    {
        const char* icon     = isDir ? ICON_FA_FOLDER_O : ICON_FA_GAMEPAD;
        const bool  selected = (m_browserSelected == entry);
        const std::string id = std::string(isDir ? "##bdir_" : "##bfile_") + entry.string();
        const ImVec2 pos     = ImGui::GetCursorPos();

        if (ImGui::Selectable(id.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(64, 80)))
        {
            m_browserSelected = entry;
            if (ImGui::IsMouseDoubleClicked(0))
            {
                if (isDir)
                {
                    m_browserPath = entry;
                    m_browserSelected.clear();
                    refreshBrowser();
                }
                else
                {
                    pl::ProjectLoader::instance().openProject(entry);
                    editor::EditorContext::get().reloadRequested = true;
                }
            }
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", entry.string().c_str());

        ImGui::SetCursorPos(pos);

        float textWidth = ImGui::CalcTextSize(icon).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (64.0f - textWidth) / 2.0f);
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        ImGui::Text("%s", icon);
        ImGui::PopFont();

        const std::string label = entry.filename().string();
        float labelWidth = ImGui::CalcTextSize(label.c_str()).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (64.0f - labelWidth) / 2.0f));
        ImGui::TextUnformatted(label.c_str());
    }

    void ProjectBrowserPanel::drawBrowser()
    {
        // Current path display
        ImGui::TextDisabled("%s", m_browserPath.string().c_str());
        ImGui::Spacing();

        const int colNb = std::max(1, (int)(ImGui::GetContentRegionAvail().x / 64.0f));
        if (!ImGui::BeginTable("##BrowserTable", colNb))
            return;

        drawBrowserBackButton();

        for (const auto& dir : m_browserDirs)
        {
            ImGui::TableNextColumn();
            drawBrowserEntry(dir, true);
        }
        for (const auto& proj : m_browserProjFiles)
        {
            ImGui::TableNextColumn();
            drawBrowserEntry(proj, false);
        }

        ImGui::EndTable();

        if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
            m_browserSelected.clear();
    }

} // namespace gcep::panel
