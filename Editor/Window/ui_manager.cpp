#include "ui_manager.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <cmath>
#include <Editor/Camera/camera.hpp>

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

void UiManager::uiUpdate(VkDescriptorSet sceneTexture, const std::function<void(uint32_t, uint32_t)>& viewportResizeCallback, Camera* camera, uint32_t drawCount)
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
        ImGui::Begin("Hello, world!");

        ImGui::Checkbox("Demo Window", &showDemoWindow);
        ImGui::ColorEdit4("ClearColor", (float*)&m_clearColor, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);

        ImGui::SeparatorText("Scene infos");
        ImGui::Text("Entities drawn : %d", drawCount);

        ImGui::SeparatorText("Camera");
        ImGui::SliderFloat("Camera Speed", &f, 2.0f, 100.0f);
        ImGui::DragFloat3("Camera position", glm::value_ptr(camera->position));
        ImGui::DragFloat3("Camera front vector", glm::value_ptr(camera->front));
        ImGui::DragFloat("Camera yaw", &camera->yaw);
        ImGui::DragFloat("Camera pitch", &camera->pitch);

        ImGui::SeparatorText("Lights & normals");
        ImGui::ColorEdit3("Ambient color", glm::value_ptr(ambientColor), ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);
        ImGui::ColorEdit3("Light color", glm::value_ptr(lightColor), ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);
        ImGui::DragFloat3("Light direction", glm::value_ptr(lightDirection));

        auto& io = ImGui::GetIO();
        ImGui::SeparatorText("Application infos");
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
