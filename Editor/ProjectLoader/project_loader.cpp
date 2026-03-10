#include "project_loader.hpp"
#include <tinyfiledialogs.h>
#include <imgui.h>
#include <fstream>

namespace pl
{
    static std::filesystem::path getAppDataPath()
    {
        const char* appData = std::getenv("APPDATA");
        std::filesystem::path base = appData
            ? std::filesystem::path(appData) / "GCEngine"
            : std::filesystem::current_path() / "GCEngine";
        if (!std::filesystem::exists(base))
            std::filesystem::create_directories(base);
        return base;
    }

    project_loader::project_loader(GLFWwindow* window) : m_window(window) {}

    void project_loader::drawUI(bool& stillSelecting)
    {
        ImGui::Begin("Project Loader", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text("Select a project to open:");

        if (ImGui::Button("Open project"))
        {
            const char* filters[] = { "*.gcproj" };
            if (auto path = tinyfd_openFileDialog("Open a project",
                std::filesystem::current_path().string().c_str(), 1, filters, "GC Project", 0);
                path != nullptr)
            {
                m_info.projectPath = std::filesystem::path(path).parent_path();
                m_info.startScene  = m_info.projectPath / "Scenes" / "default.gcmap";
                stillSelecting     = false;
            }
        }

        ImGui::InputText("Project name", m_projectName, sizeof(m_projectName));

        if (ImGui::Button("Create project"))
        {
            if (m_projectName[0] == '\0')
            {
                ImGui::OpenPopup("Error");
            }
            else
            {
                std::filesystem::path projectPath = getAppDataPath() / m_projectName;
                std::filesystem::create_directories(projectPath / "Scenes");

                std::filesystem::path projFile = projectPath / (std::string(m_projectName) + ".gcproj");
                if (!std::filesystem::exists(projFile))
                {
                    std::ofstream ofs(projFile, std::ios::binary);
                    ofs << "{}";
                }

                m_info.projectPath = projectPath;
                m_info.projectName = m_projectName;
                m_info.startScene  = projectPath / "Scenes" / "default.gcmap";
                stillSelecting     = false;
            }

        }
        if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Please enter a project name.");
            if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::End();
    }
}