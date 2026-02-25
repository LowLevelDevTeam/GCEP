#include <iostream>

#include <Editor/Window/ui_manager.hpp>
#include <Editor/Window/Window.hpp>
#include <Engine/Core/RHI/ObjParser/ObjParser.hpp>
#include <Editor/Inputs/Inputs.hpp>
#include <Editor/Camera/camera.hpp>
#include <RHI/Vulkan/VulkanRHI.hpp>

int main()
{
    gcep::Window& window = gcep::Window::getInstance();
    window.initWindow();
    gcep::Inputs inputs;
    gcep::Camera camera(&inputs);

    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window.getGlfwWindow(), &fbWidth, &fbHeight);

    gcep::rhi::SwapchainDesc swapDesc{};
    swapDesc.nativeWindowHandle = window.getGlfwWindow();
    swapDesc.width              = static_cast<uint32_t>(fbWidth);
    swapDesc.height             = static_cast<uint32_t>(fbHeight);
    swapDesc.vsync              = false;

    std::unique_ptr<gcep::rhi::vulkan::VulkanRHI> rhi = std::make_unique<gcep::rhi::vulkan::VulkanRHI>(swapDesc);

    try
    {
        rhi->setWindow(window.getGlfwWindow());
        rhi->initRHI();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 1;
    }

    // Initialize ImGui BEFORE creating offscreen resources (ImGui_ImplVulkan_AddTexture requires ImGui)
    gcep::UiManager uiManager(window.getGlfwWindow(), rhi->getInitInfo());
    uiManager.setMeshList(rhi->getMeshData());
    while (!glfwWindowShouldClose(window.getGlfwWindow()))
    {
        glfwPollEvents();
        inputs.update(window.getGlfwWindow());

        // Process any pending resize BEFORE updating UI
        // This ensures the descriptor passed to ImGui is valid
        rhi->processPendingOffscreenResize();

        rhi->updateEditorInfo(
            uiManager.getClearColor(),
            camera.update(uiManager.getViewportSize().x / uiManager.getViewportSize().y, uiManager.f),
            uiManager.getShininess()
        );
        // Update UI with viewport texture and resize callback
        uiManager.uiUpdate(
            rhi->getImGuiTextureDescriptor(),
            [&rhi](uint32_t width, uint32_t height) {
                rhi->requestOffscreenResize(width, height);
            },
            &camera,
            rhi->getDrawCount()
        );
        rhi->updateSceneUBO(uiManager.getLightDirection(), uiManager.getLightColor(), uiManager.getAmbientColor(), camera.position);

        rhi->drawFrame();
    }
    rhi->cleanup();
    rhi.reset();

    glfwDestroyWindow(gcep::Window::getInstance().getGlfwWindow());
    glfwTerminate();

    return 0;
}
