#pragma once

// Internals
#include <Editor/Camera/camera.hpp>
#include <Editor/Helpers.hpp>
#include <Editor/ImGUIManager/imgui_manager.hpp>
#include <Editor/Inputs/inputs.hpp>
#include <Editor/UIManager/ui_manager.hpp>
#include <PhysicsWrapper/physics_system.hpp>
#include <Editor/Window/window.hpp>
#include <Engine/RHI/Vulkan/vulkan_rhi.hpp>
#include <Log/log.hpp>
#include <Maths/vector3.hpp>
#include <Scene/header/scene_manager.hpp>

// STL
#include <iostream>
#include <memory>

/// @file application.hpp
/// @brief Entry point and main loop of the GCEP editor application.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.4
/// @date 2026-02-17

namespace gcep
{
    /// @class Application
    /// @brief Top-level application class that owns and orchestrates all editor subsystems.
    ///
    /// @c Application is the root object of the editor process. It is responsible for:
    /// - Initialising the window, RHI, input system, camera, ImGui, and UI manager.
    /// - Running the main loop and computing per-frame delta time.
    /// - Gracefully shutting down every subsystem in reverse-initialisation order.
    ///
    /// @par Lifecycle
    /// @code
    /// Application app;
    /// return app.run();   // blocks until the window is closed
    /// @endcode
    ///
    /// @par Simulation state
    /// The field @c m_simulationState tracks whether the in-editor simulation is
    /// stopped, playing, or paused. It is shared with @c UiManager through a reference
    /// so the toolbar can mutate it directly.
    ///
    /// @note All methods must be called from the main thread.
    class Application
    {
    public:
        Application()  = default;
        ~Application() = default;

        /// @brief Initialises all subsystems and enters the main loop.
        ///
        /// Calls @c init(), then loops until @c m_isRunning is false, calling
        /// @c update() each iteration with the delta time returned by
        /// @c computeDeltaTime(). Calls @c shutdown() before returning.
        ///
        /// @returns 0 on clean exit, non-zero on unrecoverable error.
        [[nodiscard]] int run();

    private:
        /// @brief Creates and initialises every subsystem in dependency order.
        ///
        /// Order: Window → RHI → InputSystem → Camera → ImGUIManager → UiManager.
        /// Also initialises @c pl::project_loader and enters the project-selection loop
        /// while @c m_whileSelectingProject is true.
        void init();

        /// @brief Destroys every subsystem in reverse-initialisation order.
        ///
        /// Waits for the GPU to be idle before releasing Vulkan resources, then
        /// calls @c shutdownImGUI() and resets all smart pointers.
        void shutdown();

        /// @brief Executes one application frame.
        ///
        /// Polls GLFW events, updates the input system, advances the camera,
        /// calls @c UiManager::uiUpdate(), and submits the frame to the RHI.
        ///
        /// @param deltaTime  Time elapsed since the previous frame, in seconds.
        void update(float deltaTime);

        /// @brief Computes the elapsed time since the last call.
        ///
        /// Uses @c glfwGetTime() and the stored @c m_lastFrameTime to derive the
        /// interval. Updates @c m_lastFrameTime as a side-effect.
        ///
        /// @returns Delta time in seconds. Clamped to a maximum of 0.1 s to avoid
        ///          large spikes on the first frame or after a debugger break.
        [[nodiscard]] float computeDeltaTime();

    private:
        /// @brief Current simulation state (STOPPED, PLAYING, or PAUSED).
        gcep::SimulationState m_simulationState = SimulationState::STOPPED;

        /// @brief Scene-level rendering and environment settings.
        SLS::SceneSettings m_sceneSettings;

        /// @brief Main loop condition; set to false to request application exit.
        bool m_isRunning = true;

        /// @brief Loop condition for the initial project-selection screen.
        bool m_whileSelectingProject = true;

        /// @brief When true, the application will reload the current project on the next frame.
        bool m_reloadRequested = false;

        /// @brief Non-owning pointer to the singleton @c Window. Lifetime managed by @c Window::getInstance().
        Window* m_window = nullptr;

        std::unique_ptr<UI::ImGuiManager>          m_imguiManager;
        std::unique_ptr<rhi::vulkan::VulkanRHI>    m_rhi;
        std::unique_ptr<InputSystem>               m_input;
        std::unique_ptr<Camera>                    m_camera;
        std::unique_ptr<UiManager>                 m_uiManager;

        /// @brief Absolute filesystem path of the scene currently loaded.
        std::string m_currentScenePath;

        /// @brief Non-owning pointer to the physics subsystem. Managed externally.
        PhysicsSystem* m_physicsSystem = nullptr;

        /// @brief Non-owning pointer to the audio subsystem. Managed externally.
        AudioSystem* m_audioSystem = nullptr;

        /// @brief Timestamp of the last frame start, as returned by @c glfwGetTime().
        double m_lastFrameTime = 0.0;
    };
} // namespace gcep
