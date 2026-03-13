// STL
#include <stdexcept>

// Externals
#include <font_awesome.hpp>
#include <imgui_impl_glfw.h>

#include <Editor/Helpers.hpp>
#include <Editor/ImGUIManager/imgui_manager.hpp>

namespace gcep::UI
{

ImGuiManager::ImGuiManager()
{
    m_initialized = false;
}

void ImGuiManager::init(GLFWwindow* window, ImGui_ImplVulkan_InitInfo initInfo)
{
    if (m_initialized)
    {
        throw std::runtime_error("ImGUI Manager already initialized");
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    float scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale);
    style.FontScaleDpi   = scale;
    style.WindowPadding  = ImVec2(0.0f, 0.0f);

    gcep::setDarkTheme();

    constexpr float baseFontSize = 18.0f;
    constexpr float iconFontSize = baseFontSize * 2.0f / 3;
    io.FontDefault = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Nunito-Regular.ttf", baseFontSize);

    ImFontConfig iconsConfig;
    iconsConfig.MergeMode        = true;
    iconsConfig.PixelSnapH       = true;
    iconsConfig.GlyphMinAdvanceX = iconFontSize;
    static constexpr ImWchar iconsRanges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_FA, iconFontSize, &iconsConfig, iconsRanges);

    constexpr float bigIconSize = 32.0f;
    ImFontConfig bigIconsConfig;
    bigIconsConfig.MergeMode        = false;
    bigIconsConfig.PixelSnapH       = true;
    bigIconsConfig.GlyphMinAdvanceX = bigIconSize;
    io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_FA, bigIconSize, &bigIconsConfig, iconsRanges);

    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_Init(&initInfo);

    m_initialized = true;
}

void ImGuiManager::shutdown()
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

void ImGuiManager::beginFrame()
{
    if (!m_initialized)
    {
        throw std::runtime_error("ImGUI Manager not initialized");
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::endFrame()
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
}

} // namespace gcep::UI
