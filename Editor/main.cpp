#include <iostream>

#include <Editor/Window/ui_manager.hpp>
#include <Editor/Window/Window.hpp>
#include <Engine/Core/RHI/RenderWrapper/RHI_Vulkan.hpp>
#include <Engine/Core/RHI/ObjParser/ObjParser.hpp>
#include <Editor/Inputs/Inputs.hpp>
#include <Editor/Camera/camera.hpp>

int main()
{
    // Rendering - moved to the heap because the class is getting too big (precaution).
    std::unique_ptr<gcep::RHI_Vulkan> rhi = std::make_unique<gcep::RHI_Vulkan>();
    gcep::Window& window = gcep::Window::getInstance();
    window.initWindow();
    gcep::Inputs inputs;
    gcep::Camera camera(&inputs);
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

    // Now create offscreen resources for viewport rendering
    rhi->createOffscreenResources(800, 600);


    ImVec4 test = uiManager.getClearColor();
    while (!glfwWindowShouldClose(window.getGlfwWindow()))
    {
        glfwPollEvents();
        inputs.update(window.getGlfwWindow());

        // Process any pending resize BEFORE updating UI
        // This ensures the descriptor passed to ImGui is valid
        rhi->processPendingOffscreenResize();

        rhi->updateEditorInfo(uiManager.getClearColor());
        // Update UI with viewport texture and resize callback
        uiManager.uiUpdate(
            rhi->getImGuiTextureDescriptor(),
            [&rhi](uint32_t width, uint32_t height) {
                rhi->requestOffscreenResize(width, height);
            }
        );

        rhi->drawFrame();
    }
    rhi->cleanup();
    glfwDestroyWindow(gcep::Window::getInstance().getGlfwWindow());
    glfwTerminate();

    return 0;
}