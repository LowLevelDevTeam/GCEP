#pragma once

/// @file inputs.hpp
/// @brief Callback-based keyboard input system built on GLFW.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.4
/// @date 2026-02-17

#include <GLFW/glfw3.h>

#include <functional>
#include <optional>
#include <unordered_map>

namespace gcep
{

/// @class InputSystem
/// @brief Polls registered GLFW key states and dispatches callbacks each frame.
///
/// @c InputSystem provides a lightweight, callback-driven abstraction over raw
/// GLFW key polling. Callers register @c (key, callback) pairs via
/// @c addTrackedKey(). Each call to @c update() polls every registered key;
/// when a key is held down (GLFW_PRESS), its associated callback is invoked
/// immediately on the calling thread.
///
/// @par Typical usage
/// @code
/// InputSystem input(window);
/// input.addTrackedKey(GLFW_KEY_W, [&]{ camera.moveForward(); });
/// input.addTrackedKey(GLFW_KEY_ESCAPE, [&]{ running = false; });
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
    /// @brief Binds the input system to a GLFW window.
    ///
    /// @param window  The GLFW window whose key state will be polled.
    ///                Must outlive this object.
    explicit InputSystem(GLFWwindow* window);

    /// @brief Registers a callback to be fired while @p key is held down.
    ///
    /// If @p key was already registered, its previous callback is replaced.
    ///
    /// @param key       GLFW key token (e.g. @c GLFW_KEY_W).
    /// @param callback  Zero-argument callable invoked each frame the key is pressed.
    void addTrackedKey(int key, std::function<void()> callback);

    /// @brief Polls all registered keys and fires their callbacks if pressed.
    ///
    /// For each entry in @c m_trackedKeys, calls @c glfwGetKey() and invokes the
    /// stored callback when the result is @c GLFW_PRESS. Must be called once
    /// per frame after @c glfwPollEvents().
    void update();

private:
    /// @brief Non-owning pointer to the GLFW window used for key-state queries.
    GLFWwindow* m_windowRef;

    /// @brief Map from GLFW key token to the callback fired while that key is held.
    std::unordered_map<int, std::function<void()>> m_trackedKeys{};
};

} // namespace gcep
