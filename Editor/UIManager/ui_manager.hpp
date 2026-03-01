#pragma once

// Externals
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <TextEditor.h>
#include <vulkan/vulkan_raii.hpp>

// STL
#include <iostream>
#include <limits>

#include <Engine/Audio/audio_system.hpp>
#include <ECS/headers/registry.hpp>
#include <Engine/RHI/Vulkan/VulkanMesh.hpp>
#include <Engine/RHI/Vulkan/VulkanRHIDataTypes.hpp>
#include <PhysicsWrapper/physics_system.hpp>

namespace gcep
{
class Camera;

class ImGuiConsoleBuffer : public std::streambuf
{
public:
    ImGuiConsoleBuffer(std::streambuf* original, std::vector<std::string>& log)
            : m_original(original), m_log(log)
    {}

protected:
    virtual int overflow(int c) override
    {
        if (c == '\n')
        {
            m_log.push_back(m_currentLine);
            m_currentLine.clear();
        }
        else
        {
            m_currentLine += static_cast<char>(c);
        }

        return m_original->sputc(c);
    }

private:
    std::streambuf* m_original;
    std::vector<std::string>& m_log;
    std::string m_currentLine;
};

class UiManager
{
public:
    // @brief creation of the Editor UI
    UiManager(GLFWwindow* window, ImGui_ImplVulkan_InitInfo initInfo);
    ~UiManager();

    void setInfos(rhi::vulkan::InitInfos* initInfos);

    /// @brief Update UI with viewport texture
    void uiUpdate();
    void drawGizmoControls();
    void handleGizmoInput();
    void drawGizmo(Camera* camera);

    void setCamera(Camera *pCamera);

private:
    // Init
    ImGuiViewport* setupViewport();
    ImGuiID getDockspaceID();
    void drawMainMenuBar();
    void initDockspace(const ImGuiID& dockspace_id, const ImGuiViewport* viewport);
    void initConsole();
    // Shutdown
    void shutdownConsole();
    // Loop
    void beginFrame();
    void drawCodeEditor();
    void drawViewport();
    void drawSceneInfos();
    void drawSceneHierarchy();
    void drawEntityProperties();
    void drawAudioControl();
    void drawConsole();
    void drawSimulationPanel();

private:
    TextEditor editor;
    GLFWwindow* m_window;
    ImGui_ImplVulkan_InitInfo m_initInfo;
    bool showDemoWindow = false;
    ImVec2 m_viewportSize = {800, 600};
    std::vector<rhi::vulkan::VulkanMesh>* meshData;
    ECS::EntityID m_selectedEntityID = std::numeric_limits<ECS::EntityID>::max();
    glm::vec4 m_clearColor = {0.1f, 0.1f, 0.1f, 1.0f};
    glm::vec3 ambientColor = {0.2f, 0.2f, 0.2f};
    glm::vec3 lightColor = {0.5f, 0.5f, 0.5f};
    glm::vec3 lightDirection = {1.0f, 1.0f, 0.0f};
    float shininess = 64.0f;
    float camSpeed = 2.0f; // Camera speed, exposed to UI for real-time tweaking
    rhi::vulkan::SceneInfos sceneInfos;
    rhi::vulkan::GridPushConstant gridPC;
    ImGuizmo::OPERATION m_currentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE m_currentGizmoMode = ImGuizmo::WORLD; // WORLD ou LOCAL
    bool m_useSnap = false;
    float m_snapTranslation[3] = {0.5f, 0.5f, 0.5f};
    float m_snapRotation = 15.0f;  // degrés
    float m_snapScale = 0.1f;
    ECS::Registry* m_registry;
    Camera* cameraRef;
    VkDescriptorSet* viewportTexture;
    rhi::vulkan::VulkanRHI* pRHI;
    AudioSystem* audioSystem;
    std::vector<std::shared_ptr<gcep::AudioSource>> audioSources;
    // Console
    std::vector<std::string> m_consoleItems;
    std::unique_ptr<ImGuiConsoleBuffer> m_consoleBuffer;
    std::streambuf* m_oldCout = nullptr;
    // Physics
    PhysicsSystem& physicsSystem;
};

} // Namespace gcep
