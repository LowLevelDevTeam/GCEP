#include <iostream>

#include <Editor/Window/ui_manager.hpp>
#include <Editor/Window/Window.hpp>
#include <Engine/Core/RHI/RenderWrapper/RHI_Vulkan.hpp>
#include <Engine/Core/RHI/ObjParser/ObjParser.hpp>

int main()
{
    // Obj parser
    std::cout << "Viking room - 468KB" << std::endl;
    std::filesystem::path filepath = "TestTextures/viking_room.obj";
    gcep::objParser::ObjFile objFile(filepath);
    std::cout << "Chinese Qilin statue - 46.2MB" << std::endl;
    std::filesystem::path qilinPath = "TestTextures/qilin.obj";
    gcep::objParser::ObjFile qilinObj(qilinPath);

    // Rendering - moved to the heap because the class is getting too big (precaution).
    std::unique_ptr<gcep::RHI_Vulkan> rhi = std::make_unique<gcep::RHI_Vulkan>();
    gcep::Window& window = gcep::Window::getInstance();
    window.initWindow();
    try
    {
        rhi->setWindow(window.getGlfwWindow());
        rhi->initRHI();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
    gcep::UiManager uiManager(window.getGlfwWindow(), rhi->getInitInfo());
    while (!glfwWindowShouldClose(window.getGlfwWindow()))
    {
        glfwPollEvents();
        uiManager.uiUpdate();
        rhi->drawFrame();
    }
    rhi->cleanup();
    glfwDestroyWindow(gcep::Window::getInstance().getGlfwWindow());
    glfwTerminate();

    return 0;
}