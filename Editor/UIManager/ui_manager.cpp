#include "ui_manager.hpp"

// Internals
#include <config.hpp>
#include <Editor/Camera/camera.hpp>
#include <Editor/Helpers.hpp>
#include <Log/log.hpp>
#include <Maths/quaternion.hpp>

// Externals
#include <font_awesome.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <tinyfiledialogs.h>

// STL
#include <cmath>
#include <fstream>

namespace gcep
{
    UiManager::UiManager(GLFWwindow* window, SLS::SceneManager* manager, bool& reload, bool& close)
        : m_physicsSystem(PhysicsSystem::getInstance()),
          m_reloadApp(reload),
          m_isRunning(close),
          m_contentBrowser(pl::ProjectLoader::instance().getProjectInfo().contentPath),
          m_currentScenePath(pl::ProjectLoader::instance().getProjectInfo().startScene.string())
    {
        m_window       = window;
        m_audioSystem  = gcep::AudioSystem::getInstance();
        m_sceneManager = manager;
        m_registry     = &m_sceneManager->current().getRegistry();
        initConsole();
        Log::info("UiManager initialized successfully");
    }

    UiManager::~UiManager()
    {
        shutdownConsole();
        Log::info("UiManager destroyed");
    }

    void UiManager::setCamera(Camera* pCamera)
    {
        m_cameraRef = pCamera;
    }

    void UiManager::setSceneManager(SLS::SceneManager* sceneManager)
    {
        m_sceneManager = sceneManager;
        m_registry     = &sceneManager->current().getRegistry();
    }

    void UiManager::setInfos(rhi::vulkan::InitInfos* infos)
    {
        m_pRHI          = infos->instance;
        m_meshData      = infos->meshData;
        m_viewportTexture = infos->ds;
        m_spotLights    = &(m_pRHI->getLightSystem().getSpotLights());
        m_pointLights   = &(m_pRHI->getLightSystem().getPointLights());
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Static helper — 3-component drag control
    // ─────────────────────────────────────────────────────────────────────────────

    static bool drawVec3Control(const std::string& label, Vector3<float>& values,
                                 float resetValue = 0.0f, float columnWidth = 100.0f)
    {
        bool valueChanged = false;
        ImGuiIO& io       = ImGui::GetIO();
        auto boldFont     = io.Fonts->Fonts[0];

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
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f,  1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
        ImGui::PushFont(boldFont);
        if (ImGui::Button("X", buttonSize)) { values.x = resetValue; valueChanged = true; }
        ImGui::PopFont();
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f")) valueChanged = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();

        // Y
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
        ImGui::PushFont(boldFont);
        if (ImGui::Button("Y", buttonSize)) { values.y = resetValue; valueChanged = true; }
        ImGui::PopFont();
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f")) valueChanged = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();

        // Z
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
        ImGui::PushFont(boldFont);
        if (ImGui::Button("Z", buttonSize)) { values.z = resetValue; valueChanged = true; }
        ImGui::PopFont();
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        if (ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f")) valueChanged = true;
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();
        ImGui::Columns(1);
        ImGui::PopID();

        return valueChanged;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Init / shutdown helpers
    // ─────────────────────────────────────────────────────────────────────────────

    ImGuiViewport* UiManager::setupViewport()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        return viewport;
    }

    ImGuiID UiManager::getDockspaceID()
    {
        constexpr ImGuiWindowFlags hostFlags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove     |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("DockSpaceHost", nullptr, hostFlags);
        ImGuiID dockspaceId = ImGui::GetID("MainDockspace");
        ImGui::DockSpace(dockspaceId, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
        drawMainMenuBar();
        ImGui::End();
        ImGui::PopStyleVar();
        return dockspaceId;
    }

    void UiManager::initDockspace(const ImGuiID& dockspaceId, const ImGuiViewport* viewport)
    {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

        ImGuiID dockIdLeft, dockIdMain;
        ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.2f, &dockIdLeft, &dockIdMain);

        ImGuiID dockIdRight, dockIdCenter;
        ImGui::DockBuilderSplitNode(dockIdMain, ImGuiDir_Right, 0.25f, &dockIdRight, &dockIdCenter);

        ImGuiID dockIdViewport, dockIdBottom;
        ImGui::DockBuilderSplitNode(dockIdCenter, ImGuiDir_Down, 0.25f, &dockIdBottom, &dockIdViewport);

        ImGuiID dockIdConsole, dockIdContent;
        ImGui::DockBuilderSplitNode(dockIdBottom, ImGuiDir_Right, 0.6f, &dockIdContent, &dockIdConsole);

        ImGuiID dockIdHierarchy, dockIdProperties;
        ImGui::DockBuilderSplitNode(dockIdLeft, ImGuiDir_Down, 0.5f, &dockIdProperties, &dockIdHierarchy);

        ImGui::DockBuilderDockWindow("Scene Hierarchy",  dockIdHierarchy);
        ImGui::DockBuilderDockWindow("Entity properties", dockIdProperties);
        ImGui::DockBuilderDockWindow("Audio control",    dockIdProperties);
        ImGui::DockBuilderDockWindow("Viewport",         dockIdViewport);
        ImGui::DockBuilderDockWindow("Console",          dockIdConsole);
        ImGui::DockBuilderDockWindow("ContentDrawer",    dockIdContent);

        ImGui::DockBuilderGetNode(dockIdViewport)->LocalFlags |= ImGuiDockNodeFlags_AutoHideTabBar;
        ImGui::DockBuilderFinish(dockspaceId);
    }

    void UiManager::initConsole()
    {
        m_oldCout       = std::cout.rdbuf();
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

    // ─────────────────────────────────────────────────────────────────────────────
    // Per-frame
    // ─────────────────────────────────────────────────────────────────────────────

    void UiManager::beginFrame()
    {
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
                if (ImGui::MenuItem((std::string(ICON_FA_FLOPPY_O) + " Save scene").c_str(), "Ctrl+S"))
                {
                    if (m_sceneManager && !m_currentScenePath.empty())
                    {
                        pl::ProjectLoader::instance().saveProject();
                        m_sceneManager->current().save(m_currentScenePath);
                    }
                }
                if (ImGui::MenuItem((std::string(ICON_FA_WINDOW_CLOSE) + " Exit").c_str(), "Ctrl+Q"))
                {
                    m_isRunning = false;
                    m_reloadApp = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu((std::string(ICON_FA_TASKS) + " Window").c_str()))
            {
                ImGui::EndMenu();
            }
            if (ImGui::Button((std::string(ICON_FA_PLUS) + " Settings").c_str()))
            {
                m_showSettings = !m_showSettings;
            }
            if (ImGui::Button("Reload engine"))
            {
                m_reloadApp = true;
            }
            ImGui::EndMainMenuBar();
        }
        ImGui::PopFont();
        ImGui::PopStyleVar();
    }

    void UiManager::drawViewport()
    {
        ImGui::Begin("Viewport", nullptr,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar);
        {
            const ImGuiIO& io = ImGui::GetIO();
            ImGui::PushFont(io.Fonts->Fonts[0]);
            ImGui::BeginMenuBar();
            {
                if (ImGui::Button((std::string(ICON_FA_PLAY) + " Play").c_str()))
                {
                    m_simulationState = SimulationState::PLAYING;
                    m_pRHI->setSimulationStarted(true);
                    m_physicsSystem.setRegistry(&SLS::SceneManager::instance().current().getRegistry());
                    m_physicsSystem.startSimulation();
                }
                ImGui::SameLine();
                if (ImGui::Button((std::string(ICON_FA_PAUSE) + " Pause").c_str()))
                {
                    if (m_simulationState == SimulationState::PLAYING)
                    {
                        m_simulationState = SimulationState::PAUSED;
                        m_pRHI->setSimulationStarted(false);
                    }
                    else if (m_simulationState == SimulationState::PAUSED)
                    {
                        m_simulationState = SimulationState::PLAYING;
                        m_pRHI->setSimulationStarted(true);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button((std::string(ICON_FA_STOP) + " Stop").c_str())
                    && m_simulationState != SimulationState::STOPPED)
                {
                    m_simulationState = SimulationState::STOPPED;
                    m_pRHI->setSimulationStarted(false);
                    m_physicsSystem.stopSimulation();
                }
                ImGui::SameLine();
                std::string simStatus = "Simulation status: ";
                switch (m_simulationState)
                {
                    case SimulationState::PLAYING: simStatus += "Playing"; break;
                    case SimulationState::PAUSED:  simStatus += "Paused";  break;
                    case SimulationState::STOPPED: simStatus += "Stopped"; break;
                }
                ImGui::Text(simStatus.c_str());
            }
            ImGui::EndMenuBar();
            ImGui::PopFont();

            if (m_simulationState == SimulationState::PLAYING)
            {
                auto& registry = SLS::SceneManager::instance().current().getRegistry();
                m_physicsSystem.update(io.DeltaTime);
            }

            ImVec2 availSize = ImGui::GetContentRegionAvail();

            bool sizeChanged = std::abs(availSize.x - m_viewportSize.x) > 1.0f ||
                               std::abs(availSize.y - m_viewportSize.y) > 1.0f;

            if (sizeChanged && availSize.x > 0 && availSize.y > 0)
            {
                m_viewportSize = availSize;
                m_pRHI->requestOffscreenResize(
                    static_cast<uint32_t>(availSize.x),
                    static_cast<uint32_t>(availSize.y)
                );
                goto loading;
            }

            if (*m_viewportTexture != VK_NULL_HANDLE && availSize.x > 0 && availSize.y > 0)
            {
                ImGui::Image(reinterpret_cast<ImTextureID>(*m_viewportTexture), availSize);
            }
            else
            {
                loading:
                ImGui::Text("Viewport loading...");
            }

            ImVec2 windowPos   = ImGui::GetWindowPos();
            ImVec2 contentMin  = ImGui::GetWindowContentRegionMin();
            ImGuizmo::SetRect(
                windowPos.x + contentMin.x,
                windowPos.y + contentMin.y,
                availSize.x,
                availSize.y
            );

            if (m_simulationState != SimulationState::PLAYING)
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
            auto& settings = SLS::SceneManager::instance().current().getSceneSettings();
            auto& ps       = pl::ProjectLoader::instance().getSettings();

            bool vsyncState = m_pRHI->isVSync();
            if (ImGui::Checkbox("V-Sync", &vsyncState))
            {
                m_pRHI->setVSync(!m_pRHI->isVSync());
                ps.vsync = !m_pRHI->isVSync();
                pl::ProjectLoader::instance().markDirty();
            }

            ImGui::SeparatorText("Scene infos");
            if (ImGui::ColorEdit4("ClearColor", ps.clearColor,
                ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel))
            {
                settings.clearColor = { ps.clearColor[0], ps.clearColor[1],
                                        ps.clearColor[2], ps.clearColor[3] };
                pl::ProjectLoader::instance().markDirty();
            }

            ImGui::Text("Total entities : %d", m_meshData->size());
            ImGui::Text("Entities drawn : %d", m_pRHI->getDrawCount());

            ImGui::SeparatorText("Camera");
            if (ImGui::SliderFloat("Camera Speed", &ps.cameraSpeed, 1.0f, 20.0f))
            {
                m_camSpeed = ps.cameraSpeed;
                pl::ProjectLoader::instance().markDirty();
            }

            ImGui::SeparatorText("Scene light");
            if (ImGui::ColorEdit3("Ambient color", ps.ambientColor,
                ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel))
            {
                settings.ambientColor = { ps.ambientColor[0], ps.ambientColor[1], ps.ambientColor[2] };
                pl::ProjectLoader::instance().markDirty();
            }
            if (ImGui::ColorEdit3("Light color", ps.lightColor,
                ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel))
            {
                settings.lightColor = { ps.lightColor[0], ps.lightColor[1], ps.lightColor[2] };
                pl::ProjectLoader::instance().markDirty();
            }
            if (ImGui::DragFloat3("Light direction", ps.lightDirection))
            {
                settings.lightDirection = { ps.lightDirection[0], ps.lightDirection[1], ps.lightDirection[2] };
                pl::ProjectLoader::instance().markDirty();
            }

            ImGui::SeparatorText("Grid options");
            if (ImGui::InputFloat("Cell size",     &ps.gridCellSize))     { m_sceneInfos.cellSize     = ps.gridCellSize;     pl::ProjectLoader::instance().markDirty(); }
            if (ImGui::InputFloat("Thick every",   &ps.gridThickEvery))   { m_sceneInfos.thickEvery   = ps.gridThickEvery;   pl::ProjectLoader::instance().markDirty(); }
            if (ImGui::InputFloat("Fade distance", &ps.gridFadeDistance)) { m_sceneInfos.fadeDistance = ps.gridFadeDistance; pl::ProjectLoader::instance().markDirty(); }
            if (ImGui::InputFloat("Line width",    &ps.gridLineWidth))    { m_sceneInfos.lineWidth    = ps.gridLineWidth;    pl::ProjectLoader::instance().markDirty(); }

            ImGui::SeparatorText("Temporal Anti-Aliasing");
            if (ImGui::InputFloat("Blend alpha", &ps.taaBlendAlpha, 0.01f, 0.10f))
            {
                m_pRHI->setTAABlendAlpha(ps.taaBlendAlpha);
                pl::ProjectLoader::instance().markDirty();
            }

            const auto& io = ImGui::GetIO();
            ImGui::SeparatorText("Application infos");
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                1000.0f / io.Framerate, io.Framerate);
            ImGui::Text("Viewport size: %.0f x %.0f", m_viewportSize.x, m_viewportSize.y);
        }
        ImGui::End();
    }

    void UiManager::drawSceneHierarchy()
    {
        ImGui::Begin("Scene Hierarchy");
        {
            ImGui::SeparatorText("Add objects");

            auto& scene      = SLS::SceneManager::instance().current();
            glm::vec3 frontOfCam = m_cameraRef->m_position + m_cameraRef->m_front * 4.0f;

            if (ImGui::BeginMenu("Add shape"))
            {
                if (ImGui::MenuItem("Cone"))      editor::spawnCone     (scene, m_pRHI, frontOfCam);
                if (ImGui::MenuItem("Cube"))      editor::spawnCube     (scene, m_pRHI, frontOfCam);
                if (ImGui::MenuItem("Cylinder"))  editor::spawnCylinder (scene, m_pRHI, frontOfCam);
                if (ImGui::MenuItem("Icosphere")) editor::spawnIcosphere(scene, m_pRHI, frontOfCam);
                if (ImGui::MenuItem("Sphere"))    editor::spawnSphere   (scene, m_pRHI, frontOfCam);
                if (ImGui::MenuItem("Suzanne"))   editor::spawnSuzanne  (scene, m_pRHI, frontOfCam);
                if (ImGui::MenuItem("Torus"))     editor::spawnTorus    (scene, m_pRHI, frontOfCam);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Add light"))
            {
                if (ImGui::MenuItem("Spot light"))  editor::spawnSpotLight (scene, m_pRHI, frontOfCam);
                if (ImGui::MenuItem("Point light")) editor::spawnPointLight(scene, m_pRHI, frontOfCam);
                ImGui::EndMenu();
            }
            if (ImGui::Button("Add asset"))
            {
                const char* filters[] = { "*.obj", "*.gltf" };
                if (auto path = tinyfd_openFileDialog("Choose an asset",
                    std::filesystem::current_path().string().c_str(), 2, filters, "3D Object files", 0);
                    path != nullptr)
                {
                    editor::spawnAsset(scene, m_pRHI, path, frontOfCam);
                }
            }

            ImGui::SeparatorText("Scene tree");
            if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (const auto& entity : *m_meshData)
                {
                    const bool selected = (m_selectedEntityID == entity.id);
                    std::string label   = std::string(ICON_FA_CUBE) + " " + entity.name;
                    if (ImGui::Selectable(label.c_str(), selected))
                        m_selectedEntityID = entity.id;
                }
                for (auto& point : *m_pointLights)
                {
                    const bool selected = (m_selectedEntityID == point.id);
                    std::string label   = std::string(ICON_FA_LIGHTBULB_O) + " " + point.name;
                    if (ImGui::Selectable(label.c_str(), selected))
                        m_selectedEntityID = point.id;
                }
                for (auto& spot : *m_spotLights)
                {
                    const bool selected = (m_selectedEntityID == spot.id);
                    std::string label   = std::string(ICON_FA_LIGHTBULB_O) + " " + spot.name;
                    if (ImGui::Selectable(label.c_str(), selected))
                        m_selectedEntityID = spot.id;
                }
                ImGui::TreePop();
            }

            if (m_selectedEntityID != ECS::INVALID_VALUE)
            {
                if (ImGui::Button("Remove selected") || ImGui::IsKeyPressed(ImGuiKey_Delete))
                {
                    bool isMesh  = std::ranges::find_if(*m_meshData,    [&](const rhi::vulkan::Mesh& m)       { return m.id == m_selectedEntityID; }) != m_meshData->end();
                    bool isSpot  = std::ranges::find_if(*m_spotLights,  [&](const rhi::vulkan::SpotLight& m)  { return m.id == m_selectedEntityID; }) != m_spotLights->end();
                    bool isPoint = std::ranges::find_if(*m_pointLights, [&](const rhi::vulkan::PointLight& m) { return m.id == m_selectedEntityID; }) != m_pointLights->end();
                    if (isMesh)
                        m_pRHI->removeMesh(m_selectedEntityID);
                    else if (isSpot)
                        m_pRHI->getLightSystem().removeSpotLight(m_selectedEntityID);
                    else if (isPoint)
                        m_pRHI->getLightSystem().removePointLight(m_selectedEntityID);
                    scene.destroyEntity(m_selectedEntityID);
                    m_selectedEntityID = ECS::INVALID_VALUE;
                }
            }

            if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
            {
                m_selectedEntityID = ECS::INVALID_VALUE;
            }
        }
        ImGui::End();
    }

    void UiManager::drawEntityProperties()
    {
        auto& scene    = SLS::SceneManager::instance().current();
        auto& registry = scene.getRegistry();

        for (auto& entity : *m_meshData)
        {
            entity.transform = registry.getComponent<ECS::Transform>(entity.id);
            entity.transform.rotation.Normalize();
            entity.physics   = registry.getComponent<ECS::PhysicsComponent>(entity.id);
        }

        ImGui::Begin("Entity properties");
        {
            if (m_selectedEntityID == ECS::INVALID_VALUE)
            {
                ImGui::TextDisabled("No entity selected");
                ImGui::End();
                return;
            }

            rhi::vulkan::Mesh*       mesh  = m_pRHI->findMesh(m_selectedEntityID);
            rhi::vulkan::SpotLight*  spot  = m_pRHI->getLightSystem().findSpotLight(m_selectedEntityID);
            rhi::vulkan::PointLight* point = m_pRHI->getLightSystem().findPointLight(m_selectedEntityID);

            if (!mesh && !spot && !point)
            {
                m_selectedEntityID = ECS::INVALID_VALUE;
                ImGui::End();
                return;
            }
            else if (mesh  != nullptr) { showMeshInfos(mesh);        }
            else if (spot  != nullptr) { showSpotLightInfos(spot);   }
            else if (point != nullptr) { showPointLightInfos(point); }

            if (m_simulationState != SimulationState::PLAYING)
            {
                ImGui::SeparatorText("Gizmo controls");
                drawGizmoControls();
            }
        }
        ImGui::End();
    }

    void UiManager::showMeshInfos(rhi::vulkan::Mesh* mesh)
    {
        auto& scene    = SLS::SceneManager::instance().current();
        auto& registry = scene.getRegistry();
        auto& tc       = registry.getComponent<ECS::Transform>(m_selectedEntityID);
        auto& physics  = registry.getComponent<ECS::PhysicsComponent>(m_selectedEntityID);

        char buffer[256];
        strncpy(buffer, mesh->name.c_str(), sizeof(buffer));
        if (ImGui::InputText("Name", buffer, sizeof(buffer)))
            mesh->name = std::string(buffer);

        ImGui::SeparatorText("Transform");
        drawVec3Control("Position", tc.position);
        if (drawVec3Control("Rotation", tc.eulerRadians)
            && m_simulationState != SimulationState::PLAYING)
        {
            glm::quat q = glm::quat(glm::vec3(tc.eulerRadians.x, tc.eulerRadians.y, tc.eulerRadians.z));
            tc.rotation = Quaternion(q.w, q.x, q.y, q.z);
        }
        drawVec3Control("Scale", tc.scale);

        ImGui::SeparatorText("Texture");
        if (ImGui::Button("Add texture"))
        {
            const char* filters[] = { "*.png", "*.jpg", "*.jpeg" };
            char* path = tinyfd_openFileDialog("Choose a texture",
                std::filesystem::current_path().string().c_str(), 3, filters, "Image files", 0);
            if (path)
                m_pRHI->uploadTexture(m_selectedEntityID, path, false);
        }
        if (mesh->hasTexture() && mesh->hasMipmaps())
        {
            static float lodLevel = 0.0f;
            if (ImGui::SliderFloat("LOD Bias", &lodLevel, 0.0f, mesh->texture()->getMipLevels()))
                mesh->texture()->setLodLevel(lodLevel);
        }

        ImGui::SeparatorText("Physics");
        const char* motionTypeLabels[] = { "Static", "Dynamic", "Kinematic" };
        const char* layerLabels[]      = { "Non moving", "Moving" };
        const char* bodyShapes[]       = { "Cube", "Sphere", "Cylinder", "Capsule" };

        int currentMotion = static_cast<int>(physics.motionType);
        int currentLayer  = static_cast<int>(physics.layers);
        int currentShape  = static_cast<int>(physics.shapeType);

        ImGuiIO& io = ImGui::GetIO();
        ImGui::PushFont(io.Fonts->Fonts[0]);
        if (ImGui::Combo("Motion Type", &currentMotion, motionTypeLabels, IM_ARRAYSIZE(motionTypeLabels)))
            physics.motionType = static_cast<ECS::EMotionType>(currentMotion);
        if (ImGui::Combo("Layer", &currentLayer, layerLabels, IM_ARRAYSIZE(layerLabels)))
            physics.layers = static_cast<ECS::ELayers>(currentLayer);
        if (ImGui::Combo("Shape type", &currentShape, bodyShapes, IM_ARRAYSIZE(bodyShapes)))
            physics.shapeType = static_cast<ECS::EShapeType>(currentShape);
        ImGui::Checkbox("Is Trigger", &physics.isTrigger);
        ImGui::PopFont();

        ImGui::SeparatorText("Scripting");
        ImGui::PushFont(io.Fonts->Fonts[0]);
        if (ImGui::Button((std::string(ICON_FA_CODE) + " Add script").c_str()))
        {
            std::string modelPath  = std::string(PROJECT_ROOT) + "/Engine/Core/Scripting/ScriptModel.cpp";
            std::string outputDir  = std::string(PROJECT_ROOT) + "/Scripts";
            std::string outputPath = outputDir + "/Script" + std::to_string(mesh->id) + ".cpp";

            std::filesystem::create_directories(outputDir);

            std::ifstream scriptFileModel(modelPath, std::ios::binary);
            if (!scriptFileModel.is_open())
            {
                std::cerr << "Failed to open script model: " << modelPath
                          << " (cwd: " << std::filesystem::current_path() << ")" << std::endl;
            }
            else
            {
                std::string modelContent((std::istreambuf_iterator<char>(scriptFileModel)),
                                          std::istreambuf_iterator<char>());

                const std::string scriptName  = "Script" + std::to_string(mesh->id);
                const std::string placeholder = "SCRIPT_NAME";
                auto pos = modelContent.find(placeholder);
                if (pos != std::string::npos)
                    modelContent.replace(pos, placeholder.size(), scriptName);

                std::ofstream scriptFile(outputPath, std::ios::binary);
                if (!scriptFile.is_open())
                    std::cerr << "Failed to create script file: " << outputPath << std::endl;
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
    }

    void UiManager::showSpotLightInfos(rhi::vulkan::SpotLight* spot)
    {
        char buffer[256];
        strncpy(buffer, spot->name.c_str(), sizeof(buffer));
        if (ImGui::InputText("Name", buffer, sizeof(buffer)))
            spot->name = std::string(buffer);

        ImGui::SeparatorText("Transform");
        if (drawVec3Control("Position",  spot->position))
            m_registry->getComponent<ECS::SpotLightComponent>(spot->id).position  = spot->position;
        if (drawVec3Control("Direction", spot->direction))
            m_registry->getComponent<ECS::SpotLightComponent>(spot->id).direction = spot->direction;
        if (drawVec3Control("Color",     spot->color))
            m_registry->getComponent<ECS::SpotLightComponent>(spot->id).color     = spot->color;
        if (ImGui::SliderFloat("Intensity",            &spot->intensity,     0.0f,  5.0f))
            m_registry->getComponent<ECS::SpotLightComponent>(spot->id).intensity     = spot->intensity;
        if (ImGui::SliderFloat("Radius",               &spot->radius,        0.0f, 100.0f))
            m_registry->getComponent<ECS::SpotLightComponent>(spot->id).radius        = spot->radius;
        if (ImGui::SliderFloat("Inner cut-off degrees",&spot->innerCutoffDeg, 0.0f, 89.0f))
            m_registry->getComponent<ECS::SpotLightComponent>(spot->id).innerCutoffDeg = spot->innerCutoffDeg;
        if (ImGui::SliderFloat("Outer cut-off degrees",&spot->outerCutoffDeg, 0.0f, 90.0f))
            m_registry->getComponent<ECS::SpotLightComponent>(spot->id).outerCutoffDeg = spot->outerCutoffDeg;
        if (ImGui::Checkbox("Show gizmo", &spot->showGizmo))
            m_registry->getComponent<ECS::SpotLightComponent>(spot->id).showGizmo = spot->showGizmo;
    }

    void UiManager::showPointLightInfos(rhi::vulkan::PointLight* point)
    {
        char buffer[256];
        strncpy(buffer, point->name.c_str(), sizeof(buffer));
        if (ImGui::InputText("Name", buffer, sizeof(buffer)))
            point->name = std::string(buffer);

        ImGui::SeparatorText("Transform");
        if (drawVec3Control("Position", point->position))
            m_registry->getComponent<ECS::PointLightComponent>(point->id).position = point->position;
        if (drawVec3Control("Color",    point->color))
            m_registry->getComponent<ECS::PointLightComponent>(point->id).color    = point->color;
        if (ImGui::SliderFloat("Intensity", &point->intensity, 0.0f, 5.0f))
            m_registry->getComponent<ECS::PointLightComponent>(point->id).intensity = point->intensity;
        if (ImGui::SliderFloat("Radius",    &point->radius,    0.0f, 100.0f))
            m_registry->getComponent<ECS::PointLightComponent>(point->id).radius    = point->radius;
    }

    void UiManager::drawAudioControl()
    {
        ImGui::Begin("Audio control");
        {
            m_audioSystem->update();
            int i = 0;
            for (auto& source : m_audioSources)
            {
                bool isPlaying     = source->isPlaying();
                bool isLooping     = source->isLooping();
                bool isSpatialized = source->isSpatialized();
                drawVec3Control(std::string("Audio source " + std::to_string(i)).c_str(), source->getPosition());
                if (ImGui::Checkbox("Play", &isPlaying))
                {
                    if (source->isPlaying()) source->pause();
                    else                     source->play();
                }
                if (ImGui::Checkbox("Loop",       &isLooping))     source->setLooping(!source->isLooping());
                if (ImGui::Checkbox("Spatialize", &isSpatialized)) source->setSpatialized(!source->isSpatialized());
                i++;
            }
            ImGui::SeparatorText("Options");
            if (ImGui::Button("Add audio source"))
            {
                const char* filters[] = { "*.mp3", "*.wav", "*.flac" };
                auto path = tinyfd_openFileDialog("Add an audio source",
                    std::filesystem::current_path().string().c_str(), 3, filters, "Audio files", 0);
                if (path != nullptr)
                {
                    m_audioSources.push_back(m_audioSystem->loadAudio(path));
                    m_audioSources.back()->setVolume(1.f);
                    m_audioSources.back()->setPitch(1.f);
                }
            }
        }
        ImGui::End();
    }

    void UiManager::drawConsole()
    {
        static char inputBuffer[256];
        static bool autoScroll     = true;
        static bool scrollToBottom = false;

        ImGui::Begin("Console");
        {
            if (ImGui::SmallButton("Clear"))  m_consoleItems.clear();
            ImGui::SameLine();
            bool copyToClipboard = ImGui::SmallButton("Copy");

            const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
            if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeight),
                ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_HorizontalScrollbar))
            {
                if (ImGui::BeginPopupContextWindow())
                {
                    if (ImGui::Selectable("Clear")) m_consoleItems.clear();
                    ImGui::EndPopup();
                }

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
                if (copyToClipboard) ImGui::LogToClipboard();

                for (const auto& item : m_consoleItems)
                    ImGui::TextUnformatted(item.c_str());

                if (copyToClipboard)    ImGui::LogFinish();
                if (scrollToBottom || (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
                    ImGui::SetScrollHereY(1.0f);
                scrollToBottom = false;
                ImGui::PopStyleVar();
            }
            ImGui::EndChild();
            ImGui::Separator();

            bool reclaimFocus = false;
            ImGuiInputTextFlags inputTextFlags =
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll;
            if (ImGui::InputText("Input", inputBuffer, IM_COUNTOF(inputBuffer), inputTextFlags))
            {
                m_consoleItems.emplace_back(inputBuffer);
                inputBuffer[0] = '\0';
                reclaimFocus   = true;
            }
            ImGui::SetItemDefaultFocus();
            if (reclaimFocus) ImGui::SetKeyboardFocusHere(-1);
        }
        ImGui::End();
    }

    void UiManager::drawBottomBar()
    {
        constexpr ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_MenuBar;

        const float height = ImGui::GetFrameHeight();
        if (ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(),
            ImGuiDir_Down, height, windowFlags))
        {
            if (ImGui::BeginMenuBar())
            {
                ImGui::Text(config::engineName.data());
                ImGui::SameLine();
                ImGui::Text(config::engineVersion.data());
                ImGui::SameLine();

                constexpr float internPadding = 8.0f;
                std::string rightText = std::string(config::buildType) +
                                        " [" + config::architecture.data() + "] | " +
                                        config::platform.data();

                float rightTextWidth = ImGui::CalcTextSize(rightText.c_str()).x;
                float avail          = ImGui::GetContentRegionAvail().x;
                float alignPos       = ImGui::GetCursorPosX() + avail - rightTextWidth - internPadding;

                ImGui::SameLine(alignPos);
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

        const ImGuiViewport* viewport   = setupViewport();
        const ImGuiID        dockspaceId = getDockspaceID();
        static bool dockspaceInitialized = false;
        if (!dockspaceInitialized)
        {
            initDockspace(dockspaceId, viewport);
            dockspaceInitialized = true;
        }

        drawViewport();
        if (m_showSettings) drawSettings();
        drawSceneHierarchy();
        drawEntityProperties();
        drawAudioControl();
        drawConsole();
        drawBottomBar();
        m_contentBrowser.render();

        if (m_showDemoWindow)
            ImGui::ShowDemoWindow(&m_showDemoWindow);

        auto& settings            = SLS::SceneManager::instance().current().getSceneSettings();
        m_sceneInfos.clearColor     = { settings.clearColor.x,     settings.clearColor.y,     settings.clearColor.z,     settings.clearColor.w };
        m_sceneInfos.ambientColor   = { settings.ambientColor.x,   settings.ambientColor.y,   settings.ambientColor.z   };
        m_sceneInfos.lightColor     = { settings.lightColor.x,     settings.lightColor.y,     settings.lightColor.z     };
        m_sceneInfos.lightDirection = { settings.lightDirection.x, settings.lightDirection.y, settings.lightDirection.z };

        m_pRHI->updateSceneUBO(&m_sceneInfos, m_cameraRef->m_position);
        if (m_viewportSize.x != 0 && m_viewportSize.y != 0)
        {
            m_pRHI->updateCameraUBO(m_cameraRef->update(m_viewportSize.x / m_viewportSize.y, m_camSpeed));
        }
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_S))
        {
            if (m_sceneManager && !m_currentScenePath.empty())
            {
                pl::ProjectLoader::instance().saveProject();
                m_sceneManager->current().save(m_currentScenePath);
            }
        }
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_Q))
        {
            m_isRunning = false;
            m_reloadApp = true;
        }
    }

    void UiManager::applyLoadedSettings()
    {
        auto& ps       = pl::ProjectLoader::instance().getSettings();
        auto& settings = SLS::SceneManager::instance().current().getSceneSettings();

        m_pRHI->setVSync(ps.vsync);
        m_pRHI->setTAABlendAlpha(ps.taaBlendAlpha);

        settings.clearColor     = { ps.clearColor[0],     ps.clearColor[1],     ps.clearColor[2],     ps.clearColor[3] };
        settings.ambientColor   = { ps.ambientColor[0],   ps.ambientColor[1],   ps.ambientColor[2]   };
        settings.lightColor     = { ps.lightColor[0],     ps.lightColor[1],     ps.lightColor[2]     };
        settings.lightDirection = { ps.lightDirection[0], ps.lightDirection[1], ps.lightDirection[2] };

        m_camSpeed              = ps.cameraSpeed;
        m_sceneInfos.cellSize     = ps.gridCellSize;
        m_sceneInfos.thickEvery   = ps.gridThickEvery;
        m_sceneInfos.fadeDistance = ps.gridFadeDistance;
        m_sceneInfos.lineWidth    = ps.gridLineWidth;
    }

    void UiManager::drawGizmoControls()
    {
        ImGui::Text("Gizmo");

        const char* gizmoOpLabels[]   = { "Translate (W)", "Rotate (E)", "Scale (R)" };
        const char* gizmoModeLabels[] = { "Local", "World" };

        int currentMode = 0;
        switch (m_currentGizmoMode)
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

        if (m_currentGizmoOperation != ImGuizmo::SCALE)
        {
            if (ImGui::Combo("Mode", &currentMode, gizmoModeLabels, IM_ARRAYSIZE(gizmoModeLabels)))
            {
                switch (currentMode)
                {
                    case 0: m_currentGizmoMode = ImGuizmo::LOCAL; break;
                    case 1: m_currentGizmoMode = ImGuizmo::WORLD; break;
                    default:                                      break;
                }
            }
        }

        ImGui::Checkbox("Snap", &m_useSnap);
        if (m_useSnap)
        {
            switch (m_currentGizmoOperation)
            {
                case ImGuizmo::TRANSLATE: ImGui::DragFloat3("Snapping",    m_snapTranslation);                       break;
                case ImGuizmo::ROTATE:    ImGui::DragFloat("Snap Rotation",&m_snapRotation, 1.0f, 0.0f, 360.0f);    break;
                case ImGuizmo::SCALE:     ImGui::DragFloat("Snap Scale",   &m_snapScale,    0.01f, 0.0f, 1.0f);     break;
                default:                                                                                              break;
            }
        }
    }

    void UiManager::handleGizmoInput()
    {
        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) || ImGuizmo::IsUsing())
            return;
        if (m_simulationState == SimulationState::PLAYING)
            return;

        if (ImGui::IsKeyPressed(ImGuiKey_W)) m_currentGizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) m_currentGizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_currentGizmoOperation = ImGuizmo::SCALE;
    }

    void UiManager::drawGizmo()
    {
        if (m_selectedEntityID == ECS::INVALID_VALUE)
            return;

        auto it       = std::ranges::find_if(*m_meshData,    [&](const rhi::vulkan::Mesh& m)       { return m.id == m_selectedEntityID; });
        auto spotIt   = std::ranges::find_if(*m_spotLights,  [&](const rhi::vulkan::SpotLight& m)  { return m.id == m_selectedEntityID; });
        auto pointIt  = std::ranges::find_if(*m_pointLights, [&](const rhi::vulkan::PointLight& m) { return m.id == m_selectedEntityID; });

        if (it == m_meshData->end() && spotIt == m_spotLights->end() && pointIt == m_pointLights->end())
        {
            m_selectedEntityID = ECS::INVALID_VALUE;
            return;
        }

        if (spotIt != m_spotLights->end())   { drawGizmoSpotLight(*spotIt);   return; }
        if (pointIt != m_pointLights->end()) { drawGizmoPointLight(*pointIt); return; }
        drawGizmoMesh(*it);
    }

    void UiManager::drawGizmoMesh(rhi::vulkan::Mesh& selectedMesh)
    {
        glm::mat4 view       = m_cameraRef->getViewMatrix();
        glm::mat4 projection = m_cameraRef->getProjectionMatrix();
        projection[1][1] *= -1.0f;

        glm::mat4 modelMatrix = selectedMesh.getTransform();
        float modelData[16];
        memcpy(modelData, glm::value_ptr(modelMatrix), sizeof(float) * 16);

        float* snapPtr      = nullptr;
        float  snapValues[3] = {};
        if (m_useSnap)
        {
            switch (m_currentGizmoOperation)
            {
                case ImGuizmo::TRANSLATE: memcpy(snapValues, m_snapTranslation, sizeof(snapValues)); break;
                case ImGuizmo::ROTATE:    snapValues[0] = snapValues[1] = snapValues[2] = m_snapRotation; break;
                case ImGuizmo::SCALE:     snapValues[0] = snapValues[1] = snapValues[2] = m_snapScale;    break;
                default: break;
            }
            snapPtr = snapValues;
        }

        auto& tc = SLS::SceneManager::instance().current().getRegistry().getComponent<ECS::Transform>(m_selectedEntityID);
        glm::vec3 oldRotation = { tc.eulerRadians.x, tc.eulerRadians.y, tc.eulerRadians.z };
        glm::vec3 oldScale    = { tc.scale.x,        tc.scale.y,        tc.scale.z        };

        float     deltaData[16];
        glm::mat4 identityDelta = glm::mat4(1.0f);
        memcpy(deltaData, glm::value_ptr(identityDelta), sizeof(float) * 16);

        ImGuizmo::MODE activeMode = (m_currentGizmoOperation == ImGuizmo::SCALE)
            ? ImGuizmo::LOCAL
            : m_currentGizmoMode;

        ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(projection),
            m_currentGizmoOperation,
            activeMode,
            modelData, deltaData, snapPtr
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
                tc.position     = { position.x, position.y, position.z };
                tc.rotation     = Quaternion(rotation.w, rotation.x, rotation.y, rotation.z);
                tc.rotation.Normalize();
                tc.eulerRadians = {
                    glm::eulerAngles(rotation).x,
                    glm::eulerAngles(rotation).y,
                    glm::eulerAngles(rotation).z
                };
                break;
            }
            case ImGuizmo::SCALE:
            {
                glm::vec3 deltaScale = {
                    glm::length(glm::vec3(delta[0])),
                    glm::length(glm::vec3(delta[1])),
                    glm::length(glm::vec3(delta[2]))
                };
                glm::vec3 newScale  = oldScale * deltaScale;
                tc.scale        = { newScale.x,    newScale.y,    newScale.z    };
                tc.eulerRadians = { oldRotation.x, oldRotation.y, oldRotation.z };
                break;
            }
            default: break;
        }
    }

    void UiManager::drawGizmoPointLight(rhi::vulkan::PointLight& selectedLight)
    {
        glm::mat4 view       = m_cameraRef->getViewMatrix();
        glm::mat4 projection = m_cameraRef->getProjectionMatrix();
        projection[1][1] *= -1.0f;

        auto& pos = m_registry->getComponent<ECS::PointLightComponent>(selectedLight.id).position;
        glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, pos.z));

        float modelData[16];
        memcpy(modelData, glm::value_ptr(modelMatrix), sizeof(float) * 16);
        float     deltaData[16];
        glm::mat4 identityDelta = glm::mat4(1.0f);
        memcpy(deltaData, glm::value_ptr(identityDelta), sizeof(float) * 16);

        ImGuizmo::Manipulate(
            glm::value_ptr(view), glm::value_ptr(projection),
            ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
            modelData, deltaData, nullptr
        );

        if (!ImGuizmo::IsUsing()) return;

        glm::mat4 newModel;
        memcpy(glm::value_ptr(newModel), modelData, sizeof(float) * 16);
        pos                    = { newModel[3].x, newModel[3].y, newModel[3].z };
        selectedLight.position = { newModel[3].x, newModel[3].y, newModel[3].z };
    }

    void UiManager::drawGizmoSpotLight(rhi::vulkan::SpotLight& selectedLight)
    {
        glm::mat4 view       = m_cameraRef->getViewMatrix();
        glm::mat4 projection = m_cameraRef->getProjectionMatrix();
        projection[1][1] *= -1.0f;

        auto& oldDir = m_registry->getComponent<ECS::SpotLightComponent>(selectedLight.id).direction;
        auto& pos    = m_registry->getComponent<ECS::SpotLightComponent>(selectedLight.id).position;
        glm::vec3 dir = glm::normalize(glm::vec3(oldDir.x, oldDir.y, oldDir.z));

        glm::quat orientQuat;
        if (glm::abs(glm::dot(dir, glm::vec3(0.0f, 1.0f, 0.0f))) < 0.999f)
            orientQuat = glm::quatLookAt(dir, glm::vec3(0.0f, 1.0f, 0.0f));
        else
            orientQuat = glm::quatLookAt(dir, glm::vec3(0.0f, 0.0f, 1.0f));

        glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, pos.z))
                              * glm::mat4_cast(orientQuat);

        float modelData[16];
        memcpy(modelData, glm::value_ptr(modelMatrix), sizeof(float) * 16);
        float     deltaData[16];
        glm::mat4 identityDelta = glm::mat4(1.0f);
        memcpy(deltaData, glm::value_ptr(identityDelta), sizeof(float) * 16);

        ImGuizmo::MODE activeMode = (m_currentGizmoOperation == ImGuizmo::SCALE)
            ? ImGuizmo::LOCAL
            : m_currentGizmoMode;

        float* snapPtr      = nullptr;
        float  snapValues[3] = {};
        if (m_useSnap)
        {
            switch (m_currentGizmoOperation)
            {
                case ImGuizmo::TRANSLATE: memcpy(snapValues, m_snapTranslation, sizeof(snapValues)); break;
                case ImGuizmo::ROTATE:    snapValues[0] = snapValues[1] = snapValues[2] = m_snapRotation; break;
                default: break;
            }
            snapPtr = snapValues;
        }

        ImGuizmo::Manipulate(
            glm::value_ptr(view), glm::value_ptr(projection),
            m_currentGizmoOperation == ImGuizmo::SCALE ? ImGuizmo::TRANSLATE : m_currentGizmoOperation,
            activeMode, modelData, deltaData, snapPtr
        );

        if (!ImGuizmo::IsUsing()) return;

        glm::mat4 newModel;
        memcpy(glm::value_ptr(newModel), modelData, sizeof(float) * 16);

        switch (m_currentGizmoOperation)
        {
            case ImGuizmo::TRANSLATE:
            {
                pos                    = { newModel[3].x, newModel[3].y, newModel[3].z };
                selectedLight.position = { newModel[3].x, newModel[3].y, newModel[3].z };
                break;
            }
            case ImGuizmo::ROTATE:
            {
                glm::vec3 position, scale, skew;
                glm::vec4 perspective;
                glm::quat rotation;
                glm::decompose(newModel, scale, rotation, position, skew, perspective);
                pos                    = { position.x, position.y, position.z };
                selectedLight.position = { position.x, position.y, position.z };
                glm::vec3 newDir       = glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f));
                oldDir                 = { newDir.x, newDir.y, newDir.z };
                selectedLight.direction = { newDir.x, newDir.y, newDir.z };
                break;
            }
            default: break;
        }
    }
} // namespace gcep
