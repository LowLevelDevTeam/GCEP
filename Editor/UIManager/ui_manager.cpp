#include "ui_manager.hpp"

// Internals
#include <config.hpp>
#include <Editor/Camera/camera.hpp>
#include <Editor/UI_Panels/Panels/Inspector/inspector_registry.hpp>
#include <ECS/Components/components.hpp>
#include <ECS/Components/hierarchy_component.hpp>
#include <ECS/Components/world_transform.hpp>
#include <Log/log.hpp>

// Externals
#include <font_awesome.hpp>
#include <imgui_impl_glfw.h>
#include <ImGuizmo.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

// STL
#include <cmath>
#include <filesystem>
#include <functional>
#include <string>

namespace gcep::editor { void registerEngineDrawers(); }

namespace gcep
{
    UiManager::UiManager(GLFWwindow* window, SLS::SceneManager* manager, bool& reload, bool& close)
        : m_reloadApp(reload),
          m_isRunning(close),
          m_contentBrowser(pl::ProjectLoader::instance().getProjectInfo().contentPath),
          m_scriptState{},
          m_scriptManagerPanel(m_scriptState),
          m_inspector(m_scriptState)
    {
        m_window       = window;
        m_sceneManager = manager;

        editor::registerEngineDrawers();

        const auto& projectInfo = pl::ProjectLoader::instance().getProjectInfo();
        const std::string scriptsDir  = projectInfo.contentPath.generic_string() + "/Scripts";
        const std::string buildDir    = projectInfo.contentPath.generic_string() + "/Scripts/bin";
        const std::string includeDir  = std::string(PROJECT_ROOT) + "/Engine/Core/Scripting";

        #ifdef GCE_BUILD_DIR
            const std::string cmakeBuildDir = GCE_BUILD_DIR;
        #else
            const std::string cmakeBuildDir = std::string(PROJECT_ROOT) + "/build";
        #endif

        auto& ctx      = editor::EditorContext::get();
        ctx.registry   = &m_sceneManager->current().getRegistry();
        editor::registerEngineDrawers();
        ctx.m_scriptManager.init(scriptsDir, buildDir, includeDir);

        m_scriptState.manager       = &ctx.m_scriptManager;
        m_scriptState.registry      = ctx.registry;
        m_scriptState.scriptsDir    = scriptsDir;
        m_scriptState.cmakeBuildDir = cmakeBuildDir;
        ctx.scriptManagerPanel      = &m_scriptManagerPanel;

        // OS drag & drop — copy dropped files into the content browser's current folder
        glfwSetDropCallback(m_window, [](GLFWwindow*, int count, const char** paths)
        {
            auto& ctx = editor::EditorContext::get();
            ctx.droppedPaths.clear();
            for (int i = 0; i < count; ++i)
                ctx.droppedPaths.emplace_back(paths[i]);
        });

        Log::info("UiManager initialized successfully");
    }

    void UiManager::setInfos(rhi::vulkan::InitInfos* infos)
    {
        auto& ctx = editor::EditorContext::get();
        ctx.pRHI  = infos->instance;
        m_viewport.setViewportTexture(infos->ds);
    }

    void UiManager::setCamera(Camera* pCamera)
    {
        editor::EditorContext::get().camera = pCamera;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Init / dockspace
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

        ImGui::DockBuilderDockWindow("Scene Hierarchy",   dockIdHierarchy);
        ImGui::DockBuilderDockWindow("Entity properties", dockIdProperties);
        ImGui::DockBuilderDockWindow("Audio control",     dockIdProperties);
        ImGui::DockBuilderDockWindow("Script Manager",    dockIdProperties);
        ImGui::DockBuilderDockWindow("Viewport",          dockIdViewport);
        ImGui::DockBuilderDockWindow("Console",           dockIdConsole);
        ImGui::DockBuilderDockWindow("ContentDrawer",     dockIdContent);
        ImGui::DockBuilderDockWindow("Settings",          dockIdRight);
        ImGui::DockBuilderDockWindow("Performance",       dockIdRight);

        ImGui::DockBuilderGetNode(dockIdViewport)->LocalFlags |= ImGuiDockNodeFlags_AutoHideTabBar;
        ImGui::DockBuilderFinish(dockspaceId);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Per-frame
    // ─────────────────────────────────────────────────────────────────────────────

    void UiManager::drawMainMenuBar()
    {
        const ImGuiIO& io = ImGui::GetIO();
        ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 5.f);
        ImGui::PushFont(io.Fonts->Fonts[0]);
        if (ImGui::BeginMainMenuBar())
        {
            // ── File ──────────────────────────────────────────────────────────────
            if (ImGui::BeginMenu((std::string(ICON_FA_FILE) + " File").c_str()))
            {
                if (ImGui::MenuItem((std::string(ICON_FA_FLOPPY_O) + " Save Scene").c_str(), "Ctrl+S"))
                {
                    if (m_sceneManager && !m_sceneManager->current().getPath().empty())
                    {
                        pl::ProjectLoader::instance().saveProject();
                        m_sceneManager->current().save();
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem((std::string(ICON_FA_FOLDER_OPEN) + " Open Project").c_str()))
                    m_reloadApp = true;
                ImGui::Separator();
                if (ImGui::MenuItem((std::string(ICON_FA_WINDOW_CLOSE) + " Exit").c_str(), "Ctrl+Q"))
                {
                    m_isRunning = false;
                    m_reloadApp = true;
                }
                ImGui::EndMenu();
            }

            // ── View ─────────────────────────────────────────────────────────────
            if (ImGui::BeginMenu((std::string(ICON_FA_EYE) + " View").c_str()))
            {
                ImGui::MenuItem("Viewport",        nullptr, &m_viewport.isVisible);
                ImGui::MenuItem("Hierarchy",       nullptr, &m_hierarchy.isVisible);
                ImGui::MenuItem("Inspector",       nullptr, &m_inspector.isVisible);
                ImGui::MenuItem("Console",         nullptr, &m_console.isVisible);
                ImGui::MenuItem("Content Browser", nullptr, &m_contentBrowser.isVisible);
                ImGui::MenuItem("Script Manager",  nullptr, &m_scriptManagerPanel.isVisible);
                ImGui::MenuItem("Audio",           nullptr, &m_audio.isVisible);
                ImGui::MenuItem("Performance",     nullptr, &m_performance.isVisible);
                ImGui::MenuItem("Settings",        nullptr, &m_settings.isVisible);
                ImGui::Separator();
                if (ImGui::MenuItem((std::string(ICON_FA_REFRESH) + " Reset Layout").c_str()))
                    m_dockspaceInitialized = false;
                ImGui::EndMenu();
            }

            // ── World ────────────────────────────────────────────────────────────
            if (ImGui::BeginMenu((std::string(ICON_FA_GLOBE) + " World").c_str()))
            {
                if (ImGui::BeginMenu((std::string(ICON_FA_FOLDER_OPEN) + " Open Scene").c_str()))
                {
                    const auto& scenes = m_sceneManager->getSceneList();
                    if (scenes.empty())
                        ImGui::TextDisabled("No scenes registered");
                    for (const auto& scenePath : scenes)
                    {
                        const std::string label = std::filesystem::path(scenePath).filename().string();
                        if (ImGui::MenuItem(label.c_str()))
                            m_pendingScenePath = scenePath;
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem((std::string(ICON_FA_FLOPPY_O) + " Save Scene").c_str(), "Ctrl+S"))
                {
                    if (m_sceneManager && !m_sceneManager->current().getPath().empty())
                    {
                        pl::ProjectLoader::instance().saveProject();
                        m_sceneManager->current().save();
                    }
                }
                ImGui::EndMenu();
            }

            // ── Debug ────────────────────────────────────────────────────────────
            if (ImGui::BeginMenu((std::string(ICON_FA_BUG) + " Debug").c_str()))
            {
                ImGui::MenuItem("ImGui Demo", nullptr, &m_showDemoWindow);
                ImGui::Separator();
                if (ImGui::MenuItem((std::string(ICON_FA_REFRESH) + " Reload Engine").c_str()))
                    m_reloadApp = true;
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
        ImGui::PopFont();
        ImGui::PopStyleVar();
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

                const std::string rightText = std::string(config::buildType) +
                                              " [" + config::architecture.data() + "] | " +
                                              config::platform.data();

                constexpr float internPadding = 8.0f;
                const float rightTextWidth = ImGui::CalcTextSize(rightText.c_str()).x;
                const float alignPos       = ImGui::GetCursorPosX()
                                           + ImGui::GetContentRegionAvail().x
                                           - rightTextWidth - internPadding;
                ImGui::SameLine(alignPos);
                ImGui::Text("%s", rightText.c_str());
                ImGui::EndMenuBar();
            }
            ImGui::End();
        }
    }

    void UiManager::beginFrame()
    {
        ImGuizmo::BeginFrame();
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Sync helper
    // ─────────────────────────────────────────────────────────────────────────────

    // ─────────────────────────────────────────────────────────────────────────────
    // World transform propagation
    // ─────────────────────────────────────────────────────────────────────────────

    static glm::mat4 toMatrix(const ECS::Transform& t)
    {
        const glm::quat q(t.rotation.w, t.rotation.x, t.rotation.y, t.rotation.z);
        return glm::translate(glm::mat4(1.f), { t.position.x, t.position.y, t.position.z })
             * glm::mat4_cast(q)
             * glm::scale(glm::mat4(1.f), { t.scale.x, t.scale.y, t.scale.z });
    }

    static void propagateWorldTransform(ECS::Registry& registry,
                                        ECS::EntityID  id,
                                        const glm::mat4& parentWorld)
    {
        if (!registry.hasComponent<ECS::Transform>(id))      return;
        if (!registry.hasComponent<ECS::WorldTransform>(id)) return;

        const glm::mat4 worldMatrix = parentWorld * toMatrix(registry.getComponent<ECS::Transform>(id));

        auto& wt = registry.getComponent<ECS::WorldTransform>(id);

        glm::vec3 pos, scale, skew;
        glm::vec4 perspective;
        glm::quat rot;
        glm::decompose(worldMatrix, scale, rot, pos, skew, perspective);

        wt.position     = { pos.x,   pos.y,   pos.z   };
        wt.scale        = { scale.x, scale.y, scale.z };
        wt.rotation     = Quaternion(rot.w, rot.x, rot.y, rot.z);
        wt.rotation.Normalize();
        wt.eulerRadians = { glm::eulerAngles(rot).x,
                            glm::eulerAngles(rot).y,
                            glm::eulerAngles(rot).z };

        if (!registry.hasComponent<ECS::HierarchyComponent>(id)) return;
        ECS::EntityID child = registry.getComponent<ECS::HierarchyComponent>(id).firstChild;
        while (child != ECS::INVALID_VALUE)
        {
            propagateWorldTransform(registry, child, worldMatrix);
            child = registry.getComponent<ECS::HierarchyComponent>(child).nextSibling;
        }
    }

    static void updateWorldTransforms(ECS::Registry& registry)
    {
        for (const ECS::EntityID id : registry.view<ECS::Tag>())
        {
            const bool isRoot = !registry.hasComponent<ECS::HierarchyComponent>(id) ||
                                registry.getComponent<ECS::HierarchyComponent>(id).parent == ECS::INVALID_VALUE;
            if (isRoot)
                propagateWorldTransform(registry, id, glm::mat4(1.f));
        }
    }

    void UiManager::syncECSToRHI()
    {
        auto& ctx      = editor::EditorContext::get();
        auto& registry = *ctx.registry;

        updateWorldTransforms(registry);

        for (auto& mesh : *ctx.pRHI->getInitInfos()->meshData)
        {
            if (registry.hasComponent<ECS::WorldTransform>(mesh.id))
            {
                const auto& wt    = registry.getComponent<ECS::WorldTransform>(mesh.id);
                mesh.transform.position     = wt.position;
                mesh.transform.scale        = wt.scale;
                mesh.transform.rotation     = wt.rotation;
                mesh.transform.eulerRadians = wt.eulerRadians;
            }
            else
            {
                mesh.transform = registry.getComponent<ECS::Transform>(mesh.id);
            }
            mesh.transform.rotation.Normalize();
            mesh.physics = registry.getComponent<ECS::PhysicsComponent>(mesh.id);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Per-frame
    // ─────────────────────────────────────────────────────────────────────────────

    void UiManager::uiUpdate()
    {
        if (glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            return;
        }
        auto& ctx = editor::EditorContext::get();

        beginFrame();
        syncECSToRHI();
        ctx.m_scriptManager.pollForChanges();

        const ImGuiViewport* viewport    = setupViewport();
        const ImGuiID        dockspaceId = getDockspaceID();
        if (!m_dockspaceInitialized)
        {
            initDockspace(dockspaceId, viewport);
            m_dockspaceInitialized = true;
        }

        // ── Scene switch requested from menu ──────────────────────────────────────
        if (!m_pendingScenePath.empty())
        {
            m_sceneManager->loadScene(m_pendingScenePath, ctx.pRHI);
            ctx.registry          = &m_sceneManager->current().getRegistry();
            m_scriptState.registry = ctx.registry;
            ctx.pRHI->setRegistry(ctx.registry);
            m_pendingScenePath.clear();
        }

        // ── Panels ────────────────────────────────────────────────────────────────
        if (m_viewport.isVisible)        m_viewport.draw();
        if (m_settings.isVisible)        m_settings.draw();
        if (m_hierarchy.isVisible)       m_hierarchy.draw();
        if (m_inspector.isVisible)       m_inspector.draw();
        if (m_audio.isVisible)           m_audio.draw();
        if (m_console.isVisible)         m_console.draw();
        if (m_contentBrowser.isVisible)     m_contentBrowser.draw();
        if (m_performance.isVisible)        m_performance.draw();
        if (m_scriptManagerPanel.isVisible) m_scriptManagerPanel.draw();
        drawBottomBar();

        if (m_showDemoWindow)
            ImGui::ShowDemoWindow(&m_showDemoWindow);

        // ── Push scene / camera UBOs ──────────────────────────────────────────────
        auto& settings = SLS::SceneManager::instance().current().getSceneSettings();

        ctx.sceneInfos.clearColor     = { settings.clearColor.x,     settings.clearColor.y,
                                          settings.clearColor.z,     settings.clearColor.w };
        ctx.sceneInfos.ambientColor   = { settings.ambientColor.x,   settings.ambientColor.y,   settings.ambientColor.z   };
        ctx.sceneInfos.lightColor     = { settings.lightColor.x,     settings.lightColor.y,     settings.lightColor.z     };
        ctx.sceneInfos.lightDirection = { settings.lightDirection.x, settings.lightDirection.y, settings.lightDirection.z };

        ctx.pRHI->updateSceneUBO(&ctx.sceneInfos, ctx.camera->m_position);
        if (ctx.viewportSize.x != 0 && ctx.viewportSize.y != 0)
            ctx.pRHI->updateCameraUBO(ctx.camera->update(ctx.viewportSize.x / ctx.viewportSize.y, ctx.camSpeed));

        // ── Global keyboard shortcuts ─────────────────────────────────────────────
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_S))
        {
            if (m_sceneManager && !m_sceneManager->current().getPath().empty())
            {
                pl::ProjectLoader::instance().saveProject();
                m_sceneManager->current().save();
            }
        }
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_Q))
        {
            m_isRunning = false;
            m_reloadApp = true;
        }

        // Flush deferred ECS deletions (destroyEntity is deferred until update())
        ctx.registry->update();

        // Project reload requested from a panel (e.g. ProjectBrowserPanel)
        if (ctx.reloadRequested)
        {
            ctx.reloadRequested = false;
            m_reloadApp         = true;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Settings propagation
    // ─────────────────────────────────────────────────────────────────────────────

    void UiManager::applyLoadedSettings()
    {
        auto& ps       = pl::ProjectLoader::instance().getSettings();
        auto& settings = SLS::SceneManager::instance().current().getSceneSettings();
        auto& ctx      = editor::EditorContext::get();

        ctx.pRHI->setVSync(ps.vsync);
        ctx.pRHI->setTAABlendAlpha(ps.taaBlendAlpha);

        settings.clearColor     = { ps.clearColor[0],     ps.clearColor[1],     ps.clearColor[2],     ps.clearColor[3] };
        settings.ambientColor   = { ps.ambientColor[0],   ps.ambientColor[1],   ps.ambientColor[2]   };
        settings.lightColor     = { ps.lightColor[0],     ps.lightColor[1],     ps.lightColor[2]     };
        settings.lightDirection = { ps.lightDirection[0], ps.lightDirection[1], ps.lightDirection[2] };

        ctx.camSpeed                = ps.cameraSpeed;
        ctx.sceneInfos.cellSize     = ps.gridCellSize;
        ctx.sceneInfos.thickEvery   = ps.gridThickEvery;
        ctx.sceneInfos.fadeDistance = ps.gridFadeDistance;
        ctx.sceneInfos.lineWidth    = ps.gridLineWidth;
    }

} // namespace gcep
