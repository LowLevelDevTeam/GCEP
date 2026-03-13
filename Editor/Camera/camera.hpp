#pragma once

/// @file camera.hpp
/// @brief First-person editor camera driven by keyboard and mouse input.
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.4
/// @date 2026-02-17

#include <glm/glm.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

#include <Editor/Inputs/inputs.hpp>
#include <Editor/Window/window.hpp>
#include <Engine/RHI/Vulkan/VulkanRHI.hpp>

namespace gcep
{

/// @class Camera
/// @brief First-person editor camera that maps keyboard/mouse events to view and
///        projection matrices consumed by the RHI.
///
/// @c Camera polls an @c InputSystem each frame to translate WASD / QE keyboard
/// input into positional movement and drag-based mouse input into yaw/pitch
/// rotation. The resulting matrices are packed into a
/// @c rhi::vulkan::UniformBufferObject and returned by @c update().
///
/// @par Coordinate system
/// - The world-up axis is @c (0, 0, 1) (Z-up).
/// - The camera's local up is @c (0, 1, 0).
/// - Yaw is measured from the negative-X axis, pitch from the XY plane.
///
/// @par Typical usage
/// @code
/// Camera camera(&inputSystem, &window);
/// // each frame:
/// auto ubo = camera.update(aspectRatio, settings.cameraSpeed);
/// rhi->updateCameraUBO(ubo);
/// @endcode
class Camera
{
public:
    /// @brief Constructs the camera and binds it to an input source and window.
    ///
    /// @param inputs  Pointer to the active @c InputSystem. Must outlive this object.
    /// @param window  Pointer to the application @c Window. Must outlive this object.
    Camera(InputSystem* inputs, Window* window);

    /// @brief Builds and returns the view matrix from the current position and orientation.
    ///
    /// Computed as @c glm::lookAt(position, position + front, up).
    ///
    /// @returns A 4×4 view matrix in column-major order.
    [[nodiscard]] glm::mat4 getViewMatrix();

    /// @brief Builds and returns the perspective projection matrix.
    ///
    /// Uses a 60° vertical FOV, the window's current aspect ratio, and a
    /// near/far clip of 0.01 / 1000.
    ///
    /// @returns A 4×4 projection matrix in column-major order (Vulkan NDC).
    [[nodiscard]] glm::mat4 getProjectionMatrix();

    /// @brief Advances the camera simulation by one frame and returns the updated UBO.
    ///
    /// Processes movement keys (WASD / QE) scaled by @p camSpeed, updates
    /// yaw/pitch from right-mouse-button drag, then recomputes @c front,
    /// @c right, and @c velocity. Writes the new view/projection matrices and
    /// the camera position into @c ubo before returning it.
    ///
    /// @param aspect    Viewport width divided by height.
    /// @param camSpeed  Movement speed in world units per second.
    /// @returns         The updated @c UniformBufferObject ready to be uploaded to the GPU.
    [[nodiscard]] rhi::vulkan::UniformBufferObject update(float aspect, float camSpeed);

public:
    /// @brief Current velocity vector (world units / second).
    glm::vec3 m_velocity;

    /// @brief World-space position of the camera eye point.
    glm::vec3 m_position;

    /// @brief Unit vector pointing in the camera's look direction.
    glm::vec3 m_front = glm::vec3(-1.0f, 0.0f, 0.0f);

    /// @brief Vertical rotation angle, in degrees. Positive = tilt up.
    float m_pitch = 0.0f;

    /// @brief Horizontal rotation angle, in degrees. 0 = negative-X direction.
    float m_yaw = -180.0f;

private:
    /// @brief Moves the camera in the @c m_front direction, scaled by @c m_camSpeed.
    void moveForward();

    /// @brief Moves the camera opposite to the @c m_front direction.
    void moveBackward();

    /// @brief Strafes the camera to the left (negative @c m_right).
    void moveLeft();

    /// @brief Strafes the camera to the right (positive @c m_right).
    void moveRight();

    /// @brief Moves the camera upward along the world-up axis.
    void moveUp();

    /// @brief Moves the camera downward along the world-up axis.
    void moveDown();

    /// @brief Reads mouse delta and updates @c m_yaw and @c m_pitch.
    ///
    /// Active only while the right mouse button is held. Clamps @c m_pitch to
    /// ±89° to avoid gimbal lock.
    void rotate();

private:
    /// @brief Packed GPU-ready buffer updated every frame by @c update().
    rhi::vulkan::UniformBufferObject m_ubo{};

    /// @brief Base movement speed (world units / second). May be overridden via @c update().
    float m_camSpeed = 2.0f;

    /// @brief Non-owning pointer to the application window used for cursor queries.
    Window* m_window;

    /// @brief Camera-local up vector.
    glm::vec3 m_up = glm::vec3(0.0f, 1.0f, 0.0f);

    /// @brief Camera-local right vector (recomputed each frame).
    glm::vec3 m_right = glm::vec3(1.0f, 0.0f, 0.0f);

    /// @brief Absolute world-up axis used to re-orthogonalise the basis.
    glm::vec3 m_worldUp = glm::vec3(0.0f, 0.0f, 1.0f);
};

} // namespace gcep
