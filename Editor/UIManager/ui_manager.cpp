#include "ui_manager.hpp"

// Externals
#include <font_awesome.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <tinyfiledialogs.h>
#include <fstream>

// STL
#include <cmath>

#include <config.hpp>
#include <Editor/Camera/camera.hpp>
#include <Editor/Helpers.hpp>
#include <Log/Log.hpp>
#include <Maths/quaternion.hpp>

namespace gcep
{

UiManager::UiManager(GLFWwindow* window, ImGui_ImplVulkan_InitInfo initInfo, bool& reload, bool& close) : physicsSystem(PhysicsSystem::getInstance()), scriptSystem(scripting::ScriptSystem::getInstance()), reloadApp(reload), closeApp(close)
{
    m_window = window;
    m_initInfo = initInfo;
    audioSystem = gcep::AudioSystem::getInstance();

    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());

    IMGUI_CHECKVERSION();

    // Create context
    ImGui::CreateContext();

    // Get variables references
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // setup ImGui style
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;
    style.WindowPadding = ImVec2(0.0f, 0.0f);

    constexpr float baseFontSize = 18.0f;
    constexpr float iconFontSize = baseFontSize * 2.0f / 3.0f;
    io.FontDefault = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Nunito-Regular.ttf", baseFontSize);
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = iconFontSize;
    static constexpr ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_FA, iconFontSize, &icons_config, icons_ranges);

    // Setup Platfrom/renderer backend
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_Init(&m_initInfo);
    initConsole();
    setDarkTheme();
    Log::info("UiManager initialized successfully");
}

UiManager::~UiManager()
{
    shutdownConsole();
    Log::info("UiManager destroyed");
}

void UiManager::setCamera(Camera *pCamera)
{
    cameraRef = pCamera;
}

void UiManager::setInfos(rhi::vulkan::InitInfos* infos)
{
    pRHI = infos->instance;
    meshData = infos->meshData;
    m_registry = infos->registry;
    viewportTexture = infos->ds;
}

static bool DrawVec3Control(const std::string& label, Vector3<float>& values, float resetValue = 0.0f, float columnWidth = 100.0f) {
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
    if (ImGui::Button("X", buttonSize))
    {
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
    if (ImGui::Button("Y", buttonSize))
    {
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
    if (ImGui::Button("Z", buttonSize))
    {
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

ImGuiViewport* UiManager::setupViewport() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    return viewport;
}

ImGuiID UiManager::getDockspaceID() {
    constexpr ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                           ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                           ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("DockSpaceHost", nullptr, hostFlags);
    ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
    drawMainMenuBar();
    ImGui::End();
    ImGui::PopStyleVar();
    return dockspace_id;
}

void UiManager::initDockspace(const ImGuiID& dockspace_id, const ImGuiViewport* viewport)
{
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

    // Main dockspace: left panel and a "main" area
    ImGuiID dock_id_left, dock_id_main;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.2f, &dock_id_left, &dock_id_main);

    // "main" area: right panel and a "center" area
    ImGuiID dock_id_right, dock_id_center;
    ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.25f, &dock_id_right, &dock_id_center);

    // "center" area: viewport on top, console on bottom
    ImGuiID dock_id_viewport, dock_id_console;
    ImGui::DockBuilderSplitNode(dock_id_center, ImGuiDir_Down, 0.25f, &dock_id_console, &dock_id_viewport);

    // Left panel: hierarchy on top, properties on bottom
    ImGuiID dock_id_hierarchy, dock_id_properties;
    ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Down, 0.5f, &dock_id_properties, &dock_id_hierarchy);

    // Dock windows
    ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_hierarchy);
    ImGui::DockBuilderDockWindow("Entity properties", dock_id_properties);
    ImGui::DockBuilderDockWindow("Audio control", dock_id_properties);
    ImGui::DockBuilderDockWindow("Viewport", dock_id_viewport);
    ImGui::DockBuilderDockWindow("Console", dock_id_console);

    ImGui::DockBuilderGetNode(dock_id_viewport)->LocalFlags |= ImGuiDockNodeFlags_AutoHideTabBar;

    ImGui::DockBuilderFinish(dockspace_id);
}

void UiManager::initConsole() {
    m_oldCout = std::cout.rdbuf();
    m_consoleBuffer = std::make_unique<ImGuiConsoleBuffer>(m_oldCout, m_consoleItems);
    std::cout.rdbuf(m_consoleBuffer.get());
    std::cerr.rdbuf(m_consoleBuffer.get());
}

void UiManager::setDarkTheme()
{
    auto &style = ImGui::GetStyle();
    auto &colors = style.Colors;

    // geometry & spacing
    style.FramePadding = ImVec2(6, 4);
    style.CellPadding = ImVec2(4, 2);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    // borders & rounding
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    // core palette (complete)
    colors[ImGuiCol_Text] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.12f, 0.96f);
    colors[ImGuiCol_Border] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);

    colors[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);

    colors[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.09f, 0.07f, 1.00f);

    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);

    colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.60f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);

    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);

    colors[ImGuiCol_Header] = ImVec4(0.22f, 0.32f, 0.45f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.38f, 0.55f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.45f, 0.65f, 1.00f);

    colors[ImGuiCol_Separator] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

    colors[ImGuiCol_ResizeGrip] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);

    // docking
    colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.40f, 0.60f, 0.40f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);

    // plots
    colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.60f, 0.80f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.60f, 0.76f, 0.92f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.35f, 0.55f, 0.75f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.55f, 0.72f, 0.90f, 1.00f);

    // tables
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);

    // selection & drag/drop
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.40f, 0.60f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.40f, 0.60f, 0.80f, 0.90f);

    // navigation & modal
    colors[ImGuiCol_NavHighlight] = ImVec4(0.40f, 0.60f, 0.80f, 0.30f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.40f, 0.60f, 0.80f, 0.25f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.55f);
}

void UiManager::shutdownConsole()
{
    if (m_oldCout)
    {
        std::cout.rdbuf(m_oldCout);
        std::cerr.rdbuf(m_oldCout);
        m_oldCout = nullptr;
    }

    m_consoleBuffer.reset();
}

void UiManager::beginFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // Configurer ImGuizmo pour la frame courante
    ImGuizmo::BeginFrame();
    handleGizmoInput();
}

void UiManager::drawMainMenuBar()
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 5.f);
    ImGui::PushFont(io.Fonts->Fonts[0]);
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu((std::string(ICON_FA_FILE) + " File").c_str()))
        {
            if (ImGui::MenuItem((std::string(ICON_FA_WINDOW_CLOSE) + " Exit").c_str(), "Ctrl+Q"))
            {
                closeApp = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu((std::string(ICON_FA_TASKS) + " Window").c_str()))
        {
            ImGui::EndMenu();
        }
        if(ImGui::Button((std::string(ICON_FA_PLUS) + " Settings").c_str()))
        {
            showSettings = !showSettings;
        }
        if(ImGui::Button("Reload engine"))
        {
            reloadApp = true;
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::PopFont();
    ImGui::PopStyleVar();
}

void UiManager::drawViewport()
{
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar);
    {
        const ImGuiIO& io = ImGui::GetIO();
        ImGui::PushFont(io.Fonts->Fonts[0]);
        ImGui::BeginMenuBar();
        {
            if (ImGui::Button((std::string(ICON_FA_PLAY) + " Play").c_str()))
            {
                simulationPaused = false;
                pRHI->setSimulationStarted(true);
                if(!simulationStarted)
                {
                    simulationStarted = true;
                    physicsSystem.setRegistry(m_registry);
                    physicsSystem.startSimulation();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button((std::string(ICON_FA_PAUSE) + " Pause").c_str()))
            {
                simulationPaused = !simulationPaused;
                if(simulationPaused)
                {
                    pRHI->setSimulationStarted(false);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button((std::string(ICON_FA_STOP) + " Stop").c_str()) && simulationStarted)
            {
                simulationStarted = false;
                pRHI->setSimulationStarted(false);
                physicsSystem.stopSimulation();
            }
            ImGui::SameLine();
            std::string simStatus = "Simulation status: ";
            if (simulationStarted)
                simStatus += "Playing";
            else
                simStatus += "Stopped";

            if (simulationPaused && simulationStarted)
                simStatus = "Simulation status: Paused";
            ImGui::Text(simStatus.c_str());
        }
        ImGui::EndMenuBar();
        ImGui::PopFont();
        if(simulationStarted && !simulationPaused)
        {
            // Run script
            if (m_registry)
                scriptSystem.update(m_registry, io.DeltaTime);
            else
                std::cout << "no registry !" << std::endl;

            physicsSystem.update(io.DeltaTime);
        }

        ImVec2 availSize = ImGui::GetContentRegionAvail();

        // Check if viewport size changed (with a small threshold to avoid floating point issues)
        bool sizeChanged = std::abs(availSize.x - m_viewportSize.x) > 1.0f ||
                           std::abs(availSize.y - m_viewportSize.y) > 1.0f;

        if (sizeChanged && availSize.x > 0 && availSize.y > 0)
        {
            m_viewportSize = availSize;
            pRHI->requestOffscreenResize(
                static_cast<uint32_t>(availSize.x),
                static_cast<uint32_t>(availSize.y)
            );
            goto loading;
        }

        if (*viewportTexture != VK_NULL_HANDLE && availSize.x > 0 && availSize.y > 0)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(*viewportTexture), availSize);
        }
        else
        {
            // Display placeholder text when no texture is available
            loading:
            ImGui::Text("Viewport loading...");
        }
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
        ImGuizmo::SetRect(
                windowPos.x + contentMin.x,
                windowPos.y + contentMin.y,
                availSize.x,
                availSize.y
        );

        if(!simulationStarted || simulationPaused)
        {
            drawGizmo();
        }
    }
    ImGui::End();
}

void UiManager::drawSettings()
{
    ImGui::Begin("Settings");
    {
        bool vsyncState = pRHI->isVSync();
        if(ImGui::Checkbox("V-Sync", &vsyncState))
        {
            pRHI->setVSync(!pRHI->isVSync());
        }
        ImGui::SeparatorText("Scene infos");
        ImGui::ColorEdit4("ClearColor", (float*)&m_clearColor, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);
        ImGui::Text("Total entities : %d", meshData->size());
        ImGui::Text("Entities drawn : %d", pRHI->getDrawCount());

        ImGui::SeparatorText("Camera");
        ImGui::SliderFloat("Camera Speed", &camSpeed, 1.0f, 20.0f);

        ImGui::SeparatorText("Scene light");
        ImGui::ColorEdit3("Ambient color", glm::value_ptr(ambientColor), ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);
        ImGui::ColorEdit3("Light color", glm::value_ptr(lightColor), ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);
        ImGui::DragFloat3("Light direction", glm::value_ptr(lightDirection));

        ImGui::SeparatorText("Grid options");
        ImGui::InputFloat("Cell size",     &sceneInfos.cellSize);
        ImGui::InputFloat("Thick every",   &sceneInfos.thickEvery);
        ImGui::InputFloat("Fade distance", &sceneInfos.fadeDistance);
        ImGui::InputFloat("Line width",    &sceneInfos.lineWidth);

        ImGui::SeparatorText("Temporal Anti-Aliasing");
        static float blendAlpha = 0.10f;
        if(ImGui::InputFloat("Blend alpha", &blendAlpha, 0.01f, 0.10f))
        {
            pRHI->setTAABlendAlpha(blendAlpha);
        }

        auto& io = ImGui::GetIO();
        ImGui::SeparatorText("Application infos");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::Text("Viewport size: %.0f x %.0f", m_viewportSize.x, m_viewportSize.y);
    }
    ImGui::End();
}

void UiManager::drawSceneHierarchy()
{
    ImGui::Begin("Scene Hierarchy");
    {
        if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (const auto &entity: *meshData)
            {
                const bool selected = (m_selectedEntityID == entity.id);
                std::string label = std::string(ICON_FA_CUBE) + " " + entity.name;

                if (ImGui::Selectable(label.c_str(), selected))
                {
                    m_selectedEntityID = entity.id;
                }
            }
            ImGui::TreePop();
        }
        glm::vec3 frontOfCam = cameraRef->position + cameraRef->front * 4.0f;
        if (ImGui::BeginMenu("Add shape"))
        {
            if(ImGui::MenuItem("Cone"))
            {
                pRHI->spawnCone(frontOfCam);
            }
            if(ImGui::MenuItem("Cube"))
            {
                pRHI->spawnCube(frontOfCam);
            }
            if(ImGui::MenuItem("Cylinder"))
            {
                pRHI->spawnCylinder(frontOfCam);
            }
            if(ImGui::MenuItem("Icosphere"))
            {
                pRHI->spawnIcosphere(frontOfCam);
            }
            if(ImGui::MenuItem("Sphere"))
            {
                pRHI->spawnSphere(frontOfCam);
            }
            if(ImGui::MenuItem("Suzanne"))
            {
                pRHI->spawnSuzanne(frontOfCam);
            }
            if(ImGui::MenuItem("Torus"))
            {
                pRHI->spawnTorus(frontOfCam);
            }
            ImGui::EndMenu();
        }

        if (ImGui::Button("Add asset"))
        {
            const char *filters[] = { "*.obj", "*.gltf" };
            if (auto path = tinyfd_openFileDialog("Choose an asset", std::filesystem::current_path().string().c_str(), 2, filters, "3D Object files", 0); path != nullptr)
            {
                pRHI->spawnAsset(path, frontOfCam);
            }
        }

        // Delete selected entity
        if (m_selectedEntityID != UINT32_MAX)
        {
            if (ImGui::Button("Remove selected") || ImGui::IsKeyPressed(ImGuiKey_Delete))
            {
                auto it = std::ranges::find_if(*meshData, [&](const rhi::vulkan::Mesh& m){ return m.id == m_selectedEntityID; });

                if (it != meshData->end())
                {
                    uint32_t idx = static_cast<uint32_t>(std::distance(meshData->begin(), it));
                    pRHI->removeMesh(idx);
                    m_selectedEntityID = UINT32_MAX;
                }
            }
        }

        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
        {
            m_selectedEntityID = UINT32_MAX;
        }

    }
    ImGui::End();
}

void UiManager::drawEntityProperties()
{
    for (auto& entity : *meshData)
    {
        entity.transform = m_registry->getComponent<TransformComponent>(entity.id);
        entity.transform.rotation.Normalize();
        entity.physics   = m_registry->getComponent<PhysicsComponent>(entity.id);
    }
    ImGui::Begin("Entity properties");
    {
        if (m_selectedEntityID == UINT32_MAX)
        {
            ImGui::TextDisabled("No entity selected");
            ImGui::End();
            return;
        }

        // Always find by ID — never index meshData with a packed EntityID
        rhi::vulkan::Mesh *mesh = pRHI->findMesh(m_selectedEntityID);
        if (!mesh)
        {
            m_selectedEntityID = UINT32_MAX;
            ImGui::End();
            return;
        }

        auto &tc = m_registry->getComponent<TransformComponent>(m_selectedEntityID);

        // Name
        char buffer[256];
        strncpy(buffer, mesh->name.c_str(), sizeof(buffer));
        if (ImGui::InputText("Name", buffer, sizeof(buffer)))
            mesh->name = std::string(buffer);

        ImGui::SeparatorText("Transform");

        // Transform
        DrawVec3Control("Position", tc.position);
        if(DrawVec3Control("Rotation", tc.eulerRadians) && (!simulationStarted || simulationPaused))
        {
            glm::quat q = glm::quat(glm::vec3(tc.eulerRadians.x, tc.eulerRadians.y, tc.eulerRadians.z));
            tc.rotation = Quaternion(q.w, q.x, q.y, q.z);
        }
        DrawVec3Control("Scale", tc.scale);

        ImGui::SeparatorText("Texture");

        // Texture
        if (ImGui::Button("Add texture"))
        {
            const char *filters[] = {"*.png", "*.jpg", "*.jpeg"};
            char *path = tinyfd_openFileDialog("Choose a texture", std::filesystem::current_path().string().c_str(), 3, filters, "Image files", 0);
            if (path)
                pRHI->addTexture(m_selectedEntityID, path);
        }

        // LOD
        if (mesh->hasTexture() && mesh->hasMipmaps())
        {
            static float lodLevel = 0.0f;
            if (ImGui::SliderFloat("LOD Bias", &lodLevel, 0.0f, mesh->texture()->getMipLevels()))
                mesh->texture()->setLodLevel(lodLevel);
        }

        ImGui::SeparatorText("Physics");

        auto& physics = m_registry->getComponent<PhysicsComponent>(m_selectedEntityID);
        const char* motionTypeLabels[] =
        {
            "Static",
            "Dynamic",
            "Kinematic"
        };
        const char* layerLabels[] =
        {
            "Non moving",
            "Moving"
        };
        const char* bodyShapes[] =
        {
            "Cube",
            "Sphere",
            "Cylinder",
            "Capsule"
        };
        int currentMotion = static_cast<int>(physics.motionType);
        int currentLayer = static_cast<int>(physics.layers);
        int currentShape = static_cast<int>(physics.shapeType);
        ImGuiIO& io = ImGui::GetIO();
        ImGui::PushFont(io.Fonts->Fonts[0]);
        if (ImGui::Combo("Motion Type", &currentMotion, motionTypeLabels, IM_ARRAYSIZE(motionTypeLabels)))
        {
            physics.motionType = static_cast<EMotionType>(currentMotion);
        }
        if (ImGui::Combo("Layer", &currentLayer, layerLabels, IM_ARRAYSIZE(layerLabels)))
        {
            physics.layers = static_cast<ELayers>(currentLayer);
        }
        if (ImGui::Combo("Shape type", &currentShape, bodyShapes, IM_ARRAYSIZE(bodyShapes)))
        {
            physics.shapeType = static_cast<EShapeType>(currentShape);
        }
        ImGui::PopFont();

        ImGui::SeparatorText("Scripting");
        ImGui::PushFont(io.Fonts->Fonts[0]);
        if(ImGui::Button((std::string(ICON_FA_CODE) + " Add script").c_str()))
        {
            std::string modelPath = std::string(PROJECT_ROOT) + "/Engine/Core/Scripting/ScriptModel.cpp";
            std::string outputDir = std::string(PROJECT_ROOT) + "/Scripts";
            std::string outputPath = outputDir + "/Script" + std::to_string(mesh->id) + ".cpp";

            // Créer le répertoire Scripts s'il n'existe pas
            std::filesystem::create_directories(outputDir);

            std::ifstream scriptFileModel(modelPath, std::ios::binary);
            if (!scriptFileModel.is_open())
            {
                std::cerr << "Failed to open script model: " << modelPath
                          << " (cwd: " << std::filesystem::current_path() << ")" << std::endl;
            }
            else
            {
                // Read the model content as a string
                std::string modelContent((std::istreambuf_iterator<char>(scriptFileModel)),
                                          std::istreambuf_iterator<char>());

                // Replace the placeholder with the actual script name
                const std::string scriptName = "Script" + std::to_string(mesh->id);
                const std::string placeholder = "SCRIPT_NAME";
                auto pos = modelContent.find(placeholder);
                if (pos != std::string::npos)
                {
                    modelContent.replace(pos, placeholder.size(), scriptName);
                }

                std::ofstream scriptFile(outputPath, std::ios::binary);
                if (!scriptFile.is_open())
                {
                    std::cerr << "Failed to create script file: " << outputPath << std::endl;
                }
                else
                {
                    scriptFile << modelContent;
                    if (scriptFile.good())
                        std::cout << "Script created: " << outputPath << std::endl;
                    else
                        std::cerr << "Error writing script file: " << outputPath << std::endl;
                }
            }
        }
        ImGui::PopFont();

        if(!simulationStarted || simulationPaused)
        {
            ImGui::SeparatorText("Gizmo controls");
            drawGizmoControls();
        }

    }
    ImGui::End();
}

void UiManager::drawAudioControl()
{
    ImGui::Begin("Audio control");
    {
        audioSystem->update();
        int i = 0;
        for(auto& source : audioSources)
        {
            bool isPlaying = source->isPlaying();
            bool isLooping = source->isLooping();
            bool isSpatialized = source->isSpatialized();
            DrawVec3Control(std::string("Audio source " + std::to_string(i)).c_str(), source->getPosition());
            if(ImGui::Checkbox("Play", &isPlaying))
            {
                if(source->isPlaying())
                {
                    source->pause();
                }
                else
                {
                    source->play();
                }
            }
            if(ImGui::Checkbox("Loop", &isLooping))
            {
                source->setLooping(!source->isLooping());
            }
            if(ImGui::Checkbox("Spatialize", &isSpatialized))
            {
                source->setSpatialized(!source->isSpatialized());
            }
            i++;
        }
        ImGui::SeparatorText("Options");
        if(ImGui::Button("Add audio source"))
        {
            const char* filters[] = { "*.mp3", "*.ogg", "*.wav", "*.flac", "*.midi" };
            auto path = tinyfd_openFileDialog("Add an audio source", std::filesystem::current_path().string().c_str(), 5, filters, "Audio files", 0);
            if(path != nullptr)
            {
                audioSources.push_back(audioSystem->loadAudio(path));
                audioSources.back()->setVolume(1.f);
                audioSources.back()->setPitch(1.f);
            }
        }
    }
    ImGui::End();
}

void UiManager::drawConsole()
{
    static char inputBuffer[256];
    static bool autoScroll = true;
    static bool scrollToBottom = false;
    ImGui::Begin("Console");
    {
        if (ImGui::SmallButton("Clear"))
        {
            m_consoleItems.clear();
        }
        ImGui::SameLine();
        bool copyToClipboard = ImGui::SmallButton("Copy");

        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_HorizontalScrollbar))
        {
            if (ImGui::BeginPopupContextWindow())
            {
                if (ImGui::Selectable("Clear"))
                {
                    m_consoleItems.clear();
                }
                ImGui::EndPopup();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
            if (copyToClipboard)
            {
                ImGui::LogToClipboard();
            }
            for (const auto& item : m_consoleItems)
            {
                ImGui::TextUnformatted(item.c_str());
            }
            if (copyToClipboard)
            {
                ImGui::LogFinish();
            }

            if (scrollToBottom || (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
                ImGui::SetScrollHereY(1.0f);
            scrollToBottom = false;

            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
        ImGui::Separator();

        // Command-line
        bool reclaimFocus = false;
        ImGuiInputTextFlags inputTextFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll;
        if (ImGui::InputText("Input", inputBuffer, IM_COUNTOF(inputBuffer), inputTextFlags))
        {
            m_consoleItems.emplace_back(inputBuffer);
            inputBuffer[0] = '\0';
            reclaimFocus = true;
        }

        ImGui::SetItemDefaultFocus();
        if (reclaimFocus)
        {
            ImGui::SetKeyboardFocusHere(-1);
        }
    }
    ImGui::End();
}

void UiManager::drawBottomBar()
{
    constexpr ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_MenuBar;

    const float height = ImGui::GetFrameHeight();

    // Position the status bar at the bottom of the main viewport
    if (ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, height, window_flags))
    {
        if (ImGui::BeginMenuBar())
        {
            // --- Left Side: Engine Info ---
            ImGui::Text(config::engineName.data());
            ImGui::SameLine();
            ImGui::Text(config::engineVersion.data());
            ImGui::SameLine();

            // --- Right Side: All those texts ---
            constexpr float internPadding = 8.0f;
            std::string rightText = std::string(config::buildType) +
                                    " [" + config::architecture.data() + "] | " +
                                    config::platform.data();

            float rightTextWidth = ImGui::CalcTextSize(rightText.c_str()).x;
            float avail = ImGui::GetContentRegionAvail().x;
            float align_pos = ImGui::GetCursorPosX() + avail - rightTextWidth - internPadding;

            ImGui::SameLine(align_pos);
            ImGui::Text("%s", rightText.c_str());

            ImGui::EndMenuBar();
        }
        ImGui::End();
    }
}

void UiManager::uiUpdate()
{
    if (glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) != 0)
    {
        ImGui_ImplGlfw_Sleep(10);
    }

    beginFrame();

    const ImGuiViewport* viewport = setupViewport();
    const ImGuiID dockspace_id = getDockspaceID();
    static bool dockspaceInitialized = false;
    if (!dockspaceInitialized) {
        initDockspace(dockspace_id, viewport);
        dockspaceInitialized = true;
    }

    drawViewport();
    if(showSettings)
    {
        drawSettings();
    }
    drawSceneHierarchy();
    drawEntityProperties();
    drawAudioControl();
    drawConsole();
    drawBottomBar();

    if (showDemoWindow)
    {
        ImGui::ShowDemoWindow(&showDemoWindow);
    }
    sceneInfos.clearColor = m_clearColor;
    sceneInfos.ambientColor = ambientColor;
    sceneInfos.lightColor = lightColor;
    sceneInfos.lightDirection = lightDirection;
    pRHI->updateSceneUBO(&sceneInfos, cameraRef->position);
    if(m_viewportSize.x != 0 && m_viewportSize.y != 0)
    {
        pRHI->updateCameraUBO(cameraRef->update(m_viewportSize.x / m_viewportSize.y, camSpeed));
    }
}

void UiManager::drawGizmoControls()
{
    ImGui::Text("Gizmo");

    const char* gizmoOpLabels[] =
    {
        "Translate (W)",
        "Rotate (E)",
        "Scale (R)"
    };
    const char* gizmoModeLabels[] =
    {
        "Local",
        "World"
    };

    int currentMode = 0;
    switch(m_currentGizmoMode)
    {

        case ImGuizmo::LOCAL: currentMode = 0; break;
        case ImGuizmo::WORLD: currentMode = 1; break;
        default:                               break;
    }

    int currentOp = 0;
    switch (m_currentGizmoOperation)
    {
        case ImGuizmo::TRANSLATE: currentOp = 0; break;
        case ImGuizmo::ROTATE:    currentOp = 1; break;
        case ImGuizmo::SCALE:     currentOp = 2; break;
        default:                                 break;
    }

    if (ImGui::Combo("Operation", &currentOp, gizmoOpLabels, IM_ARRAYSIZE(gizmoOpLabels)))
    {
        switch (currentOp)
        {
            case 0: m_currentGizmoOperation = ImGuizmo::TRANSLATE; break;
            case 1: m_currentGizmoOperation = ImGuizmo::ROTATE;    break;
            case 2: m_currentGizmoOperation = ImGuizmo::SCALE;     break;
            default:                                               break;
        }
    }
    if(m_currentGizmoOperation != ImGuizmo::SCALE)
    {
        if (ImGui::Combo("Mode", &currentMode, gizmoModeLabels, IM_ARRAYSIZE(gizmoModeLabels)))
        {
            switch (currentMode)
            {
                case 0:
                    m_currentGizmoMode = ImGuizmo::LOCAL;
                    break;
                case 1:
                    m_currentGizmoMode = ImGuizmo::WORLD;
                    break;
                default:
                    break;
            }
        }
    }

    ImGui::Checkbox("Snap", &m_useSnap);

    // Afficher les contrôles de snapping selon l'opération courante
    if (m_useSnap)
    {
        switch (m_currentGizmoOperation)
        {
        case ImGuizmo::TRANSLATE:
            ImGui::DragFloat3("Snapping", m_snapTranslation);
            break;
        case ImGuizmo::ROTATE:
            ImGui::DragFloat("Snap Rotation", &m_snapRotation, 1.0f, 0.0f, 360.0f);
            break;
        case ImGuizmo::SCALE:
            ImGui::DragFloat("Snap Scale", &m_snapScale, 0.01f, 0.0f, 1.0f);
            break;
        default:
            break;
        }
    }
}

void UiManager::handleGizmoInput()
{
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) || ImGuizmo::IsUsing())
        return;
    if(simulationStarted && !simulationPaused)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_W))
        m_currentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_E))
        m_currentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R))
        m_currentGizmoOperation = ImGuizmo::SCALE;
}

void UiManager::drawGizmo()
{
    if (m_selectedEntityID == UINT32_MAX)
        return;

    auto it = std::ranges::find_if(*meshData, [&](const rhi::vulkan::Mesh& m){ return m.id == m_selectedEntityID; });

    if (it == meshData->end())
    {
        m_selectedEntityID = UINT32_MAX;
        return;
    }

    glm::mat4 view = cameraRef->getViewMatrix();
    glm::mat4 projection = cameraRef->getProjectionMatrix();
    projection[1][1] *= -1.0f;

    auto& selectedMesh = *it;
    glm::mat4 modelMatrix = selectedMesh.getTransform();

    float modelData[16];
    memcpy(modelData, glm::value_ptr(modelMatrix), sizeof(float) * 16);

    float* snapPtr = nullptr;
    float snapValues[3] = {};
    if (m_useSnap)
    {
        switch (m_currentGizmoOperation)
        {
        case ImGuizmo::TRANSLATE:
            memcpy(snapValues, m_snapTranslation, sizeof(snapValues));
            break;
        case ImGuizmo::ROTATE:
            snapValues[0] = snapValues[1] = snapValues[2] = m_snapRotation;
            break;
        case ImGuizmo::SCALE:
            snapValues[0] = snapValues[1] = snapValues[2] = m_snapScale;
            break;
        default: break;
        }
        snapPtr = snapValues;
    }

    auto& tc = m_registry->getComponent<TransformComponent>(m_selectedEntityID);

    glm::vec3 oldRotation = { tc.eulerRadians.x, tc.eulerRadians.y, tc.eulerRadians.z };
    glm::vec3 oldScale    = { tc.scale.x,        tc.scale.y,        tc.scale.z        };

    float deltaData[16];
    glm::mat4 identityDelta = glm::mat4(1.0f);
    memcpy(deltaData, glm::value_ptr(identityDelta), sizeof(float) * 16);

    // Forcer LOCAL pour SCALE — le scale world n'est pas supporté par ImGuizmo
    ImGuizmo::MODE activeMode = (m_currentGizmoOperation == ImGuizmo::SCALE)
        ? ImGuizmo::LOCAL
        : m_currentGizmoMode;

    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(projection),
        m_currentGizmoOperation,
        activeMode,
        modelData,
        deltaData,
        snapPtr
    );

    if (!ImGuizmo::IsUsing())
        return;

    glm::mat4 newModel, delta;
    memcpy(glm::value_ptr(newModel), modelData,  sizeof(float) * 16);
    memcpy(glm::value_ptr(delta),    deltaData,   sizeof(float) * 16);

        switch (m_currentGizmoOperation)
        {
        case ImGuizmo::TRANSLATE:
        {
            tc.position = { newModel[3].x, newModel[3].y, newModel[3].z };
            break;
        }
        case ImGuizmo::ROTATE:
        {
            glm::vec3 position, scale, skew;
            glm::vec4 perspective;
            glm::quat rotation;
            glm::decompose(newModel, scale, rotation, position, skew, perspective);

            tc.position = { position.x, position.y, position.z };
            tc.rotation = Quaternion(rotation.w, rotation.x, rotation.y, rotation.z);
            tc.rotation.Normalize();

            // Optional editor display
            tc.eulerRadians = {
                glm::eulerAngles(rotation).x,
                glm::eulerAngles(rotation).y,
                glm::eulerAngles(rotation).z
            };
            break;
        }
        case ImGuizmo::SCALE:
        {
            glm::vec3 deltaScale =
            {
                glm::length(glm::vec3(delta[0])),
                glm::length(glm::vec3(delta[1])),
                glm::length(glm::vec3(delta[2]))
            };
            glm::vec3 newScale = oldScale * deltaScale;
            tc.scale        = { newScale.x,    newScale.y,    newScale.z    };
            tc.eulerRadians = { oldRotation.x, oldRotation.y, oldRotation.z };
            break;
        }
        default: break;
    }
}

}
