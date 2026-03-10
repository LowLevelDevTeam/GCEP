#pragma once

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

#include <Engine/Audio/audio_system.hpp>
#include <Engine/RHI/Mesh/Mesh.hpp>
#include <Engine/RHI/Vulkan/VulkanRHIDataTypes.hpp>
#include <PhysicsWrapper/physics_system.hpp>
#include <Engine/Core/Scene/header/scene_manager.hpp>
#include <Editor/SceneUtils/scene_util.hpp>
//#include <Scripting/ScriptSystem.hpp>
#include <Editor/Helpers.hpp>
#include <Editor/SceneUtils/scene_util.hpp>
#include <Engine/Core/ECS/Components/components.hpp>


namespace gcep
{
class Camera;

/// @brief Custom @c std::streambuf that tees all output into an ImGui console log.
///
/// Wraps an existing stream buffer (typically @c std::cout's) and intercepts
/// every character written to it. Characters are accumulated into a line buffer;
/// on each newline the completed line is pushed into @p log and the original
/// buffer receives the character unchanged, preserving normal terminal output.
///
/// @par Usage
///   Replace @c std::cout.rdbuf() with an instance of this class.
///   Restore the original buffer on teardown via @c shutdownConsole().
class ImGuiConsoleBuffer : public std::streambuf
{
public:
    /// @brief Constructs the tee buffer.
    ///
    /// @param original  The stream buffer being wrapped (must outlive this object).
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
    /// @param c  The character to process (as an @c int to match @c streambuf contract).
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

/// @brief Top-level editor UI manager built on Dear ImGui + ImGuizmo.
///
/// @c UiManager owns the full ImGui lifecycle (init, per-frame update, shutdown)
/// and renders the editor shell: a docked layout with a 3D viewport, scene
/// hierarchy, entity properties, scene settings, audio controls, and a console.
///
/// @par Owned subsystems
///   - ImGui / ImGuizmo: context, GLFW + Vulkan backends, dockspace.
///   - Console redirection: @c ImGuiConsoleBuffer tees @c std::cout /
///     @c std::cerr into @c m_consoleItems.
///   - Physics: holds a reference to the singleton @c PhysicsSystem;
///     starts/pauses/stops simulation from the viewport toolbar.
///   - Audio: holds a pointer to @c AudioSystem and a list of loaded
///     @c AudioSource handles managed from the Audio control panel.
/// @par Frame sequence (uiUpdate)
///   @code
///   beginFrame()              // ImGui + ImGuizmo new-frame housekeeping
///   if(!dockspaceInitialized)
///   {
///       setupViewport()       // position the fullscreen host window
///       getDockspaceID()      // create dockspace + main menu bar
///       initDockspace()       // one-time layout split (first frame only)
///   }
///   drawViewport()            // offscreen texture + gizmo overlay + sim toolbar
///   drawSettings()            // render settings, camera, lighting, grid
///   drawSceneHierarchy()      // entity list + spawn / remove buttons
///   drawEntityProperties()    // transform / texture / physics / gizmo controls
///   drawAudioControl()        // per-source playback controls
///   drawConsole()             // scrolling log + text input
///   // push scene and camera data to RHI
///   @endcode
///
/// @par Layout (first frame)
///   @code
///   ┌──────────────┬──────────────────────────┬─────────────┐
///   │  Hierarchy   │                          │             │
///   ├──────────────┤        Viewport          │ Scene infos │
///   │  Properties  │                          │             │
///   │  Audio ctrl  ├──────────────────────────┤             │
///   │              │        Console           │             │
///   └──────────────┴──────────────────────────┴─────────────┘
///   @endcode
///
/// @par Thread safety
///   All methods must be called from the main/render thread.
class UiManager
{
public:
    /// @brief Initialises ImGui, loads fonts, and sets up GLFW + Vulkan backends.
    ///
    /// Steps:
    ///   1. Stores @p window and @p initInfo; obtains the @c AudioSystem singleton.
    ///   2. Queries DPI scale via @c ImGui_ImplGlfw_GetContentScaleForMonitor.
    ///   3. Creates the ImGui context and enables @c DockingEnable /
    ///      @c ViewportsEnable flags.
    ///   4. Loads @c Nunito-Regular.ttf at 18 pt and merges Font Awesome icons.
    ///   5. Calls @c ImGui_ImplGlfw_InitForVulkan and @c ImGui_ImplVulkan_Init.
    ///   6. Calls @c initConsole() to redirect @c std::cout / @c std::cerr.
    ///
    /// @param window    The application GLFW window. Must outlive this object.
    /// @param initInfo  Vulkan backend initialisation info from @c VulkanRHI::getUIInitInfo().
    UiManager(GLFWwindow* window, bool& reload, bool& close);

    /// @brief Calls @c shutdownConsole() to restore stream buffers.
    ~UiManager();

    /// @brief Binds the editor to the live RHI data produced by @c VulkanRHI::getInitInfos().
    ///
    /// Stores pointers to the RHI instance, mesh list, ECS registry, and the
    /// ImGui viewport texture descriptor. Must be called before the first
    /// @c uiUpdate().
    ///
    /// @param initInfos  Pointer returned by @c VulkanRHI::getInitInfos().
    void setInfos(rhi::vulkan::InitInfos* initInfos);

    /// @brief Executes one full editor frame.
    ///
    /// Calls @c beginFrame(), lays out all docked panels, then pushes updated
    /// scene and camera data to the RHI via @c updateSceneUBO(), @c updateCameraUBO(),
    /// and @c setGridPC(). If the window is minimised, sleeps for 10 ms and
    /// returns immediately.
    void uiUpdate();

    /// @brief Draws the gizmo operation / mode / snapping controls inside the
    ///        Entity properties panel.
    ///
    /// Renders a combo for Translate / Rotate / Scale, a snap checkbox, and
    /// per-operation snap value inputs (DragFloat3 / DragFloat). Called by
    /// @c drawEntityProperties() when the simulation is not running.
    void drawGizmoControls();

    /// @brief Handles keyboard shortcuts that change the active gizmo operation.
    ///
    /// W = Translate, E = Rotate, R = Scale. Ignored when any ImGui window is
    /// unfocused or when @c ImGuizmo::IsUsing() is true, and also while the
    /// simulation is running and not paused.
    void handleGizmoInput();

    /// @brief Renders the ImGuizmo manipulation widget over the viewport.
    ///
    /// Early-returns if no entity is selected. Reads the selected mesh's current
    /// model matrix, calls @c ImGuizmo::Manipulate(), then decomposes the result
    /// and writes back position / rotation / scale into the ECS @c ECS::Transform.
    /// Scale operations are always performed in local space because ImGuizmo does
    /// not support world-space scaling.
    void drawGizmo();

    /// @brief Stores a pointer to the active camera used for view/projection matrices.
    ///
    /// Must be called before @c uiUpdate(). The camera is used to populate UBO
    /// data and to compute spawn positions for new assets.
    ///
    /// @param pCamera  Editor camera instance. Must outlive this object.
    void setCamera(Camera* pCamera);

    void setSceneManager(SLS::SceneManager* sceneManager);

    void setCurrentScenePath(const std::string &path);
private:
    // Init helpers

    /// @brief Positions the fullscreen host ImGui window over the OS viewport.
    ///
    /// Sets the next window position, size, and viewport to match
    /// @c ImGui::GetMainViewport(), then returns the viewport pointer for use
    /// by @c initDockspace().
    ///
    /// @returns The main ImGui viewport.
    ImGuiViewport* setupViewport();

    /// @brief Creates the main dockspace and draws the menu bar inside its host window.
    ///
    /// Opens a borderless, immovable host window with @c PassthruCentralNode so
    /// the 3D viewport renders through it, creates a dockspace named
    /// @c "MainDockspace", calls @c drawMainMenuBar(), then returns the dockspace ID.
    ///
    /// @returns The @c ImGuiID of the dockspace, used by @c initDockspace().
    ImGuiID getDockspaceID();

    /// @brief Draws the "File" and "Window" top-level menus inside the menu bar.
    ///
    /// Called once per frame from inside the dockspace host window by
    /// @c getDockspaceID(). Currently only the Exit item is wired up (no-op).
    void drawMainMenuBar();



    /// @brief Performs a one-time dockspace layout split on the first frame.
    ///
    /// Splits the dockspace into five regions: left (hierarchy + properties/audio),
    /// center (viewport + console), and right (scene infos). Docks each named
    /// window into its region via @c ImGui::DockBuilderDockWindow().
    ///
    /// @param dockspace_id  The dockspace ID returned by @c getDockspaceID().
    /// @param viewport      The main viewport returned by @c setupViewport().
    void initDockspace(const ImGuiID& dockspace_id, const ImGuiViewport* viewport);

    /// @brief Redirects @c std::cout and @c std::cerr to @c ImGuiConsoleBuffer.
    ///
    /// Saves the original @c std::cout buffer into @c m_oldCout, then installs
    /// a new @c ImGuiConsoleBuffer that tees output into @c m_consoleItems.
    void initConsole();

    // Shutdown

    /// @brief Restores the original @c std::cout / @c std::cerr buffers.
    ///
    /// Resets @c m_oldCout to @c nullptr after restoring to guard against
    /// double-restore on destruction.
    void shutdownConsole();

    // Per-frame helpers

    /// @brief Starts a new ImGui + ImGuizmo frame and processes gizmo keyboard input.
    ///
    /// Calls @c ImGui_ImplVulkan_NewFrame(), @c ImGui_ImplGlfw_NewFrame(),
    /// @c ImGui::NewFrame(), @c ImGuizmo::BeginFrame(), then @c handleGizmoInput().
    void beginFrame();

    /// @brief Renders the 3D viewport panel.
    ///
    /// Displays a menu bar with Play / Pause / Stop simulation buttons and a
    /// status label. Detects viewport resize and calls
    /// @c VulkanRHI::requestOffscreenResize(). Displays the offscreen texture via
    /// @c ImGui::Image(), or a "loading" placeholder if the texture is not yet ready.
    /// When the simulation is stopped or paused, calls @c drawGizmo() to overlay
    /// the manipulation widget. Also sets the @c ImGuizmo::SetRect() to match the
    /// content region.
    void drawViewport();

    /// @brief Renders the "Scene infos" panel.
    ///
    /// Exposes: V-Sync toggle, demo window checkbox, clear color picker, entity /
    /// draw-call counts, camera transform controls, lighting parameters
    /// (ambient, diffuse color, shininess, direction), infinite grid options
    /// (cell size, thick interval, fade distance, line width), and FPS / viewport
    /// size readouts.
    void drawSettings();

    /// @brief Renders the "Scene Hierarchy" panel.
    ///
    /// Lists all mesh entities under a "Scene" tree node. Clicking an entry sets
    /// @c m_selectedEntityID. Provides "Spawn cube", "Add asset" (OBJ file
    /// dialog), and "Remove selected" / Delete-key buttons. Clicking empty space
    /// deselects the current entity.
    void drawSceneHierarchy();

    /// @brief Renders the "Entity properties" panel for the selected entity.
    ///
    /// Syncs @c ECS::Transform and @c PhysicsComponent from the ECS registry
    /// each frame. Shows: editable name, position / rotation / scale via
    /// @c DrawVec3Control(), texture file dialog with LOD bias slider, physics
    /// motion type and layer combos, and gizmo controls when the simulation is
    /// not actively running.
    void drawEntityProperties();

    /// @brief Renders the "Audio control" panel.
    ///
    /// Calls @c AudioSystem::update() each frame. For each loaded @c AudioSource
    /// displays a position drag, play / loop / spatialize checkboxes. An
    /// "Add audio source" button opens a file dialog supporting MP3, OGG, WAV,
    /// FLAC, and MIDI.
    void drawAudioControl();

    /// @brief Renders the "Console" panel.
    ///
    /// Displays @c m_consoleItems in a scrolling child window with auto-scroll,
    /// a "Clear" button, a "Copy to clipboard" button, and a text input field
    /// that appends submitted lines to the log.
    void drawConsole();

    void drawBottomBar();



private:
    GLFWwindow*               m_window;
    bool                      showDemoWindow  = false;
    ImVec2                    m_viewportSize  = { 800, 600 };
    bool&                     reloadApp;
    bool&                     closeApp;

    std::vector<rhi::vulkan::Mesh>* meshData  = nullptr;

    /// @brief ECS ID of the entity currently selected in the hierarchy.
    ///        Set to @c numeric_limits<EntityID>::max() when nothing is selected.
    ECS::EntityID             m_selectedEntityID = UINT32_MAX;


    /// @brief Camera movement speed exposed to the UI for real-time tweaking.
    float     camSpeed        = 5.0f;

    rhi::vulkan::SceneInfos       sceneInfos;

    ImGuizmo::OPERATION m_currentGizmoOperation = ImGuizmo::TRANSLATE;

    /// @brief Current ImGuizmo space (WORLD or LOCAL). Overridden to LOCAL for scale.
    ImGuizmo::MODE      m_currentGizmoMode      = ImGuizmo::WORLD;

    bool  m_useSnap              = false;
    float m_snapTranslation[3]   = { 0.5f, 0.5f, 0.5f };
    float m_snapRotation         = 15.0f;  ///< Snap increment in degrees.
    float m_snapScale            = 0.1f;

    ECS::Registry*            m_registry      = nullptr;
    Camera*                   cameraRef       = nullptr;
    VkDescriptorSet*          viewportTexture = nullptr;
    rhi::vulkan::VulkanRHI*   pRHI            = nullptr;
    AudioSystem*              audioSystem     = nullptr;

    std::vector<std::shared_ptr<gcep::AudioSource>> audioSources;

    // Console
    std::vector<std::string>            m_consoleItems;
    std::unique_ptr<ImGuiConsoleBuffer> m_consoleBuffer;
    std::streambuf*                     m_oldCout = nullptr;


    // Scene
    SimulationState           m_simulationState = SimulationState::STOPPED;
    SLS::SceneManager*        m_sceneManager    = nullptr;
    std::string                 m_currentScenePath = "";

    // Physics
    PhysicsSystem& physicsSystem;    ///< Reference to the singleton @c PhysicsSystem.

    // Settings
    bool showSettings = false;

    // Scripts
    //scripting::ScriptSystem& scriptSystem;
};

} // Namespace gcep