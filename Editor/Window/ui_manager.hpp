#pragma once


#include <RHI/Vulkan/VulkanMesh.hpp>
#include <RHI/Vulkan/VulkanRHIDataTypes.hpp>

// Externals
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include "TextEditor.h"

#include <vulkan/vulkan_raii.hpp>

// STL
#include <functional>
#include <limits>

namespace gcep
{
class Camera;
class UiManager {
public:

    void initEditor();
    // @brief creation of the Editoreditor UI
    UiManager(GLFWwindow* window, ImGui_ImplVulkan_InitInfo initInfo);

    void setMeshList(std::vector<rhi::vulkan::VulkanMesh>& data);
    void setVieportResizeCallback(const std::function<void(uint32_t, uint32_t)>& callback);
    void setScriptReloadCallback(const std::function<void()>& callback);

    /// @brief Update UI with viewport texture
    /// @param sceneTexture The descriptor set for the offscreen rendered scene
    /// @param viewportResizeCallback Callback called when viewport size changes
    void uiUpdate(VkDescriptorSet sceneTexture, Camera* camera, uint32_t drawCount);

    void drawGizmoControls();

    void handleGizmoInput();

    void drawGizmo(Camera* camera);

    /// @brief Get the current viewport size
    [[nodiscard]] inline ImVec2 getViewportSize()      const { return m_viewportSize; }
    [[nodiscard]] inline rhi::vulkan::SceneInfos* getSceneInfos() { return &perFrame; }
    [[nodiscard]] inline glm::vec3 getLightColor()     const { return lightColor; }
    [[nodiscard]] inline glm::vec3 getLightDirection() const { return lightDirection; }
    [[nodiscard]] inline glm::vec4& getClearColor()          { return m_clearColor; }
    [[nodiscard]] inline float getShininess()          const { return shininess; }

public:
    float camSpeed = 2.0f; // Camera speed, exposed to UI for real-time tweaking

private:

    TextEditor editor;
    GLFWwindow* m_window;
    ImGui_ImplVulkan_InitInfo m_initInfo;
    bool showDemoWindow = false;
    ImVec2 m_viewportSize = {800, 600};
    std::vector<rhi::vulkan::VulkanMesh>* meshData;
    std::function<void(uint32_t, uint32_t)> m_viewportResizeCallback;
    std::function<void()> m_scriptReloadCallback;
    uint32_t m_SelectedEntityID = std::numeric_limits<uint32_t>::max();
    glm::vec4 m_clearColor = {0.1f, 0.1f, 0.1f, 1.0f};
    glm::vec3 ambientColor = {0.2f, 0.2f, 0.2f};
    glm::vec3 lightColor = {0.5f, 0.5f, 0.5f};
    glm::vec3 lightDirection = {1.0f, 1.0f, 0.0f};
    float shininess = 64.0f;
    rhi::vulkan::SceneInfos perFrame;

    ImGuizmo::OPERATION m_currentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE m_currentGizmoMode = ImGuizmo::WORLD; // WORLD ou LOCAL
    bool m_useSnap = false;
    float m_snapTranslation[3] = {0.5f, 0.5f, 0.5f};
    float m_snapRotation = 15.0f;  // degrés
    float m_snapScale = 0.1f;
};

}
