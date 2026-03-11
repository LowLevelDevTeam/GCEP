// STL
#include <stdexcept>

// Externals
#include <font_awesome.hpp>
#include <imgui_impl_glfw.h>

#include <Editor/Helpers.hpp>
#include <Editor/ImGUIManager/imgui_manager.hpp>

namespace gcep::UI
{
    ImGUIManager::ImGUIManager()
    {
        m_initialized = false;
    }

    void  ImGUIManager::init(GLFWwindow* window, ImGui_ImplVulkan_InitInfo initInfo)
    {
        if (m_initialized)
        {
            // Maybe just Log::error() and return ? Seems a little excessive...
            throw std::runtime_error("ImGUI Manager already initialized");
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
       // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        float scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
        ImGuiStyle &style = ImGui::GetStyle();
        style.ScaleAllSizes(scale);
        style.FontScaleDpi = scale;
        style.WindowPadding = ImVec2(0.0f, 0.0f);

        gcep::setDarkTheme();

        constexpr float baseFontSize = 18.0f;
        constexpr float iconFontSize = baseFontSize * 2.0f / 3;
        io.FontDefault = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Nunito-Regular.ttf", baseFontSize);
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphMinAdvanceX = iconFontSize;
        static constexpr ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
        io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_FA, iconFontSize, &icons_config, icons_ranges);

        constexpr float bigIconSize = 32.0f;
        ImFontConfig big_icons_config;
        big_icons_config.MergeMode = false;
        big_icons_config.PixelSnapH = true;
        big_icons_config.GlyphMinAdvanceX = bigIconSize;
        io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_FA, bigIconSize, &big_icons_config, icons_ranges);

        ImGui_ImplGlfw_InitForVulkan(window, true);
        ImGui_ImplVulkan_Init(&initInfo);

        m_initialized = true;
    }

    void ImGUIManager::shutdown()
    {
        if (!m_initialized)
        {
            throw std::runtime_error("ImGUI Manager not initialized");
        }

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        m_initialized = false;
    }

    void ImGUIManager::beginFrame()
    {
        if (!m_initialized)
        {
            throw std::runtime_error("ImGUI Manager not initialized");
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGUIManager::endFrame()
    {
        if (!m_initialized)
            throw std::runtime_error("ImGUI Manager not initialized");

        ImGui::Render();

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        // Marquer la frame platform comme terminée
        ImGui::EndFrame(); // ← non, Render() appelle déjà EndFrame implicitement
    }
} // Namespace gcep::UI