#include "content_browser.hpp"

// Internals
#include <Editor/Helpers.hpp>
#include <Editor/UI_Panels/editor_context.hpp>
#include <Log/log.hpp>

// Externals
#include <font_awesome.hpp>

// STL
#include <algorithm>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace gcep::editor
{
    ContentBrowser::ContentBrowser(const std::filesystem::path &defaultPath) : m_folderSize(64)
    {
        m_rootPath = defaultPath;
        m_currentPath = defaultPath;
        refreshFolder();
    }

    void ContentBrowser::draw()
    {
        ImGui::Begin("ContentDrawer");

        drawToolbar();
        ImGui::Separator();

        // ── Left: folder tree ─────────────────────────────────────────────────
        ImGui::BeginChild("##cb_tree", ImVec2(200.f, 0.f), false,
            ImGuiWindowFlags_HorizontalScrollbar);
        drawFolderTree();
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // ── Right: breadcrumb + content ───────────────────────────────────────
        ImGui::BeginChild("##cb_content", ImVec2(0.f, 0.f), false);
        drawBreadcrumb();
        ImGui::Separator();
        if (m_viewMode == ViewMode::Grid)
            folderTable();
        else
            folderList();
        ImGui::EndChild();

        ImGui::End();
    }

    void ContentBrowser::drawToolbar()
    {
        // Vue grille
        const bool gridActive = (m_viewMode == ViewMode::Grid);
        if (gridActive) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::SmallButton(ICON_FA_TH))
            m_viewMode = ViewMode::Grid;
        if (gridActive) ImGui::PopStyleColor();

        ImGui::SameLine();

        // Vue liste
        const bool listActive = (m_viewMode == ViewMode::List);
        if (listActive) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::SmallButton(ICON_FA_LIST))
            m_viewMode = ViewMode::List;
        if (listActive) ImGui::PopStyleColor();
    }

    void ContentBrowser::drawFolderTree()
    {
        drawFolderTreeNode(m_rootPath);
    }

    void ContentBrowser::drawFolderTreeNode(const std::filesystem::path& path)
    {
        std::error_code ec;

        // Collect subdirectories (sorted)
        std::vector<std::filesystem::path> subDirs;
        for (const auto& entry : std::filesystem::directory_iterator(path, ec))
            if (entry.is_directory()) subDirs.push_back(entry.path());
        std::ranges::sort(subDirs);

        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        if (m_currentPath == path)
            flags |= ImGuiTreeNodeFlags_Selected;

        if (subDirs.empty())
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        // Auto-expand ancestors of the current path
        const auto pathStr    = path.string();
        const auto currentStr = m_currentPath.string();
        if (currentStr.size() > pathStr.size() &&
            currentStr.substr(0, pathStr.size()) == pathStr)
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);

        const std::string icon  = subDirs.empty() ? ICON_FA_FOLDER : ICON_FA_FOLDER_OPEN;
        const std::string label = icon + "  " + path.filename().string()
                                + "##" + pathStr;

        const bool open = ImGui::TreeNodeEx(label.c_str(), flags);

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            m_currentPath = path;
            m_selectedPath.clear();
            refreshFolder();
        }

        if (open && !subDirs.empty())
        {
            for (const auto& sub : subDirs)
                drawFolderTreeNode(sub);
            ImGui::TreePop();
        }
    }

    void ContentBrowser::drawBreadcrumb()
    {
        // Build relative path segments from root
        std::filesystem::path rel = std::filesystem::relative(m_currentPath, m_rootPath);
        std::vector<std::filesystem::path> segments;
        segments.push_back(m_rootPath);
        std::filesystem::path accumulated = m_rootPath;
        for (const auto& part : rel)
        {
            if (part == ".") continue;
            accumulated /= part;
            segments.push_back(accumulated);
        }

        for (std::size_t i = 0; i < segments.size(); ++i)
        {
            if (i > 0) { ImGui::SameLine(); ImGui::TextDisabled(">"); ImGui::SameLine(); }
            const std::string name = (i == 0) ? segments[i].filename().string() : segments[i].filename().string();
            const bool isCurrent = (segments[i] == m_currentPath);
            if (isCurrent)
            {
                ImGui::TextUnformatted(name.c_str());
            }
            else
            {
                if (ImGui::SmallButton(name.c_str()))
                {
                    m_currentPath = segments[i];
                    m_selectedPath.clear();
                    refreshFolder();
                }
            }
        }
    }

    void ContentBrowser::folderList()
    {
        constexpr ImGuiTableFlags flags =
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY       | ImGuiTableFlags_SizingStretchProp;

        if (!ImGui::BeginTable("FolderList", 3, flags))
            return;

        ImGui::TableSetupColumn("Name",     ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type",     ImGuiTableColumnFlags_WidthFixed, 80.f);
        ImGui::TableSetupColumn("Size",     ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableHeadersRow();

        bool needRefresh = false;

        // Back button
        if (m_currentPath != m_rootPath)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable((std::string(ICON_FA_BACKWARD) + "  ..##listback").c_str(),
                false, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns))
            {
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    m_currentPath = m_currentPath.parent_path();
                    m_selectedPath.clear();
                    needRefresh = true;
                }
            }
        }

        // Directories
        for (const auto& entry : m_directories)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const bool sel = (m_selectedPath == entry);
            const std::string label = std::string(ICON_FA_FOLDER) + "  " + entry.filename().string() + "##d" + entry.string();
            if (ImGui::Selectable(label.c_str(), sel,
                ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns))
            {
                m_selectedPath = entry;
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    m_currentPath = entry;
                    needRefresh = true;
                }
            }
            drawItemContextMenu(entry);
            ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Folder");
            ImGui::TableSetColumnIndex(2); ImGui::TextDisabled("—");
        }

        // Files
        for (const auto& entry : m_files)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const bool sel = (m_selectedPath == entry);
            const std::string icon  = getFileIcon(entry);
            const std::string label = icon + "  " + entry.filename().string() + "##f" + entry.string();
            if (ImGui::Selectable(label.c_str(), sel,
                ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns))
                m_selectedPath = entry;
            renderDragSource(entry);
            drawItemContextMenu(entry);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("%s", entry.extension().string().c_str());

            ImGui::TableSetColumnIndex(2);
            std::error_code ec;
            const auto size = std::filesystem::file_size(entry, ec);
            if (!ec)
            {
                if (size < 1024)             ImGui::TextDisabled("%llu B",    size);
                else if (size < 1024 * 1024) ImGui::TextDisabled("%.1f KB",  size / 1024.f);
                else                         ImGui::TextDisabled("%.1f MB",  size / (1024.f * 1024.f));
            }
        }

        ImGui::EndTable();

        drawBackgroundContextMenu();
        drawRenameModal();
        drawCreateModal();
        processDroppedFiles();

        if (!m_pendingDelete.empty())
        {
            if (std::filesystem::is_directory(m_pendingDelete))
                std::filesystem::remove_all(m_pendingDelete);
            else
                std::filesystem::remove(m_pendingDelete);
            if (m_selectedPath == m_pendingDelete)
                m_selectedPath.clear();
            m_pendingDelete.clear();
            needRefresh = true;
        }

        if (needRefresh)
            refreshFolder();
    }

    void ContentBrowser::folderTable()
    {
        int colNb = std::max(1, (int) (ImGui::GetContentRegionAvail().x / m_folderSize));

        if (!ImGui::BeginTable("Folders", colNb))
            return;

        bool needRefresh = false;

        renderBackButton();
        for (auto &entry: m_directories)
        {
            ImGui::TableNextColumn();
            renderDirectoryEntry(entry);

            if (m_currentPath == entry)
            {
                needRefresh = true;
                break;
            }
        }

        if (!needRefresh)
        {
            for (auto &entry: m_files)
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

        drawBackgroundContextMenu();
        drawRenameModal();
        drawCreateModal();
        processDroppedFiles();

        if (!m_pendingDelete.empty())
        {
            if (std::filesystem::is_directory(m_pendingDelete))
                std::filesystem::remove_all(m_pendingDelete);
            else
                std::filesystem::remove(m_pendingDelete);
            if (m_selectedPath == m_pendingDelete)
                m_selectedPath.clear();
            m_pendingDelete.clear();
            needRefresh = true;
        }

        if (needRefresh)
            refreshFolder();
    }

    void ContentBrowser::refreshFolder()
    {
        if (!std::filesystem::is_directory(m_currentPath))
        {
            m_currentPath = m_rootPath;
        }
        m_directories.clear();
        m_files.clear();
        for (auto &entry: std::filesystem::directory_iterator(m_currentPath))
        {
            if (entry.is_directory())
            {
                m_directories.push_back(entry.path());
            }
            else
            {
                m_files.push_back(entry.path());
            }
        }
        std::ranges::sort(m_directories, [](const std::filesystem::path &a, const std::filesystem::path &b)
        {
            return a.filename().string() < b.filename().string();
        });
        std::ranges::sort(m_files, [](const std::filesystem::path &a, const std::filesystem::path &b)
        {
            bool isExt = (a.extension().string() < b.extension().string());
            bool isName = (a.filename().string() < b.filename().string());
            if (a.extension() != b.extension())
            {
                return isExt;
            }
            return isName;
        });
    }

    void ContentBrowser::renderCenteredIcon(const std::string &icon)
    {
        float textWidth = ImGui::CalcTextSize(icon.c_str()).x;
        float offset = (64.0f - textWidth) / 2.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        ImGui::Text("%s", icon.c_str());
        ImGui::PopFont();
    }

    void ContentBrowser::renderCenteredLabel(const std::string &label, float cellWidth)
    {
        float textWidth = ImGui::CalcTextSize(label.c_str()).x;
        float offset = (cellWidth - textWidth) / 2.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
        ImGui::Text("%s", label.c_str());
    }

    void ContentBrowser::renderDragSource(const std::filesystem::path &entry)
    {
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            std::string pathStr = entry.string();
            ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", pathStr.c_str(), pathStr.size() + 1);
            ImGui::Text("%s", entry.filename().string().c_str());
            ImGui::EndDragDropSource();
        }
    }

    bool ContentBrowser::renderDropTarget(const std::filesystem::path &destination)
    {
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
            {
                std::filesystem::path droppedPath = (const char *) payload->Data;
                std::filesystem::rename(droppedPath, destination / droppedPath.filename());
                ImGui::EndDragDropTarget();
                return true;
            }
            ImGui::EndDragDropTarget();
        }
        return false;
    }

    void ContentBrowser::renderBackButton()
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

    void ContentBrowser::createFolder(const std::filesystem::path& parent, const std::string& name)
    {
        std::filesystem::path newPath = parent / name;
        int counter = 1;
        while (std::filesystem::exists(newPath))
            newPath = parent / (name + " " + std::to_string(counter++));
        std::filesystem::create_directory(newPath);
        refreshFolder();
    }

    void ContentBrowser::createFile(const std::filesystem::path& parent, const std::string& name, const std::string& extension)
    {
        std::filesystem::path newPath = parent / (name + extension);
        int counter = 1;
        while (std::filesystem::exists(newPath))
            newPath = parent / (name + " " + std::to_string(counter++) + extension);
        std::ofstream(newPath).close();
        refreshFolder();
    }

    void ContentBrowser::requestCreateFolder(const std::filesystem::path& parent)
    {
        m_createParent    = parent;
        m_createExtension = "";
        m_createNameBuffer[0] = '\0';
        m_openCreateModal = true;
    }

    void ContentBrowser::requestCreateFile(const std::filesystem::path& parent, const std::string& extension)
    {
        m_createParent    = parent;
        m_createExtension = extension;
        m_createNameBuffer[0] = '\0';
        m_openCreateModal = true;
    }

    void ContentBrowser::drawCreateModal()
    {
        if (m_openCreateModal)
        {
            ImGui::OpenPopup("Create##modal");
            m_openCreateModal = false;
        }

        if (ImGui::BeginPopupModal("Create##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            const bool isFolder = m_createExtension.empty();
            ImGui::Text(isFolder ? "Folder name:" : "File name:");
            ImGui::SetNextItemWidth(300.f);

            const bool confirmed = ImGui::InputText("##createname", m_createNameBuffer,
                sizeof(m_createNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::SameLine();
            if (!m_createExtension.empty())
                ImGui::TextDisabled("%s", m_createExtension.c_str());

            ImGui::Spacing();
            const bool nameValid = m_createNameBuffer[0] != '\0';

            if (!nameValid) ImGui::BeginDisabled();
            if (ImGui::Button("Create") || (confirmed && nameValid))
            {
                if (isFolder)
                    createFolder(m_createParent, m_createNameBuffer);
                else
                    createFile(m_createParent, m_createNameBuffer, m_createExtension);
                ImGui::CloseCurrentPopup();
            }
            if (!nameValid) ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    void ContentBrowser::processDroppedFiles()
    {
        auto& ctx = editor::EditorContext::get();
        if (ctx.droppedPaths.empty()) return;
        if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)) return;

        for (const auto& src : ctx.droppedPaths)
        {
            const std::filesystem::path dst = m_currentPath / src.filename();
            std::error_code ec;
            std::filesystem::copy(src, dst,
                std::filesystem::copy_options::recursive |
                std::filesystem::copy_options::skip_existing, ec);
            if (ec)
                Log::info("Drop copy failed: " + ec.message());
        }
        ctx.droppedPaths.clear();
        refreshFolder();
    }

    static void drawNewFileSubmenu(const std::function<void(const std::string&)>& onRequest)
    {
        if (ImGui::BeginMenu((std::string(ICON_FA_FILE) + " New File").c_str()))
        {
            if (ImGui::MenuItem("Scene (.gcmap)"))  onRequest(".gcmap");
            if (ImGui::MenuItem("Script (.cpp)"))   onRequest(".cpp");
            if (ImGui::MenuItem("Text (.txt)"))     onRequest(".txt");
            ImGui::EndMenu();
        }
    }

    void ContentBrowser::drawItemContextMenu(const std::filesystem::path& path)
    {
        const std::string popupId = "ctx_item_" + path.string();
        const bool isDir = std::filesystem::is_directory(path);

        if (ImGui::BeginPopupContextItem(popupId.c_str()))
        {
            if (isDir)
            {
                if (ImGui::MenuItem((std::string(ICON_FA_FOLDER) + " New Folder here").c_str()))
                    requestCreateFolder(path);
                drawNewFileSubmenu([&](const std::string& ext) { requestCreateFile(path, ext); });
                ImGui::Separator();
            }

            if (ImGui::MenuItem((std::string(ICON_FA_PENCIL) + " Rename").c_str()))
            {
                m_renamePath = path;
                const std::string name = path.filename().string();
                name.copy(m_renameBuffer, sizeof(m_renameBuffer) - 1);
                m_renameBuffer[name.size()] = '\0';
                m_openRenameModal = true;
            }

            if (ImGui::MenuItem((std::string(ICON_FA_TRASH) + " Delete").c_str()))
                m_pendingDelete = path;

            ImGui::EndPopup();
        }
    }

    void ContentBrowser::drawBackgroundContextMenu()
    {
        if (ImGui::BeginPopupContextWindow("ctx_bg",
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem((std::string(ICON_FA_FOLDER) + " New Folder").c_str()))
                requestCreateFolder(m_currentPath);
            drawNewFileSubmenu([&](const std::string& ext) { requestCreateFile(m_currentPath, ext); });
            ImGui::EndPopup();
        }
    }

    void ContentBrowser::drawRenameModal()
    {
        if (m_openRenameModal)
        {
            ImGui::OpenPopup("Rename");
            m_openRenameModal = false;
        }

        if (ImGui::BeginPopupModal("Rename", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("New name:");
            ImGui::SetNextItemWidth(300.f);
            if (ImGui::InputText("##rename", m_renameBuffer, sizeof(m_renameBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue))
            {
                const std::filesystem::path newPath = m_renamePath.parent_path() / m_renameBuffer;
                std::filesystem::rename(m_renamePath, newPath);
                if (m_selectedPath == m_renamePath) m_selectedPath = newPath;
                m_renamePath.clear();
                refreshFolder();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("OK"))
            {
                const std::filesystem::path newPath = m_renamePath.parent_path() / m_renameBuffer;
                std::filesystem::rename(m_renamePath, newPath);
                if (m_selectedPath == m_renamePath) m_selectedPath = newPath;
                m_renamePath.clear();
                refreshFolder();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    void ContentBrowser::renderDirectoryEntry(const std::filesystem::path &entry)
    {
        std::string icon = std::filesystem::is_empty(entry) ? ICON_FA_FOLDER_O : ICON_FA_FOLDER_OPEN_O;
        bool isSelected = (m_selectedPath == entry);
        std::string id = "##dir_" + entry.string();
        ImVec2 pos = ImGui::GetCursorPos();

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
            return;
        drawItemContextMenu(entry);

        ImGui::SetCursorPos(pos);
        renderCenteredIcon(icon);
        renderCenteredLabel(entry.filename().string());
    }

    void ContentBrowser::renderFileEntry(const std::filesystem::path &entry)
    {
        std::string icon = getFileIcon(entry);
        bool isSelected = (m_selectedPath == entry);
        std::string selectableId = "##file_" + entry.string();
        ImVec2 pos = ImGui::GetCursorPos();

        if (ImGui::Selectable(selectableId.c_str(), isSelected, 0, ImVec2(64, 80)))
            m_selectedPath = entry;

        renderDragSource(entry);
        drawItemContextMenu(entry);

        ImGui::SetCursorPos(pos);
        renderCenteredIcon(icon);
        renderCenteredLabel(entry.filename().string());
    }
} // namespace gcep::editor
