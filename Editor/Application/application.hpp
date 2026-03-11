#pragma once
#include <iostream>
#include <memory>

#include <Editor/Camera/camera.hpp>
#include <Editor/Helpers.hpp>
#include <Editor/Inputs/Inputs.hpp>
#include <Editor/UIManager/ui_manager.hpp>
#include <Editor/Window/Window.hpp>
#include <Engine/RHI/Vulkan/VulkanRHI.hpp>
#include <Log/Log.hpp>
#include <Engine/Core/Scene/header/scene_manager.hpp>
//#include <Engine/Core/Scripting/ScriptHost.hpp>
//#include <Engine/Core/Scripting/ScriptSystem.hpp>
#include <Engine/Core/Maths/vector3.hpp>

#include "Editor/ImGUIManager/imgui_manager.hpp"


namespace gcep
{


    class application
    {
    public:
        application() = default;
        ~application() = default;

        int run();
    private:
        void init();
        void shutdown();
        void update(float deltaTime);
        float computeDeltaTime();

        void initImGUI();
        void shutdownImGUI();

        gcep::SimulationState m_simulationState = SimulationState::STOPPED;
        SLS::SceneSettings m_sceneSettings;

        bool m_isRunning = true;
        bool m_whileSelectingProject = true;
        bool m_reloadRequested = false;

        Window* m_window = nullptr;
        std::unique_ptr<UI::ImGUIManager>  m_imguiManager;
        std::unique_ptr<rhi::vulkan::VulkanRHI>     m_rhi;
        std::unique_ptr<InputSystem>              m_input;
        std::unique_ptr<Camera>                  m_camera;
        std::unique_ptr<UiManager>            m_uiManager;
        std::string                    m_currentScenePath;

        //scripting::ScriptSystem* m_scriptSystem = nullptr;
        PhysicsSystem*                m_physicsSystem = nullptr;
        AudioSystem*                  m_audioSystem = nullptr;

        double m_lastFrameTime = 0.0;
    };
}


