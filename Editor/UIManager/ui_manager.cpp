#include "ui_manager.hpp"

// Externals
#include <font_awesome.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tinyfiledialogs.h>

// STL
#include <cmath>

#include <Editor/Camera/camera.hpp>
#include <Editor/Helpers.hpp>
#include <Log/Log.hpp>

namespace gcep
{

UiManager::UiManager(GLFWwindow* window, ImGui_ImplVulkan_InitInfo initInfo) : physicsSystem(PhysicsSystem::getInstance())
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

    constexpr float baseFontSize = 16.0f;
    constexpr float iconFontSize = baseFontSize * 2.0f / 3.0f;
    io.FontDefault = io.Fonts->AddFontFromFileTTF("TestTextures/Roboto-Regular.ttf", baseFontSize);
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

static bool DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f) {
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
    ImGui::DockSpace(dockspace_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar);
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
    ImGui::DockBuilderDockWindow("Viewport", dock_id_viewport);
    ImGui::DockBuilderDockWindow("Scene infos", dock_id_right);
    ImGui::DockBuilderDockWindow("Audio control", dock_id_right);
    ImGui::DockBuilderDockWindow("Console", dock_id_console);

    ImGui::DockBuilderFinish(dockspace_id);
}

void UiManager::initConsole() {
    m_oldCout = std::cout.rdbuf();
    m_consoleBuffer = std::make_unique<ImGuiConsoleBuffer>(m_oldCout, m_consoleItems);
    std::cout.rdbuf(m_consoleBuffer.get());
    std::cerr.rdbuf(m_consoleBuffer.get());
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
                // TODO: Add stuff
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu((std::string(ICON_FA_TASKS) + " Window").c_str()))
        {
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::PopFont();
    ImGui::PopStyleVar();
}

void UiManager::drawViewport()
{
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
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

        drawGizmo(cameraRef);
    }
    ImGui::End();
}

void UiManager::drawCodeEditor()
{
    ImGui::Begin("Code Editor", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        // Bouton pour récupérer le texte et l’exécuter (exemple Lua/C++)
        if (ImGui::Button("Run Script"))
        {
            std::string code = editor.GetText();
            // Ici tu peux envoyer 'code' à Lua ou compiler en DLL
            printf("Script:\n%s\n", code.c_str());
        }
        editor.Render("TextEditor");
    }
    ImGui::End();
}

void UiManager::drawSceneInfos()
{
    ImGui::Begin("Scene infos");
    {
        bool vsyncState = pRHI->isVSync();
        if(ImGui::Checkbox("V-Sync", &vsyncState))
        {
            pRHI->setVSync(!pRHI->isVSync());
        }
        ImGui::Checkbox("Demo Window", &showDemoWindow);
        ImGui::SeparatorText("Scene infos");
        ImGui::ColorEdit4("ClearColor", (float*)&m_clearColor, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);
        ImGui::Text("Total entities : %d", meshData->size());
        ImGui::Text("Entities drawn : %d", pRHI->getDrawCount());

        ImGui::SeparatorText("Camera");
        ImGui::SliderFloat("Camera Speed", &camSpeed, 2.0f, 5.0f);
        ImGui::DragFloat3("Camera position", glm::value_ptr(cameraRef->position));
        ImGui::DragFloat3("Camera front vector", glm::value_ptr(cameraRef->front), 0.1f, -1.0f, 1.0f);
        ImGui::DragFloat("Camera yaw", &cameraRef->yaw);
        ImGui::DragFloat("Camera pitch", &cameraRef->pitch);

        ImGui::SeparatorText("Lights & normals");
        ImGui::ColorEdit3("Ambient color", glm::value_ptr(ambientColor), ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);
        ImGui::ColorEdit3("Light color", glm::value_ptr(lightColor), ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);
        ImGui::DragFloat("Shininess", &shininess, 0.5f, 0.0f, 64.0f, "%.2f");
        DrawVec3Control("Light direction", lightDirection);

        ImGui::SeparatorText("Grid options");
        ImGui::InputFloat("Cell size", &gridPC.cellSize);
        ImGui::InputFloat("Thick every", &gridPC.thickEvery);
        ImGui::InputFloat("Fade distance", &gridPC.fadeDistance);
        ImGui::InputFloat("Line width", &gridPC.lineWidth);

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
            for (auto &entity: *meshData)
            {
                bool is_selected = (m_selectedEntityID == entity.id);
                std::string name = std::string(ICON_FA_CUBE) + " ";
                name += entity.name;
                if (ImGui::Selectable(name.c_str(), is_selected))
                {
                    m_selectedEntityID = entity.id;
                }
            }
            ImGui::TreePop();
        }
        if (ImGui::Button("Spawn cube"))
        {
            pRHI->spawnCube(cameraRef->position + cameraRef->front * 4.0f);
        }
        if (ImGui::Button("Add asset"))
        {
            const char *filters[] = { "*.obj" };
            if (auto path = tinyfd_openFileDialog("Choose an asset", std::filesystem::current_path().string().c_str(), 1, filters, "3D Object files", 0); path != nullptr)
            {
                pRHI->spawnAsset(path, cameraRef->position + cameraRef->front * 4.0f);
            }
        }

        // Deselect if clicking in empty space
        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
        {
            m_selectedEntityID = std::numeric_limits<uint32_t>::max();
        }

    }
    ImGui::End();
}

void UiManager::drawEntityProperties()
{
    ImGui::Begin("Entity properties");
    {
        if (m_selectedEntityID != std::numeric_limits<ECS::EntityID>::max())
        {
            static float lodLevel = 0.0f;
            auto &entities = *meshData;
            auto &entity = entities[m_selectedEntityID];
            auto &[position, rotation, scale] = m_registry->getComponent<rhi::vulkan::TransformComponent>(m_selectedEntityID);
            char buffer[256];
            strncpy(buffer, entity.name.c_str(), sizeof(buffer));
            if (ImGui::InputText("Name", buffer, sizeof(buffer)))
            {
                entity.name = std::string(buffer);
            }
            DrawVec3Control("Translation", position);
            DrawVec3Control("Rotation", rotation);
            DrawVec3Control("Scale", scale);
            if (entity.hasTexture() && entity.hasMipmaps()) {

                if (ImGui::DragFloat("LOD level", &lodLevel, 0.1f, 0.0f, static_cast<float>(entity.texture()->getMipLevels())))
                {
                    entity.texture()->setLodLevel(lodLevel);
                }
            }
            if (ImGui::Button("Add texture"))
            {
                const char *filters[] = {"*.png", "*.jpg", "*.jpeg"};
                char *path = tinyfd_openFileDialog("Choose a texture",
                                                   std::filesystem::current_path().string().c_str(), 3, filters,
                                                   "Image files", 0);
                if (path != nullptr)
                {
                    pRHI->addTexture(m_selectedEntityID, path);
                }
            }
            entity.transform = m_registry->getComponent<rhi::vulkan::TransformComponent>(m_selectedEntityID);

            drawGizmoControls();
        }
        else
        {
            ImGui::Text("Select an entity to see its properties.");
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
            DrawVec3Control(("Audio source " + std::to_string(i)), const_cast<glm::vec3&>(source->getPosition()));
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
        ImGuiInputTextFlags inputTextFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
        if (ImGui::InputText("Input", inputBuffer, IM_COUNTOF(inputBuffer), inputTextFlags))
        {
            m_consoleItems.push_back(inputBuffer);
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

void UiManager::drawSimulationPanel()
{
    static bool open = true;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("Simple overlay", &open, window_flags);
    {
        ImGui::Text("Overlay");
    }
    ImGui::End();
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
    drawSceneInfos();
    drawSceneHierarchy();
    drawEntityProperties();
    drawAudioControl();
    drawConsole();

    if (showDemoWindow)
    {
        ImGui::ShowDemoWindow(&showDemoWindow);
    }
    sceneInfos.clearColor = m_clearColor;
    sceneInfos.ambientColor = ambientColor;
    sceneInfos.lightColor = lightColor;
    sceneInfos.lightDirection = lightDirection;
    sceneInfos.shininess = shininess;
    pRHI->updateSceneUBO(&sceneInfos, cameraRef->position);
    if(m_viewportSize.x != 0 && m_viewportSize.y != 0)
    {
        pRHI->updateCameraUBO(cameraRef->update(m_viewportSize.x / m_viewportSize.y, camSpeed));
    }
    gridPC.invView      = glm::inverse(cameraRef->getViewMatrix());
    gridPC.invProj      = glm::inverse(cameraRef->getProjectionMatrix());
    gridPC.viewProj     = cameraRef->getProjectionMatrix() * cameraRef->getViewMatrix();
    pRHI->setGridPC(&gridPC);
}

void UiManager::drawGizmoControls()
{
    ImGui::Text("Gizmo");
    if (ImGui::RadioButton("Translate (W)", m_currentGizmoOperation == ImGuizmo::TRANSLATE))
    {
        m_currentGizmoOperation = ImGuizmo::TRANSLATE;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate (E)", m_currentGizmoOperation == ImGuizmo::ROTATE))
    {
        m_currentGizmoOperation = ImGuizmo::ROTATE;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale (R)", m_currentGizmoOperation == ImGuizmo::SCALE))
    {
        m_currentGizmoOperation = ImGuizmo::SCALE;
    }

    if (m_currentGizmoOperation == ImGuizmo::SCALE)
    {
        ImGui::TextDisabled("Mode: Local (forced for Scale)");
    }
    else
    {
        if (ImGui::RadioButton("Local", m_currentGizmoMode == ImGuizmo::LOCAL))
            m_currentGizmoMode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (ImGui::RadioButton("World", m_currentGizmoMode == ImGuizmo::WORLD))
            m_currentGizmoMode = ImGuizmo::WORLD;
    }

    ImGui::Checkbox("Snap", &m_useSnap);

    // Afficher les contrôles de snapping selon l'opération courante
    if (m_useSnap)
    {
        switch (m_currentGizmoOperation)
        {
        case ImGuizmo::TRANSLATE:
            ImGui::DragFloat("Snapping X", &m_snapTranslation[0], 0.1f, 0.0f, 100.0f);
            ImGui::DragFloat("Snapping Y", &m_snapTranslation[1], 0.1f, 0.0f, 100.0f);
            ImGui::DragFloat("Snapping Z", &m_snapTranslation[2], 0.1f, 0.0f, 100.0f);
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

    if (ImGui::IsKeyPressed(ImGuiKey_W))
        m_currentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_E))
        m_currentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R))
        m_currentGizmoOperation = ImGuizmo::SCALE;
}

void UiManager::drawGizmo(Camera* camera)
{
    if (m_selectedEntityID == std::numeric_limits<uint32_t>::max())
        return;
    if (!meshData || m_selectedEntityID >= meshData->size())
        return;

    glm::mat4 view = cameraRef->getViewMatrix();
    glm::mat4 projection = cameraRef->getProjectionMatrix();
    projection[1][1] *= -1.0f;

    rhi::vulkan::VulkanMesh& selectedMesh = (*meshData)[m_selectedEntityID];
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

    glm::vec3 oldRotation = m_registry->getComponent<rhi::vulkan::TransformComponent>(m_selectedEntityID).rotation;
    glm::vec3 oldScale = m_registry->getComponent<rhi::vulkan::TransformComponent>(m_selectedEntityID).scale;

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

    if (ImGuizmo::IsUsing())
    {
        glm::mat4 newModel;
        memcpy(glm::value_ptr(newModel), modelData, sizeof(float) * 16);

        glm::mat4 delta;
        memcpy(glm::value_ptr(delta), deltaData, sizeof(float) * 16);

        switch (m_currentGizmoOperation)
        {
        case ImGuizmo::TRANSLATE:
        {
            m_registry->getComponent<rhi::vulkan::TransformComponent>(m_selectedEntityID).position = glm::vec3(newModel[3]);
            break;
        }
        case ImGuizmo::ROTATE:
        {
            m_registry->getComponent<rhi::vulkan::TransformComponent>(m_selectedEntityID).position = glm::vec3(newModel[3]);

            glm::vec3 scale;
            scale.x = glm::length(glm::vec3(newModel[0]));
            scale.y = glm::length(glm::vec3(newModel[1]));
            scale.z = glm::length(glm::vec3(newModel[2]));

            glm::mat3 r;
            r[0] = glm::vec3(newModel[0]) / scale.x;
            r[1] = glm::vec3(newModel[1]) / scale.y;
            r[2] = glm::vec3(newModel[2]) / scale.z;

            float sinX = glm::clamp(-r[2][1], -1.0f, 1.0f);
            m_registry->getComponent<rhi::vulkan::TransformComponent>(m_selectedEntityID).rotation.x = asinf(sinX);
            m_registry->getComponent<rhi::vulkan::TransformComponent>(m_selectedEntityID).rotation.y = atan2f(r[2][0], r[2][2]);
            m_registry->getComponent<rhi::vulkan::TransformComponent>(m_selectedEntityID).rotation.z = atan2f(r[0][1], r[1][1]);
            break;
        }
        case ImGuizmo::SCALE:
        {
            // En mode LOCAL, le delta de scale est dans l'espace local de l'objet
            // On extrait directement le scale du delta
            glm::vec3 deltaScale;
            deltaScale.x = glm::length(glm::vec3(delta[0]));
            deltaScale.y = glm::length(glm::vec3(delta[1]));
            deltaScale.z = glm::length(glm::vec3(delta[2]));

            m_registry->getComponent<rhi::vulkan::TransformComponent>(m_selectedEntityID).scale = oldScale * deltaScale;
            m_registry->getComponent<rhi::vulkan::TransformComponent>(m_selectedEntityID).rotation = oldRotation;
            break;
        }
        default: break;
        }
    }
}

}
