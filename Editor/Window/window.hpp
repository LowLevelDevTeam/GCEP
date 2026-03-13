#pragma once

/// @file window.hpp
/// @brief Singleton GLFW window wrapper for the editor application.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.4
/// @date 2026-02-17

#include <GLFW/glfw3.h>

namespace gcep
{

/// @class Window
/// @brief Singleton that owns the GLFW window for the entire editor process.
///
/// @c Window wraps a @c GLFWwindow* and exposes the minimal interface needed by
/// the rest of the editor: initialisation, the native handle, close-request
/// polling, and destruction.
///
/// The class is a Meyer's singleton: the unique instance is created on the first
/// call to @c getInstance() and lives until program exit.
///
/// @par Typical usage
/// @code
/// Window& win = Window::getInstance();
/// win.initWindow();
///
/// while (!win.shouldClose())
/// {
///     glfwPollEvents();
///     // render frame ...
/// }
///
/// win.destroy();
/// @endcode
///
/// @note The copy constructor and copy-assignment operator are deleted to enforce
///       the singleton invariant.
class Window
{
public:
    /// @brief Returns the process-wide singleton instance.
    /// @returns Reference to the unique @c Window instance.
    static Window& getInstance()
    {
        static Window instance;
        return instance;
    }

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    /// @brief Creates and displays the GLFW window.
    ///
    /// Initialises GLFW, sets Vulkan-specific window hints (no OpenGL context),
    /// creates the window with a hard-coded default resolution, sets the window
    /// icon, and centres it on the primary monitor via @c centerWindow().
    ///
    /// @note Must be called exactly once before any other method.
    void initWindow();

    /// @brief Returns the underlying GLFW window handle.
    ///
    /// @returns Non-null @c GLFWwindow* once @c initWindow() has been called.
    [[nodiscard]] GLFWwindow* getGlfwWindow();

    /// @brief Queries whether the user has requested the window to close.
    ///
    /// Delegates to @c glfwWindowShouldClose().
    ///
    /// @returns @c true if the close button has been pressed or @c glfwSetWindowShouldClose()
    ///          was called with @c true.
    [[nodiscard]] bool shouldClose();

    /// @brief Destroys the GLFW window and terminates the GLFW library.
    ///
    /// Safe to call multiple times; subsequent calls after the first are no-ops.
    void destroy();

private:
    Window()  = default;
    ~Window();

    /// @brief Centres the window on the primary monitor.
    ///
    /// Reads the primary monitor's video mode and the window's framebuffer size,
    /// then calls @c glfwSetWindowPos() to place the window at the centre.
    void centerWindow();

private:
    /// @brief The underlying GLFW window handle. Null before @c initWindow().
    GLFWwindow* m_window = nullptr;
};

} // namespace gcep
