//
// Created by leoam on 07/03/2026.
//

#include "application.hpp"

#include "Externals/glfw/include/GLFW/glfw3.h"
#include "Externals/glm/glm/vec3.hpp"
#include "Editor/ProjectLoader/project_loader.hpp"

int gcep::application::run()
{
    while (m_isRunning)
    {
        if (!m_reloadRequested)
        {
            Log::initialize("GC Engine Paris", "crash.log");
        }
        else
        {
            Log::info("--- Engine reloaded ---\n");
        }
        m_reloadRequested = false;

        init();

        while (!m_window->shouldClose() && !m_reloadRequested)
        {
            float deltaTime = computeDeltaTime();

            pl::project_loader::instance().update(deltaTime);

            //if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_S))
            //{
            //    SLS::SceneManager::instance().saveScene(m_currentScenePath);
            //}

            glfwPollEvents();
            m_input->update();

            if (m_audioSystem)
            {
                AudioListener* listener = m_audioSystem->getListener();
                listener->setPosition(gcep::Vector3<float>(m_camera->position.x, m_camera->position.y, m_camera->position.z));
                listener->setForward(gcep::Vector3<float>(m_camera->front.x, m_camera->front.y, m_camera->front.z));
                listener->setUp(gcep::Vector3<float>(0.0f, 1.0f, 0.0f));
            }

            update(deltaTime);

            m_imguiManager->beginFrame();
            m_uiManager->uiUpdate();
            m_imguiManager->endFrame();

            m_rhi->drawFrame();
        }

        Log::info("Window closed");
        shutdown();

        if (!m_reloadRequested)
        {
            m_isRunning = false;
        }
    }
    return 0;
}

void gcep::application::init()
{
    // Systèmes de base
    m_window        = &Window::getInstance();
    m_physicsSystem = &PhysicsSystem::getInstance();
    m_audioSystem   = AudioSystem::getInstance();
    m_window->initWindow();
    m_input = std::make_unique<InputSystem>(m_window->getGlfwWindow());

    // Vulkan
    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(m_window->getGlfwWindow(), &fbWidth, &fbHeight);
    rhi::SwapchainDesc swapDesc{};
    swapDesc.nativeWindowHandle = m_window->getGlfwWindow();
    swapDesc.width              = static_cast<uint32_t>(fbWidth);
    swapDesc.height             = static_cast<uint32_t>(fbHeight);
    swapDesc.vsync              = true;
    m_rhi = std::make_unique<rhi::vulkan::VulkanRHI>(swapDesc);
    m_rhi->setWindow(m_window->getGlfwWindow());
    m_rhi->initRHI();

    // ImGui — une seule fois, via ImGuiManager
    m_imguiManager = std::make_unique<UI::ImGUIManager>();
    m_imguiManager->init(m_window->getGlfwWindow(), m_rhi->getUIInitInfo());


    // Project loader
    bool selecting = true;

    pl::project_loader::instance().init(m_window->getGlfwWindow());
    while (selecting && !m_window->shouldClose())
    {
        glfwPollEvents();
        m_imguiManager->beginFrame();
        pl::project_loader::instance().drawUI(selecting);
        m_imguiManager->endFrame();
        m_rhi->drawFrame();
    }



    // Scène
    SLS::SceneManager::instance().loadScene(pl::project_loader::instance().getProjectInfo().startScene.string(), m_rhi.get());
    m_rhi->setRegistry(&SLS::SceneManager::instance().current().getRegistry());

    m_camera = std::make_unique<Camera>(m_input.get(), m_window);

    // UiManager
    m_uiManager = std::make_unique<UiManager>(
        m_window->getGlfwWindow(),
        m_reloadRequested,
        m_isRunning
    );

    m_uiManager->setCamera(m_camera.get());
    m_uiManager->setInfos(m_rhi->getInitInfos());
    m_uiManager->setProjectLoader(&pl::project_loader::instance());
    m_uiManager->applyLoadedSettings();

    m_currentScenePath = pl::project_loader::instance().getProjectInfo().startScene.string();
    m_uiManager->setCurrentScenePath(m_currentScenePath);
    m_uiManager->setSceneManager(&SLS::SceneManager::instance());

    m_lastFrameTime = glfwGetTime();
}

void gcep::application::shutdown()
{

    m_physicsSystem->shutdown();
   //


    m_input.reset();
    m_camera.reset();
    m_rhi->cleanup();
    m_uiManager.reset();
    m_imguiManager->shutdown();
    m_rhi.reset();
    m_imguiManager.reset();
    m_window->destroy();

}

void gcep::application::update(float deltaTime)
{
    switch (m_simulationState)
    {
        case SimulationState::STOPPED:
            break;
        case SimulationState::PLAYING:
            m_physicsSystem->update(deltaTime);
            break;
        case SimulationState::PAUSED:
            break;
    }
}

float gcep::application::computeDeltaTime()
{
    double currentTime = glfwGetTime();
    float deltaTime = static_cast<float>(currentTime - m_lastFrameTime);
    m_lastFrameTime = currentTime;
    return deltaTime;
}