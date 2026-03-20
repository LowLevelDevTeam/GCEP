#pragma once

/// @file inputs.hpp
/// @brief Callback-based keyboard input system built on GLFW.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.4
/// @date 2026-02-17

// Internals
#include <Scripting/API/key_codes.hpp>
#include <Editor/Window/window.hpp>

// Externals
#include <GLFW/glfw3.h>

// STL
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>

namespace gcep
{

    enum class KeyState : uint8_t
    {
        Up       = 0,  ///< Not held, was not held last frame
        Pressed  = 1,  ///< Became held this frame
        Held     = 2,  ///< Held last frame and this frame
        Released = 3,  ///< Was held last frame, not this frame
    };

    enum class InputDevice : uint8_t { Keyboard, Mouse };

    struct KeyBinding
    {
        int                   key;
        TriggerOn             trigger;
        InputDevice           device = InputDevice::Keyboard;
        std::function<void()> callback;
    };

    struct MouseState
    {
        double posX    = 0.0, posY    = 0.0; ///< Cursor position (pixels)
        double deltaX  = 0.0, deltaY  = 0.0; ///< Movement since last frame
        double scrollX = 0.0, scrollY = 0.0; ///< Scroll this frame (reset each frame)
    };

    /// @class InputSystem
    /// @brief Polls registered GLFW key states and dispatches callbacks each frame.
    ///
    /// @c InputSystem provides a lightweight, callback-driven abstraction over raw
    /// GLFW key polling. Callers register @c (key, callback) pairs via
    /// @c addBinding(). Each call to @c update() polls every registered key;
    /// when a key is held down (GLFW_PRESS), its associated callback is invoked
    /// immediately on the calling thread.
    ///
    /// @par Typical usage
    /// @code
    /// InputSystem input(window);
    /// input.addBinding("Up",   gcep::Key::W, gcep::TriggerOn::Held,  [&]{ camera.moveForward(); });
    /// input.addBinding("Quit", gcep::Key::Q, gcep::TriggerOn::Press, [&]{ running = false; });
    ///
    /// while (running)
    /// {
    ///     glfwPollEvents();
    ///     input.update();   // fires callbacks for all pressed keys
    /// }
    /// @endcode
    ///
    /// @note @c update() must be called after @c glfwPollEvents() so that GLFW's
    ///       internal key state is up-to-date.
    /// @note Only one callback per key is supported; registering the same key twice
    ///       overwrites the previous entry.
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

        /// @brief Advance frame: snapshot new states, compute deltas, fire callbacks.
        /// Must be called once per frame after glfwPollEvents().
        void update();

        // Polling API

        [[nodiscard]] bool isPressed (gcep::Key k) const { return isPressed (static_cast<int>(k)); }
        [[nodiscard]] bool isHeld    (gcep::Key k) const { return isHeld    (static_cast<int>(k)); }
        [[nodiscard]] bool isReleased(gcep::Key k) const { return isReleased(static_cast<int>(k)); }

        [[nodiscard]] bool isMousePressed (gcep::MouseButton b) const { return isMousePressed (static_cast<int>(b)); }
        [[nodiscard]] bool isMouseHeld    (gcep::MouseButton b) const { return isMouseHeld    (static_cast<int>(b)); }
        [[nodiscard]] bool isMouseReleased(gcep::MouseButton b) const { return isMouseReleased(static_cast<int>(b)); }

        /// @brief Register a named binding (name is used for removeBinding).
        void addBinding(const std::string& name, gcep::Key key, gcep::TriggerOn trigger, std::function<void()> callback)
        {
            addBinding(name, static_cast<int>(key), static_cast<TriggerOn>(trigger), std::move(callback));
        }
        void addMouseBinding(const std::string& name, gcep::MouseButton mouseButton, TriggerOn trigger, std::function<void()> callback)
        {
            addMouseBinding(name, static_cast<int>(mouseButton), static_cast<TriggerOn>(trigger), std::move(callback));
        }

        [[nodiscard]] const MouseState& mouse() const { return m_mouse; }

        // Callback API

        void removeBinding(const std::string& name);
        void clearBindings();

        // State

        [[nodiscard]] GLFWwindow*  window() const { return m_window; }

        // State accessors

        [[nodiscard]] bool     isPressed (int key)  const;
        [[nodiscard]] bool     isHeld    (int key)  const;
        [[nodiscard]] bool     isReleased(int key)  const;
        [[nodiscard]] KeyState keyState  (int key)  const;

        [[nodiscard]] bool     isMousePressed (int button) const;
        [[nodiscard]] bool     isMouseHeld    (int button) const;
        [[nodiscard]] bool     isMouseReleased(int button) const;

        void addBinding     (const std::string& name, int key, TriggerOn trigger, std::function<void()> callback);
        void addMouseBinding(const std::string& name, int mouseButton, TriggerOn trigger, std::function<void()> callback);

    private:

        // State helpers

        KeyState computeState(bool prevDown, bool currDown) const;
        bool     glfwKeyDown (int key)    const;
        bool     glfwMouseDown(int button) const;

        // GLFW scroll callback

        static void scrollCallback(GLFWwindow* w, double xoff, double yoff);
        void        onScroll(double xoff, double yoff);

        // Data

        GLFWwindow*  m_window       = nullptr;

        // Key state: prev + curr frame bitmaps
        std::unordered_map<int, bool> m_prevKeys;
        std::unordered_map<int, bool> m_currKeys;

        // Mouse button state
        std::unordered_map<int, bool> m_prevMouse;
        std::unordered_map<int, bool> m_currMouse;

        MouseState m_mouse;
        double     m_prevPosX = 0.0, m_prevPosY = 0.0;

        // Scroll is written by callback, read + reset in update()
        double m_pendingScrollX = 0.0, m_pendingScrollY = 0.0;

        // Named bindings
        std::vector<std::pair<std::string, KeyBinding>> m_bindings;

        // GLFW userpointer slot management
        static InputSystem* s_scrollTarget;
    };
} // namespace gcep
