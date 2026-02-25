#include "ui_manager.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <cmath>
#include <Editor/Camera/camera.hpp>

#include "imgui_internal.h"

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

void UiManager::setMeshList(std::vector<rhi::vulkan::VulkanMesh> &data)
{
    meshData  = &data;
}

static bool DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f) {
    bool value_changed = false;
    ImGuiIO& io = ImGui::GetIO();
    auto boldFont = io.Fonts->Fonts[0];

    ImGui::PushID(label.c_str());

    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, columnWidth);
    ImGui::Text("%s", label.c_str());
    ImGui::NextColumn();

    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

    float lineHeight = GImGui->Font->LegacySize + GImGui->Style.FramePadding.y * 2.0f;
    ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

    // X
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::PushFont(boldFont);
    if (ImGui::Button("X", buttonSize)) {
        values.x = resetValue;
        value_changed = true;
    }
    ImGui::PopFont();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if(ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f")) value_changed = true;
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Y
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::PushFont(boldFont);
    if (ImGui::Button("Y", buttonSize)) {
        values.y = resetValue;
        value_changed = true;
    }
    ImGui::PopFont();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if(ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f")) value_changed = true;
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Z
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
    ImGui::PushFont(boldFont);
    if (ImGui::Button("Z", buttonSize)) {
        values.z = resetValue;
        value_changed = true;
    }
    ImGui::PopFont();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if(ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f")) value_changed = true;
    ImGui::PopItemWidth();

    ImGui::PopStyleVar();
    ImGui::Columns(1);
    ImGui::PopID();

    return value_changed;
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
        ImGui::DragFloat("Shininess", &shininess, 1.0f, 0.0f, 1000.0f, "%.2f");

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

    {
        ImGui::Begin("Scene Hierarchy");

        if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& entity : *meshData) {
                bool is_selected = (m_SelectedEntityID == entity.id);
                if (ImGui::Selectable(entity.name.c_str(), is_selected)) {
                    m_SelectedEntityID = entity.id;
                }
            }
            ImGui::TreePop();
        }

        // Deselect if clicking in empty space
        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
        {
            m_SelectedEntityID = std::numeric_limits<uint32_t>::max();
        }

        ImGui::End();

        ImGui::Begin("Scene infos");

        if (m_SelectedEntityID != std::numeric_limits<uint32_t>::max())
        {
            auto& data = *meshData;
            auto& entity = data[m_SelectedEntityID];
            DrawVec3Control("Translation", entity.transform.position);
            DrawVec3Control("Rotation", entity.transform.rotation);
            DrawVec3Control("Scale", entity.transform.scale);
        }
        ImGui::End();
    }
}


ImVec4& UiManager::getClearColor()
{
    return m_clearColor;
}
}
