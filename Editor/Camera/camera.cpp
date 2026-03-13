#include "camera.hpp"

#include <Editor/Helpers.hpp>

namespace gcep
{

Camera::Camera(InputSystem* inputs, Window* window) : m_window(window)
{
    inputs->addTrackedKey(GLFW_KEY_W,             std::bind(&Camera::moveForward,  this));
    inputs->addTrackedKey(GLFW_KEY_S,             std::bind(&Camera::moveBackward, this));
    inputs->addTrackedKey(GLFW_KEY_A,             std::bind(&Camera::moveLeft,     this));
    inputs->addTrackedKey(GLFW_KEY_D,             std::bind(&Camera::moveRight,    this));
    inputs->addTrackedKey(GLFW_KEY_SPACE,         std::bind(&Camera::moveUp,       this));
    inputs->addTrackedKey(GLFW_KEY_LEFT_SHIFT,    std::bind(&Camera::moveDown,     this));
    inputs->addTrackedKey(GLFW_MOUSE_BUTTON_LEFT, std::bind(&Camera::rotate,       this));

    m_position       = { 5.0f, 0.0f, 2.0f };
    m_ubo.view       = glm::lookAt(m_position, { 0.0f, 0.0f, 0.0f }, glm::vec3(0.0f, 0.0f, 1.0f));
    m_ubo.proj       = glm::perspective(glm::radians(45.0f), 1.f, 0.1f, 1000.0f);
    m_ubo.proj[1][1] *= -1;
}

void Camera::moveForward()
{
    if (isSpecificWindowFocused("Viewport"))
    {
        m_position += m_front * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
}

void Camera::moveBackward()
{
    if (isSpecificWindowFocused("Viewport"))
    {
        m_position -= m_front * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
}

void Camera::moveLeft()
{
    if (isSpecificWindowFocused("Viewport"))
    {
        m_position -= m_right * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
}

void Camera::moveRight()
{
    if (isSpecificWindowFocused("Viewport"))
    {
        m_position += m_right * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
}

void Camera::moveUp()
{
    if (isSpecificWindowFocused("Viewport"))
    {
        m_position += m_up * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
}

void Camera::moveDown()
{
    if (isSpecificWindowFocused("Viewport"))
    {
        m_position -= m_up * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
}

void Camera::rotate()
{
    if (isSpecificWindowFocused("Viewport") && !ImGuizmo::IsUsing())
    {
        static float DPI = 1.0f / 1000.0f;
        float newPitch = m_pitch - ImGui::GetIO().MouseDelta.y * 180 / glm::pi<float>() * DPI;
        if (newPitch > 89.0f)
        {
            m_pitch = 89.0f;
        }
        else if (newPitch < -89.0f)
        {
            m_pitch = -89.0f;
        }
        else
        {
            m_pitch = newPitch;
        }

        m_yaw -= ImGui::GetIO().MouseDelta.x * 180 / glm::pi<float>() * DPI;
    }
}

glm::mat4 Camera::getViewMatrix()
{
    return m_ubo.view;
}

glm::mat4 Camera::getProjectionMatrix()
{
    return m_ubo.proj;
}

rhi::vulkan::UniformBufferObject Camera::update(float aspect, float camSpeed)
{
    m_camSpeed = camSpeed;

    m_front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front.y = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front.z = sin(glm::radians(m_pitch));
    m_front   = glm::normalize(m_front);

    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up    = glm::normalize(glm::cross(m_right, m_front));

    m_ubo.proj       = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10000.0f);
    m_ubo.view       = glm::lookAt(m_position, m_position + m_front, m_up);
    m_ubo.proj[1][1] *= -1;

    return m_ubo;
}

} // namespace gcep
