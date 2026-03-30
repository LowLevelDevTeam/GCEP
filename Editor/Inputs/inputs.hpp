#pragma once

/// @file inputs.hpp
/// @brief Callback-based keyboard and mouse input system built on GLFW.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.4
/// @date 2026-02-17

// Internals
#include <Editor/Window/window.hpp>
#include <Scripting/API/key_codes.hpp>

// Externals
#include <GLFW/glfw3.h>

// STL
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gcep
{
    /// @brief Per-key transition state, updated once per frame by @c InputSystem::update().
    enum class KeyState : uint8_t
    {
        Up       = 0, ///< Key is not held and was not held last frame.
        Pressed  = 1, ///< Key became held this frame (rising edge).
        Held     = 2, ///< Key was held last frame and remains held this frame.
        Released = 3, ///< Key was held last frame but is no longer held (falling edge).
    };

    /// @brief Identifies which physical device a @c KeyBinding listens to.
    enum class InputDevice : uint8_t
    {
        Keyboard, ///< Standard keyboard key codes.
        Mouse,    ///< Mouse button codes.
    };

    /// @brief Associates a key or mouse button with a named, trigger-conditioned callback.
    struct KeyBinding
    {
        int                   key;                         ///< Raw GLFW key or mouse button code.
        TriggerOn             trigger;                     ///< Condition under which the callback fires (Pressed / Held / Released).
        InputDevice           device = InputDevice::Keyboard; ///< Whether @c key refers to a keyboard key or a mouse button.
        std::function<void()> callback;                    ///< Callable invoked by @c InputSystem::update() when the trigger condition is met.
    };

    /// @brief Snapshot of mouse axes captured at the start of each frame by @c InputSystem::update().
    struct MouseState
    {
        double posX    = 0.0, posY    = 0.0; ///< Cursor position in screen pixels (top-left origin).
        double deltaX  = 0.0, deltaY  = 0.0; ///< Cursor movement since the previous frame (pixels).
        double scrollX = 0.0, scrollY = 0.0; ///< Scroll-wheel offset this frame; reset to zero at the start of the next frame.
    };

    /// @brief Polls registered GLFW key states and dispatches callbacks each frame.
    ///
    /// @c InputSystem provides a lightweight, callback-driven abstraction over raw
    /// GLFW key polling. Callers register named @c (key, callback) pairs via
    /// @c addBinding(). Each call to @c update() snapshots every tracked key,
    /// computes per-key @c KeyState transitions, updates @c MouseState axes, and
    /// then fires every callback whose trigger condition is satisfied.
    ///
    /// @par Typical usage
    /// @code
    /// InputSystem input;
    /// input.addBinding("MoveForward", gcep::Key::W, gcep::TriggerOn::Held,    [&]{ camera.moveForward(); });
    /// input.addBinding("Quit",        gcep::Key::Q, gcep::TriggerOn::Pressed, [&]{ running = false; });
    ///
    /// while (running)
    /// {
    ///     glfwPollEvents();
    ///     input.update();   // snapshots state, fires matched callbacks
    /// }
    /// @endcode
    ///
    /// @note @c update() must be called **after** @c glfwPollEvents() so that GLFW's
    ///       internal key state is current.
    /// @note Registering two bindings with the same @p name via @c addBinding()
    ///       overwrites the previous entry; the key code itself is not unique.
    class InputSystem
    {
    public:
        InputSystem();
        ~InputSystem();

        // Non-copyable, movable
        InputSystem(const InputSystem&)            = delete;
        InputSystem& operator=(const InputSystem&) = delete;
        InputSystem(InputSystem&&)                 = default;
        InputSystem& operator=(InputSystem&&)      = default;

        /// @brief Advances one frame: snapshots key states, computes axis deltas, and fires callbacks.
        ///
        /// For every tracked key and mouse button, calls @c glfwGetKey() /
        /// @c glfwGetMouseButton(), derives the @c KeyState transition from the
        /// previous frame's snapshot, and evaluates all registered @c KeyBinding
        /// entries. Callbacks whose @c TriggerOn condition matches the current
        /// @c KeyState are invoked synchronously on the calling thread. Mouse
        /// position delta and pending scroll are also committed to @c m_mouse here.
        ///
        /// Must be called once per frame, after @c glfwPollEvents().
        void update();

        // ── Polling API ───────────────────────────────────────────────────────

        /// @brief Returns @c true on the exact frame @p k transitioned to pressed.
        [[nodiscard]] bool isPressed (gcep::Key k) const { return isPressed (static_cast<int>(k)); }
        /// @brief Returns @c true every frame @p k is held down.
        [[nodiscard]] bool isHeld    (gcep::Key k) const { return isHeld    (static_cast<int>(k)); }
        /// @brief Returns @c true on the exact frame @p k was released.
        [[nodiscard]] bool isReleased(gcep::Key k) const { return isReleased(static_cast<int>(k)); }

        /// @brief Returns @c true on the exact frame mouse button @p b was pressed.
        [[nodiscard]] bool isMousePressed (gcep::MouseButton b) const { return isMousePressed (static_cast<int>(b)); }
        /// @brief Returns @c true every frame mouse button @p b is held.
        [[nodiscard]] bool isMouseHeld    (gcep::MouseButton b) const { return isMouseHeld    (static_cast<int>(b)); }
        /// @brief Returns @c true on the exact frame mouse button @p b was released.
        [[nodiscard]] bool isMouseReleased(gcep::MouseButton b) const { return isMouseReleased(static_cast<int>(b)); }

        // ── Binding registration ──────────────────────────────────────────────

        /// @brief Registers a named keyboard binding.
        ///
        /// If a binding with @p name already exists it is replaced. The binding
        /// fires @p callback each frame the @p trigger condition is satisfied.
        ///
        /// @param name      Unique identifier used for later removal via @c removeBinding().
        /// @param key       Key to listen for.
        /// @param trigger   When to fire: @c Pressed, @c Held, or @c Released.
        /// @param callback  Callable invoked when the condition is met.
        void addBinding(const std::string& name, gcep::Key key, gcep::TriggerOn trigger, std::function<void()> callback)
        {
            addBinding(name, static_cast<int>(key), static_cast<TriggerOn>(trigger), std::move(callback));
        }

        /// @brief Registers a named mouse-button binding.
        ///
        /// @param name         Unique identifier for later removal.
        /// @param mouseButton  Mouse button to listen for.
        /// @param trigger      When to fire: @c Pressed, @c Held, or @c Released.
        /// @param callback     Callable invoked when the condition is met.
        void addMouseBinding(const std::string& name, gcep::MouseButton mouseButton, TriggerOn trigger, std::function<void()> callback)
        {
            addMouseBinding(name, static_cast<int>(mouseButton), static_cast<TriggerOn>(trigger), std::move(callback));
        }

        /// @brief Returns the current mouse axis state (position, delta, scroll).
        [[nodiscard]] const MouseState& mouse() const { return m_mouse; }

        // ── Binding removal ───────────────────────────────────────────────────

        /// @brief Removes the binding registered under @p name. No-op if not found.
        /// @param name  The name passed to @c addBinding() or @c addMouseBinding().
        void removeBinding(const std::string& name);

        /// @brief Removes all registered key and mouse bindings.
        void clearBindings();

        // ── Window accessor ───────────────────────────────────────────────────

        /// @brief Returns the GLFW window this system reads input from.
        [[nodiscard]] GLFWwindow* window() const { return m_window; }

        // ── Raw integer state accessors ───────────────────────────────────────

        /// @brief Returns @c true on the exact frame raw key code @p key was pressed.
        [[nodiscard]] bool     isPressed (int key)  const;
        /// @brief Returns @c true every frame raw key code @p key is held.
        [[nodiscard]] bool     isHeld    (int key)  const;
        /// @brief Returns @c true on the exact frame raw key code @p key was released.
        [[nodiscard]] bool     isReleased(int key)  const;
        /// @brief Returns the full @c KeyState for raw key code @p key.
        [[nodiscard]] KeyState keyState  (int key)  const;

        /// @brief Returns @c true on the exact frame raw mouse button @p button was pressed.
        [[nodiscard]] bool isMousePressed (int button) const;
        /// @brief Returns @c true every frame raw mouse button @p button is held.
        [[nodiscard]] bool isMouseHeld    (int button) const;
        /// @brief Returns @c true on the exact frame raw mouse button @p button was released.
        [[nodiscard]] bool isMouseReleased(int button) const;

        /// @brief Registers a named keyboard binding using raw integer key and trigger codes.
        void addBinding     (const std::string& name, int key,         TriggerOn trigger, std::function<void()> callback);
        /// @brief Registers a named mouse-button binding using raw integer codes.
        void addMouseBinding(const std::string& name, int mouseButton, TriggerOn trigger, std::function<void()> callback);

    private:

        // ── State helpers ─────────────────────────────────────────────────────

        /// @brief Derives a @c KeyState from previous and current down-state booleans.
        /// @param prevDown  Was the key down last frame?
        /// @param currDown  Is the key down this frame?
        /// @return          The corresponding @c KeyState transition.
        KeyState computeState(bool prevDown, bool currDown) const;

        /// @brief Returns @c true if GLFW reports @p key as pressed this frame.
        bool glfwKeyDown  (int key)    const;
        /// @brief Returns @c true if GLFW reports mouse @p button as pressed this frame.
        bool glfwMouseDown(int button) const;

        // ── GLFW scroll callback ──────────────────────────────────────────────

        /// @brief Static GLFW scroll callback; forwards to the registered @c InputSystem via @c s_scrollTarget.
        static void scrollCallback(GLFWwindow* w, double xoff, double yoff);

        /// @brief Instance-level scroll handler called by @c scrollCallback().
        ///
        /// Accumulates @p xoff and @p yoff into @c m_pendingScrollX / @c m_pendingScrollY,
        /// which are committed to @c m_mouse and reset during the next @c update() call.
        ///
        /// @param xoff  Horizontal scroll delta from GLFW.
        /// @param yoff  Vertical scroll delta from GLFW.
        void onScroll(double xoff, double yoff);

        // ── Data ──────────────────────────────────────────────────────────────

        GLFWwindow* m_window = nullptr; ///< Non-owning GLFW window handle used for key and cursor queries.

        std::unordered_map<int, bool> m_prevKeys;  ///< Down-state of each tracked key at the end of the previous frame.
        std::unordered_map<int, bool> m_currKeys;  ///< Down-state of each tracked key as of the current frame snapshot.

        std::unordered_map<int, bool> m_prevMouse; ///< Down-state of each tracked mouse button at the end of the previous frame.
        std::unordered_map<int, bool> m_currMouse; ///< Down-state of each tracked mouse button as of the current frame snapshot.

        MouseState m_mouse;                        ///< Committed mouse axis state for the current frame.
        double     m_prevPosX = 0.0, m_prevPosY = 0.0; ///< Cursor position captured at the previous frame, used to compute @c MouseState::deltaX/Y.

        double m_pendingScrollX = 0.0, m_pendingScrollY = 0.0; ///< Scroll accumulated by @c scrollCallback() between frames; consumed and reset by @c update().

        std::vector<std::pair<std::string, KeyBinding>> m_bindings; ///< All registered named key and mouse bindings.

        /// @brief Pointer to the @c InputSystem that currently owns the GLFW scroll callback slot.
        ///
        /// Only one @c InputSystem can receive scroll events at a time. Set during
        /// construction and cleared during destruction.
        static InputSystem* s_scrollTarget;
    };

} // namespace gcep
