//
// Created by leoam on 10/03/2026.
//

#include "ContentBrowser.hpp"

#include <algorithm>

#include "font_awesome.hpp"
#include "Editor/Helpers.hpp"

gcep::editor::ContentBrowser::ContentBrowser(const std::filesystem::path &defaultPath) : folderSize(64)
{
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
    int colNb = (int)(ImGui::GetContentRegionAvail().x / folderSize);
    if (colNb < 1) colNb = 1;

    if (ImGui::BeginTable("Folders", colNb))
    {
        for (auto& entry : directories)
        {
            ImGui::TableNextColumn();
            std::string icon = std::filesystem::is_empty(entry) ? ICON_FA_FOLDER_O : ICON_FA_FOLDER_OPEN_O;
            std::string label = icon + std::string(" ") + entry.filename().string();
            bool isSelected = (m_selectedPath == entry);
            std::string id = "##" + label;
            ImVec2 pos = ImGui::GetCursorPos();
            if (ImGui::Selectable(id.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(64, 80)))
            {
                m_selectedPath = entry;
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    m_currentPath = entry;
                    refreshFolder();
                }
            }

            // Drag source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                std::string pathStr = entry.string();
                ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", pathStr.c_str(), pathStr.size() + 1);
                ImGui::Text("%s", entry.filename().string().c_str());
                ImGui::EndDragDropSource();
            }

            // Drop target
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                {
                    std::filesystem::path droppedPath = (const char*)payload->Data;
                    std::filesystem::rename(droppedPath, entry / droppedPath.filename());
                    refreshFolder();
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::SetCursorPos(pos);
            float cellWidth = 48.0f;
            float textWidth = ImGui::CalcTextSize(icon.c_str()).x;
            float offset = (cellWidth - textWidth) / 2.0f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
            ImGui::Text("%s", icon.c_str());
            ImGui::PopFont();

            textWidth = ImGui::CalcTextSize(entry.filename().string().c_str()).x;
            offset = (64.0f - textWidth) / 2.0f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
            ImGui::Text("%s", entry.filename().string().c_str());
        }

        for (auto& entry : files)
        {
            std::string icon = getFileIcon(entry);
            std::string label = icon + std::string(" ") + entry.filename().string();
            ImGui::TableNextColumn();
            bool isSelected = (m_selectedPath == entry);
            std::string id = "##" + label;
            ImVec2 pos = ImGui::GetCursorPos();
            if (ImGui::Selectable(id.c_str(), isSelected, 0, ImVec2(64, 80)))
            {
                m_selectedPath = entry;
            }

            // Drag source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                std::string pathStr = entry.string();
                ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", pathStr.c_str(), pathStr.size() + 1);
                ImGui::Text("%s", entry.filename().string().c_str());
                ImGui::EndDragDropSource();
            }

            ImGui::SetCursorPos(pos);
            float cellWidth = 64.0f;
            float textWidth = ImGui::CalcTextSize(icon.c_str()).x;
            float offset = (cellWidth - textWidth) / 2.0f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
            ImGui::Text("%s", icon.c_str());
            ImGui::PopFont();

            textWidth = ImGui::CalcTextSize(entry.filename().string().c_str()).x;
            offset = (64.0f - textWidth) / 2.0f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
            ImGui::Text("%s", entry.filename().string().c_str());
        }

        ImGui::EndTable();

        // Drop target zone vide
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
            {
                std::filesystem::path droppedPath = (const char*)payload->Data;
                std::filesystem::rename(droppedPath, m_currentPath / droppedPath.filename());
                refreshFolder();
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered())
            m_selectedPath.clear();
    }
}

void gcep::editor::ContentBrowser::refreshFolder()
{
    directories.clear();
    files.clear();
    for (auto& entry : std::filesystem::directory_iterator(m_currentPath))
    {
        if(entry.is_directory())
        {
            directories.push_back(entry.path());
        }
        else
        {
           files.push_back(entry.path());
        }
    }
    std::ranges::sort(directories, [](const std::filesystem::path& a, const std::filesystem::path& b){return a.filename().string() < b.filename().string();});
    std::ranges::sort(files, [](const std::filesystem::path& a, const std::filesystem::path& b)
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