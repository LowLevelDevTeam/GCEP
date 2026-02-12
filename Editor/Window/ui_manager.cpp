#include "ui_manager.hpp"
#include <cmath>

namespace gcep {
UiManager::UiManager(GLFWwindow* window, ImGui_ImplVulkan_InitInfo initInfo)
{
    m_window = window;

    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());

    IMGUI_CHECKVERSION();

    // Create context
    ImGui::CreateContext();

    // Get variables references
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // setup ImGui style
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;
    style.WindowPadding = ImVec2(0.0f, 0.0f);

    // Setup Platfrom/renderer backend
    ImGui_ImplGlfw_InitForVulkan(window, true);
    m_initInfo = initInfo;
    ImGui_ImplVulkan_Init(&m_initInfo);
}

UiManager::~UiManager()
{

}

void UiManager::uiUpdate()
{
    uiUpdate(VK_NULL_HANDLE, nullptr);
}

void UiManager::uiUpdate(VkDescriptorSet sceneTexture, const std::function<void(uint32_t, uint32_t)>& viewportResizeCallback)
{
    if (glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) != 0)
    {
        ImGui_ImplGlfw_Sleep(10);
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Enable docking over the entire viewport
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    // Viewport window with the Vulkan rendered scene
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        ImVec2 availSize = ImGui::GetContentRegionAvail();

        // Check if viewport size changed (with a small threshold to avoid floating point issues)
        bool sizeChanged = std::abs(availSize.x - m_viewportSize.x) > 1.0f ||
                           std::abs(availSize.y - m_viewportSize.y) > 1.0f;

        if (sizeChanged && availSize.x > 0 && availSize.y > 0)
        {
            m_viewportSize = availSize;
            if (viewportResizeCallback)
            {
                viewportResizeCallback(
                    static_cast<uint32_t>(availSize.x),
                    static_cast<uint32_t>(availSize.y)
                );
                // Don't use the texture this frame since we just requested a resize
                // The texture will be valid next frame after processPendingOffscreenResize
                sceneTexture = VK_NULL_HANDLE;
            }
        }

        if (sceneTexture != VK_NULL_HANDLE && availSize.x > 0 && availSize.y > 0)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(sceneTexture), availSize);
        }
        else
        {
            // Display placeholder text when no texture is available
            ImGui::Text("Viewport loading...");
        }
    }
    ImGui::End();

    if (showDemoWindow)
        ImGui::ShowDemoWindow(&showDemoWindow);

    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");

        ImGui::Text("This is some useful text.");
        ImGui::Checkbox("Demo Window", &showDemoWindow);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);


        ImGui::ColorEdit4("ClearColor",(float*)&m_clearColor, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);
        if (ImGui::Button("Button"))
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        auto& io = ImGui::GetIO();
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::Text("Viewport size: %.0f x %.0f", m_viewportSize.x, m_viewportSize.y);
        ImGui::End();
    }
}


ImVec4& UiManager::getClearColor()
{
    return m_clearColor;
}
}
