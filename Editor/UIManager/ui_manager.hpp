#pragma once

/// @file ui_manager.hpp
/// @brief Top-level editor UI manager: dockspace, viewport, panels, and console.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.4
/// @date 2026-02-17

// Internals
#include <ECS/Components/components.hpp>
#include <Editor/Helpers.hpp>
#include <Editor/ProjectLoader/project_loader.hpp>
#include <Editor/SceneUtils/scene_util.hpp>
#include <Editor/UIManager/script_panel.hpp>
#include <Engine/Core/Scripting/script_hot_reload.hpp>
#include <Engine/Audio/audio_system.hpp>
#include <Engine/RHI/Mesh/mesh.hpp>
#include <Engine/RHI/Vulkan/vulkan_rhi_data_types.hpp>
#include <Editor/ContentBrowser/content_browser.hpp>
#include <PhysicsWrapper/physics_system.hpp>
#include <Scene/header/scene_manager.hpp>

// Externals
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan_raii.hpp>

// STL
#include <iostream>
#include <limits>

namespace gcep
{
    class Camera;

    // ─────────────────────────────────────────────────────────────────────────────

    /// @class ImGuiConsoleBuffer
    /// @brief Custom @c std::streambuf that tees all output into an ImGui console log.
    ///
    /// Wraps an existing stream buffer (typically @c std::cout's) and intercepts
    /// every character written to it. Characters are accumulated into a line buffer;
    /// on each newline the completed line is pushed into @c m_log and the original
    /// buffer receives the character unchanged, preserving normal terminal output.
    ///
    /// @par Usage
    /// Replace @c std::cout.rdbuf() with an instance of this class.
    /// Restore the original buffer on teardown via @c UiManager::shutdownConsole().
    class ImGuiConsoleBuffer : public std::streambuf
    {
    public:
        /// @brief Constructs the tee buffer.
        ///
        /// @param original  The stream buffer being wrapped. Must outlive this object.
        /// @param log       The string vector that receives completed lines.
        ImGuiConsoleBuffer(std::streambuf* original, std::vector<std::string>& log)
            : m_original(original), m_log(log)
        {}

    protected:
        /// @brief Intercepts a single character.
        ///
        /// On @c '\\n', flushes @c m_currentLine into @c m_log and resets it.
        /// All characters are forwarded to @c m_original unchanged.
        ///
        /// @param c  The character to process (as an @c int to match the @c streambuf contract).
        /// @returns  The result of forwarding @p c to the original buffer.
        int overflow(int c) override
        {
            if (c == '\n')
            {
                m_log.push_back(m_currentLine);
                m_currentLine.clear();
            }
            else
            {
                m_currentLine += static_cast<char>(c);
            }

            return m_original->sputc(static_cast<char>(c));
        }

    private:
        std::streambuf*           m_original;
        std::vector<std::string>& m_log;
        std::string               m_currentLine;
    };

    // ─────────────────────────────────────────────────────────────────────────────

    /// @class UiManager
    /// @brief Top-level editor UI manager built on Dear ImGui and ImGuizmo.
    ///
    /// @c UiManager owns the full ImGui per-frame lifecycle and renders the editor
    /// shell: a docked layout containing a 3D viewport, scene hierarchy, entity
    /// properties, scene settings, audio controls, content browser, and console.
    ///
    /// @par Owned subsystems
    /// - **ImGui / ImGuizmo** — context, GLFW + Vulkan backends, dockspace.
    /// - **Console redirection** — @c ImGuiConsoleBuffer tees @c std::cout /
    ///   @c std::cerr into @c m_consoleItems.
    /// - **Physics** — holds a reference to the singleton @c PhysicsSystem;
    ///   starts / pauses / stops simulation from the viewport toolbar.
    /// - **Audio** — holds a pointer to @c AudioSystem and a list of loaded
    ///   @c AudioSource handles managed from the audio control panel.
    ///
    /// @par Frame sequence (uiUpdate)
    /// @code
    /// beginFrame()
    /// if (!m_dockspaceInitialized)
    /// {
    ///     setupViewport()       // fullscreen host window
    ///     getDockspaceID()      // dockspace + main menu bar
    ///     initDockspace()       // one-time layout split (first frame only)
    /// }
    /// drawViewport()            // offscreen texture + gizmo overlay + toolbar
    /// drawSettings()            // render settings, camera, lighting, grid
    /// drawSceneHierarchy()      // entity list + spawn / remove buttons
    /// drawEntityProperties()    // transform / texture / physics / gizmo controls
    /// drawAudioControl()        // per-source playback controls
    /// drawConsole()             // scrolling log + text input
    /// // push scene and camera data to the RHI
    /// @endcode
    ///
    /// @par Default docked layout
    /// @code
    /// ┌──────────────┬──────────────────────────┬─────────────┐
    /// │  Hierarchy   │                          │             │
    /// ├──────────────┤        Viewport          │ Scene infos │
    /// │  Properties  │                          │             │
    /// │  Audio ctrl  ├──────────────────────────┤             │
    /// │              │        Console           │             │
    /// └──────────────┴──────────────────────────┴─────────────┘
    /// @endcode
    ///
    /// @note All methods must be called from the main / render thread.
    class UiManager
    {
    public:
        /// @brief Initialises the editor UI and binds it to the application systems.
        ///
        /// Steps performed:
        /// -# Stores @p window; obtains the @c AudioSystem singleton.
        /// -# Creates the ImGui context with docking and multi-viewport flags enabled.
        /// -# Loads @c Nunito-Regular.ttf at 18 pt and merges Font Awesome icons.
        /// -# Calls @c ImGui_ImplGlfw_InitForVulkan() and @c ImGui_ImplVulkan_Init().
        /// -# Calls @c initConsole() to redirect @c std::cout / @c std::cerr.
        ///
        /// @param window    The application GLFW window. Must outlive this object.
        /// @param manager   Pointer to the active scene manager. Must outlive this object.
        /// @param reload    Reference to the application reload flag; set to @c true
        ///                  when the user requests a project reload.
        /// @param close     Reference to the application close flag; set to @c true
        ///                  when the user requests exit.
        UiManager(GLFWwindow* window, SLS::SceneManager* manager, bool& reload, bool& close);

        /// @brief Calls @c shutdownConsole() to restore stream buffers.
        ~UiManager();

        /// @brief Binds the editor to the live RHI data produced by @c VulkanRHI.
        ///
        /// Stores pointers to the RHI instance, mesh list, ECS registry, and the
        /// ImGui viewport texture descriptor. Must be called before the first
        /// @c uiUpdate().
        ///
        /// @param initInfos  Pointer returned by @c VulkanRHI::getInitInfos().
        void setInfos(rhi::vulkan::InitInfos* initInfos);

        /// @brief Executes one full editor frame.
        ///
        /// Calls @c beginFrame(), lays out all docked panels, then pushes the
        /// updated scene and camera data to the RHI. If the window is minimised,
        /// sleeps for 10 ms and returns immediately.
        void uiUpdate();

        /// @brief Applies settings loaded from the project file to the editor state.
        ///
        /// Reads @c pl::ProjectLoader::getSettings() and propagates relevant values
        /// (camera speed, clear colour, grid parameters, etc.) to the internal
        /// UBO and push-constant structures.
        void applyLoadedSettings();

        /// @brief Renders gizmo operation / mode / snapping controls in the properties panel.
        ///
        /// Shows a combo for Translate / Rotate / Scale, a snap checkbox, and
        /// per-operation snap value inputs. Called by @c drawEntityProperties() when
        /// the simulation is not running.
        void drawGizmoControls();

        /// @brief Handles keyboard shortcuts that change the active gizmo operation.
        ///
        /// W = Translate, E = Rotate, R = Scale. Ignored when any ImGui window is
        /// unfocused, when @c ImGuizmo::IsUsing() is true, or while the simulation
        /// is actively playing.
        void handleGizmoInput();

        /// @brief Renders the ImGuizmo manipulation widget over the viewport.
        ///
        /// Early-returns if no entity is selected. Reads the selected entity's model
        /// matrix, calls @c ImGuizmo::Manipulate(), decomposes the result, and writes
        /// position / rotation / scale back into the ECS @c ECS::Transform component.
        void drawGizmo();

        void drawGizmoMesh(rhi::vulkan::Mesh &selectedMesh);

        void drawGizmoPointLight(rhi::vulkan::PointLight &selectedLight);

        void drawGizmoSpotLight(rhi::vulkan::SpotLight &selectedLight);

        /// @brief Stores a pointer to the active camera used for view / projection matrices.
        ///
        /// The camera is queried every frame inside @c drawViewport() and @c drawGizmo()
        /// to retrieve the view / projection matrices and to compute spawn positions.
        ///
        /// @param pCamera  Editor camera instance. Must outlive this object.
        void setCamera(Camera* pCamera);

        void setSceneManager(SLS::SceneManager *sceneManager);

        /// @brief Binds the project loader used to read / write project settings.
        ///
        /// @param loader  Non-owning pointer to the singleton @c ProjectLoader.
        void setProjectLoader(pl::ProjectLoader* loader) { m_projectLoader = loader; }

    private:
        // ── Initialisation helpers ────────────────────────────────────────────────

        /// @brief Positions the fullscreen host ImGui window over the OS viewport.
        ///
        /// Sets the next window position, size, and viewport to match the main
        /// @c ImGuiViewport, then returns it for use by @c initDockspace().
        ///
        /// @returns The main ImGui viewport.
        ImGuiViewport* setupViewport();

        /// @brief Creates the main dockspace and draws the menu bar inside its host window.
        ///
        /// Opens a borderless, immovable host window with @c PassthruCentralNode so
        /// the 3D viewport renders through it, creates a dockspace named
        /// @c "MainDockspace", calls @c drawMainMenuBar(), and returns the dockspace ID.
        ///
        /// @returns The @c ImGuiID of the dockspace, consumed by @c initDockspace().
        ImGuiID getDockspaceID();

        /// @brief Draws the "File" and "Window" top-level menus inside the menu bar.
        ///
        /// Called once per frame from inside the dockspace host window by
        /// @c getDockspaceID(). Currently only the Exit item is wired up.
        void drawMainMenuBar();

        /// @brief Performs a one-time dockspace layout split on the first frame.
        ///
        /// Splits the dockspace into three regions: left (hierarchy + properties /
        /// audio), centre (viewport + console), and right (scene infos). Docks
        /// each named window into its region via @c ImGui::DockBuilderDockWindow().
        ///
        /// @param dockspaceId  The dockspace ID returned by @c getDockspaceID().
        /// @param viewport     The main viewport returned by @c setupViewport().
        void initDockspace(const ImGuiID& dockspaceId, const ImGuiViewport* viewport);

        /// @brief Redirects @c std::cout and @c std::cerr to @c ImGuiConsoleBuffer.
        ///
        /// Saves the original @c std::cout buffer into @c m_oldCout, then installs
        /// a new @c ImGuiConsoleBuffer that tees output into @c m_consoleItems.
        void initConsole();

        // ── Shutdown helpers ──────────────────────────────────────────────────────

        /// @brief Restores the original @c std::cout / @c std::cerr buffers.
        ///
        /// Resets @c m_oldCout to @c nullptr after restoring to guard against
        /// double-restore on destruction.
        void shutdownConsole();

        // ── Per-frame helpers ─────────────────────────────────────────────────────

        /// @brief Starts a new ImGui + ImGuizmo frame and processes gizmo keyboard input.
        ///
        /// Calls @c ImGui_ImplVulkan_NewFrame(), @c ImGui_ImplGlfw_NewFrame(),
        /// @c ImGui::NewFrame(), @c ImGuizmo::BeginFrame(), then @c handleGizmoInput().
        void beginFrame();

        /// @brief Renders the 3D viewport panel.
        ///
        /// Displays a menu bar with Play / Pause / Stop simulation buttons and a
        /// status label. Detects viewport resize and calls
        /// @c VulkanRHI::requestOffscreenResize(). Shows the offscreen render texture
        /// via @c ImGui::Image() (or a "loading" placeholder). When the simulation is
        /// stopped or paused, calls @c drawGizmo() to overlay the manipulation widget.
        void drawViewport();

        /// @brief Renders the "Scene infos" settings panel.
        ///
        /// Exposes: V-Sync toggle, demo window checkbox, clear colour picker,
        /// entity / draw-call counters, camera transform controls, lighting parameters
        /// (ambient, diffuse colour, shininess, direction), infinite grid options
        /// (cell size, thick interval, fade distance, line width), and FPS / viewport
        /// size readouts.
        void drawSettings();

        /// @brief Renders the "Scene Hierarchy" panel.
        ///
        /// Lists all mesh entities under a "Scene" tree node. Clicking an entry sets
        /// @c m_selectedEntityID. Provides "Spawn cube", "Add asset" (OBJ file dialog),
        /// and "Remove selected" / Delete-key buttons. Clicking empty space deselects.
        void drawSceneHierarchy();

        /// @brief Renders the "Entity properties" panel for the selected entity.
        ///
        /// Syncs @c ECS::Transform and @c PhysicsComponent from the ECS registry each
        /// frame. Shows: editable name, position / rotation / scale via
        /// @c DrawVec3Control(), texture file dialog with LOD bias slider, physics
        /// motion type and layer combos, and gizmo controls when simulation is stopped.
        void drawEntityProperties();

        void showMeshInfos(rhi::vulkan::Mesh *mesh);

        void showSpotLightInfos(rhi::vulkan::SpotLight *spot);

        void showPointLightInfos(rhi::vulkan::PointLight *point);

        /// @brief Renders the "Audio control" panel.
        ///
        /// Calls @c AudioSystem::update() each frame. For each loaded @c AudioSource,
        /// displays a position drag and play / loop / spatialize checkboxes. An
        /// "Add audio source" button opens a file dialog for MP3, OGG, WAV, FLAC, MIDI.
        void drawAudioControl();

        /// @brief Renders the "Console" panel.
        ///
        /// Displays @c m_consoleItems in a scrolling child window with auto-scroll,
        /// a "Clear" button, a "Copy to clipboard" button, and a text-input field
        /// that appends submitted lines to the log.
        void drawConsole();

        /// @brief Renders the bottom status bar spanning the full window width.
        void drawBottomBar();

    private:
        /// @brief Non-owning pointer to the application GLFW window.
        GLFWwindow* m_window;

        /// @brief When true, renders the Dear ImGui demo window.
        bool m_showDemoWindow = false;

        /// @brief Current size of the 3D viewport content region in pixels.
        ImVec2 m_viewportSize = { 800, 600 };

        /// @brief Reference to the application's reload flag (set when the user requests reload).
        bool& m_reloadApp;

        /// @brief Reference to the application's close flag (set when the user requests exit).
        bool& m_isRunning;

        /// @brief Non-owning pointer to the RHI mesh list. Set via @c setInfos().
        std::vector<rhi::vulkan::Mesh>* m_meshData = nullptr;
        std::vector<rhi::vulkan::PointLight>* m_pointLights = nullptr;
        std::vector<rhi::vulkan::SpotLight>* m_spotLights = nullptr;

        /// @brief ECS ID of the entity currently selected in the hierarchy.
        ///        Set to @c UINT32_MAX when nothing is selected.
        ECS::EntityID m_selectedEntityID = UINT32_MAX;

        /// @brief Camera movement speed exposed to the UI for real-time tweaking.
        float m_camSpeed = 5.0f;

        /// @brief Scene-level UBO data pushed to the RHI every frame.
        rhi::vulkan::SceneInfos m_sceneInfos;

        /// @brief Grid push constant data pushed to the RHI every frame.
        rhi::vulkan::GridPushConstant m_gridPC;

        /// @brief Active ImGuizmo manipulation operation (Translate / Rotate / Scale).
        ImGuizmo::OPERATION m_currentGizmoOperation = ImGuizmo::TRANSLATE;

        /// @brief Active ImGuizmo reference space (WORLD or LOCAL).
        ///        Overridden to LOCAL for scale operations.
        ImGuizmo::MODE m_currentGizmoMode = ImGuizmo::WORLD;

        /// @brief When true, gizmo interactions snap to the configured increment.
        bool m_useSnap = false;

        /// @brief Snap increment for translation (X, Y, Z) in world units.
        float m_snapTranslation[3] = { 0.5f, 0.5f, 0.5f };

        /// @brief Snap increment for rotation in degrees.
        float m_snapRotation = 15.0f;

        /// @brief Snap increment for scale (uniform).
        float m_snapScale = 0.1f;

        /// @brief Non-owning pointer to the ECS registry. Set via @c setInfos().
        ECS::Registry* m_registry = nullptr;

        /// @brief Non-owning pointer to the editor camera. Set via @c setCamera().
        Camera* m_cameraRef = nullptr;

        /// @brief Non-owning pointer to the ImGui descriptor set for the viewport texture.
        VkDescriptorSet* m_viewportTexture = nullptr;

        /// @brief Non-owning pointer to the Vulkan RHI. Set via @c setInfos().
        rhi::vulkan::VulkanRHI* m_pRHI = nullptr;

        /// @brief Non-owning pointer to the audio subsystem singleton.
        AudioSystem* m_audioSystem = nullptr;

        /// @brief List of audio sources managed from the Audio control panel.
        std::vector<std::shared_ptr<gcep::AudioSource>> m_audioSources;

        // ── Console ───────────────────────────────────────────────────────────────

        /// @brief Lines captured from @c std::cout and @c std::cerr.
        std::vector<std::string> m_consoleItems;

        /// @brief Active console tee buffer; nullptr before @c initConsole().
        std::unique_ptr<ImGuiConsoleBuffer> m_consoleBuffer;

        /// @brief Original @c std::cout stream buffer, restored by @c shutdownConsole().
        std::streambuf* m_oldCout = nullptr;

        // ── Project ───────────────────────────────────────────────────────────────

        /// @brief Non-owning pointer to the project loader singleton.
        pl::ProjectLoader* m_projectLoader = nullptr;

        /// @brief In-panel file-system content browser.
        gcep::editor::ContentBrowser m_contentBrowser;

        // ── Scene ─────────────────────────────────────────────────────────────────

        /// @brief Current simulation lifecycle state (STOPPED, PLAYING, or PAUSED).
        SimulationState m_simulationState = SimulationState::STOPPED;

        /// @brief Non-owning pointer to the scene manager. Set at construction.
        SLS::SceneManager* m_sceneManager = nullptr;

        /// @brief Absolute filesystem path of the scene currently open in the editor.
        std::string m_currentScenePath;

        // ── Physics ───────────────────────────────────────────────────────────────

        /// @brief Reference to the singleton @c PhysicsSystem used to drive simulation.
        PhysicsSystem& m_physicsSystem;

        // ── Settings ──────────────────────────────────────────────────────────────

        /// @brief When true, the Settings panel is visible.
        bool m_showSettings = false;

        // ── Scripting ─────────────────────────────────────────────────────────────

        /// @brief Owns the script hot-reload manager. Initialised in the constructor
        ///        using project paths from ProjectLoader.
        ScriptHotReloadManager m_scriptManager;

        /// @brief Self-contained scripting panel (entity section + docked manager window).
        ScriptPanel m_scriptPanel;
    };
} // namespace gcep
