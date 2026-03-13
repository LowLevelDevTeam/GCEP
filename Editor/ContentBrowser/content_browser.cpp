//
// Created by leoam on 10/03/2026.
//

#include "content_browser.hpp"

#include <algorithm>

#include "font_awesome.hpp"
#include "Editor/Helpers.hpp"
#include "Log/Log.hpp"

gcep::editor::ContentBrowser::ContentBrowser(const std::filesystem::path &defaultPath) : m_folderSize(64)
{
    m_rootPath = defaultPath;
    m_currentPath = defaultPath;
    refreshFolder();
}

void gcep::editor::ContentBrowser::render()
{
    ImGui::Begin("ContentDrawer");
    folderTable();
    ImGui::End();
}

void gcep::editor::ContentBrowser::folderTable()
{
    int colNb = std::max(1, (int)(ImGui::GetContentRegionAvail().x / m_folderSize));

    if (!ImGui::BeginTable("Folders", colNb)) return;

    bool needRefresh = false;

    renderBackButton();
    for (auto& entry : m_directories)
    {
        ImGui::TableNextColumn();
        renderDirectoryEntry(entry);

        if (m_currentPath == entry) { needRefresh = true; break; }
    }

    if (!needRefresh)
    {
        for (auto& entry : m_files)
        {
            ImGui::TableNextColumn();
            renderFileEntry(entry);
        }
    }

    ImGui::EndTable();

    if (renderDropTarget(m_currentPath))
        needRefresh = true;

    if (ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered())
        m_selectedPath.clear();

    if (needRefresh)
        refreshFolder();
}

void gcep::editor::ContentBrowser::refreshFolder()
{
    if (!std::filesystem::is_directory(m_currentPath))
    {
        m_currentPath = m_rootPath;
    }
    m_directories.clear();
    m_files.clear();
    for (auto& entry : std::filesystem::directory_iterator(m_currentPath))
    {
        if(entry.is_directory())
        {
            m_directories.push_back(entry.path());
        }
        else
        {
           m_files.push_back(entry.path());
        }
    }
    std::ranges::sort(m_directories, [](const std::filesystem::path& a, const std::filesystem::path& b){return a.filename().string() < b.filename().string();});
    std::ranges::sort(m_files, [](const std::filesystem::path& a, const std::filesystem::path& b)
    {
        bool  isExt = (a.extension().string()  < b.extension().string());
        bool isName = (a.filename().string()  < b.filename().string());
        if (a.extension() != b.extension())
        {
        return isExt;
        }
    return isName;
    });
}

void gcep::editor::ContentBrowser::renderCenteredIcon(const std::string& icon)
{
    float textWidth = ImGui::CalcTextSize(icon.c_str()).x;
    float offset = (64.0f - textWidth) / 2.0f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
    ImGui::Text("%s", icon.c_str());
    ImGui::PopFont();
}

void gcep::editor::ContentBrowser::renderCenteredLabel(const std::string& label, float cellWidth)
{
    float textWidth = ImGui::CalcTextSize(label.c_str()).x;
    float offset = (cellWidth - textWidth) / 2.0f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
    ImGui::Text("%s", label.c_str());
}

void gcep::editor::ContentBrowser::renderDragSource(const std::filesystem::path& entry)
{
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
    {
        std::string pathStr = entry.string();
        ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", pathStr.c_str(), pathStr.size() + 1);
        ImGui::Text("%s", entry.filename().string().c_str());
        ImGui::EndDragDropSource();
    }
}

bool gcep::editor::ContentBrowser::renderDropTarget(const std::filesystem::path& destination)
{
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
        {
            std::filesystem::path droppedPath = (const char*)payload->Data;
            std::filesystem::rename(droppedPath, destination / droppedPath.filename());
            ImGui::EndDragDropTarget();
            return true;
        }
        ImGui::EndDragDropTarget();
    }
    return false;
}

void gcep::editor::ContentBrowser::renderBackButton()
{
    if (m_currentPath == m_rootPath)
        return;

    ImGui::TableNextColumn();
    ImVec2 pos = ImGui::GetCursorPos();

    ImGui::Selectable("##Back", false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(64, 80));

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
    {
        m_currentPath = m_currentPath.parent_path();
        m_selectedPath.clear();
        refreshFolder();
    }

    ImGui::SetCursorPos(pos);
    renderCenteredIcon(ICON_FA_BACKWARD);
    renderCenteredLabel("..");
}

void gcep::editor::ContentBrowser::renderDirectoryEntry(const std::filesystem::path& entry)
{
    std::string icon  = std::filesystem::is_empty(entry) ? ICON_FA_FOLDER_O : ICON_FA_FOLDER_OPEN_O;
    bool isSelected   = (m_selectedPath == entry);
    std::string id    = "##dir_" + entry.string(); // plus safe que label seul
    ImVec2 pos        = ImGui::GetCursorPos();

    if (ImGui::Selectable(id.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(64, 80)))
    {
        m_selectedPath = entry;
        if (ImGui::IsMouseDoubleClicked(0))
        {
            m_currentPath = entry;
        }
    }

    renderDragSource(entry);
    if (renderDropTarget(entry))
    {
        return;
    }

    ImGui::SetCursorPos(pos);
    renderCenteredIcon(icon);
    renderCenteredLabel(entry.filename().string());
}

void gcep::editor::ContentBrowser::renderFileEntry(const std::filesystem::path& entry)
{
    std::string icon = getFileIcon(entry);
    bool isSelected  = (m_selectedPath == entry);
    std::string id   = "##file_" + entry.string();
    ImVec2 pos       = ImGui::GetCursorPos();

    if (ImGui::Selectable(id.c_str(), isSelected, 0, ImVec2(64, 80)))
        m_selectedPath = entry;

    renderDragSource(entry);

    ImGui::SetCursorPos(pos);
    renderCenteredIcon(icon);
    renderCenteredLabel(entry.filename().string());
}