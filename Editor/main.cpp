#include <iostream>

#include <Editor/Camera/camera.hpp>
#include <Editor/Inputs/Inputs.hpp>
#include <Editor/UIManager/ui_manager.hpp>
#include <Editor/Window/Window.hpp>
#include <Engine/RHI/Vulkan/VulkanRHI.hpp>
#include <Log/Log.hpp>
#include <Engine/Core/Scripting/ScriptHost.hpp>
#include <Engine/Core/Scripting/ScriptSystem.hpp>
#include <Engine/Core/ECS/headers/registry.hpp>


int main()
{
    using namespace gcep;
    using VulkanRHI = rhi::vulkan::VulkanRHI;

    Log::initialize("GC Engine Paris", "crash.log");
    Window& window = Window::getInstance();
    window.initWindow();
    InputSystem inputSystem(window.getGlfwWindow());
    Camera camera(&inputSystem, &window);

    gcep::ECS::Registry registry;

    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window.getGlfwWindow(), &fbWidth, &fbHeight);

    rhi::SwapchainDesc swapDesc{};
    swapDesc.nativeWindowHandle = window.getGlfwWindow();
    swapDesc.width              = static_cast<uint32_t>(fbWidth);
    swapDesc.height             = static_cast<uint32_t>(fbHeight);
    swapDesc.vsync              = true;

    std::unique_ptr<VulkanRHI> rhi = std::make_unique<VulkanRHI>(swapDesc);


    try
    {
        rhi->setWindow(window.getGlfwWindow());
        rhi->initRHI();
    }
    catch (std::exception& e)
    {
        Log::handleException(e);
        std::cerr << e.what() << std::endl;
        return 1;
    }

    auto& meshData = rhi->getInitInfos()->meshData;

    // Run at game startup
    gcep::scripting::ScriptSystem scriptSystem = scripting::ScriptSystem::getInstance();
    scriptSystem.init(&registry, *meshData);
    scriptSystem.setMeshList(meshData);

    UiManager uiManager(window.getGlfwWindow(), rhi->getUIInitInfo());
    uiManager.setCamera(&camera);
    uiManager.setInfos(rhi->getInitInfos());


    while (!window.shouldClose())
    {
        glfwPollEvents();
        inputSystem.update();
        //scriptSystem.update(&registry, 0.16f);
        uiManager.uiUpdate();
        rhi->drawFrame();
    }
    Log::info("Window closed");
    rhi->cleanup();
    rhi.reset();

    return 0;
}
