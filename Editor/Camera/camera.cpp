#include "camera.hpp"

#include <Editor/Helpers.hpp>



namespace gcep
{



void Camera::moveForward()
{
    if (isSpecificWindowFocused("Viewport"))
    {
        position += front * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
}
void Camera::moveBackward()
{
    if (isSpecificWindowFocused("Viewport"))
    {
        position -= front * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
}
void Camera::moveLeft()
{
    if (isSpecificWindowFocused("Viewport"))
    {
        position -= right * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
}
void Camera::moveRight()
{
    if (isSpecificWindowFocused("Viewport"))
    {
        position += right * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
}
void Camera::moveUp()
{
    if (isSpecificWindowFocused("Viewport"))
    {
        position += up * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
}
void Camera::moveDown()
{
    if (isSpecificWindowFocused("Viewport"))
    {
        position -= up * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
}

void Camera::rotate()
{
    if (isSpecificWindowFocused("Viewport") && !ImGuizmo::IsUsing())
    {
        float newPitch = pitch - ImGui::GetIO().MouseDelta.y * 180 / glm::pi<float>() * ImGui::GetIO().DeltaTime;
        if (newPitch > 89.0f)
        {
            pitch = 89.0f;
        }
        else if (newPitch < -89.0f)
        {
            pitch = -89.0f;
        }
        else
        {
            pitch = newPitch;
        }

        yaw -= ImGui::GetIO().MouseDelta.x * 180 / glm::pi<float>() * ImGui::GetIO().DeltaTime;
    }
}


Camera::Camera(Inputs* inputs, Window* window) : window(window)
{
    inputs->addTrackedKey(GLFW_KEY_W, std::bind(&Camera::moveForward, this));
    inputs->addTrackedKey(GLFW_KEY_S, std::bind(&Camera::moveBackward, this));
    inputs->addTrackedKey(GLFW_KEY_A, std::bind(&Camera::moveLeft, this));
    inputs->addTrackedKey(GLFW_KEY_D, std::bind(&Camera::moveRight, this));
    inputs->addTrackedKey(GLFW_KEY_SPACE, std::bind(&Camera::moveUp, this));
    inputs->addTrackedKey(GLFW_KEY_LEFT_SHIFT, std::bind(&Camera::moveDown, this));
    inputs->addTrackedKey(GLFW_MOUSE_BUTTON_LEFT, std::bind(&Camera::rotate, this));

    position = {5.0f, 0.0f, 2.0f};
    ubo.view  = glm::lookAt(position, {0.0f, 0.0f, 0.0f}, glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj  = glm::perspective(glm::radians(45.0f), 1.f, 0.1f, 1000.0f);
    ubo.proj[1][1] *= -1;
}

glm::mat4 Camera::getViewMatrix()
{
    return ubo.view;
}

glm::mat4 Camera::getProjectionMatrix()
{
    return ubo.proj;
}
rhi::vulkan::UniformBufferObject Camera::update(float aspect, float camSpeed)
{
    m_camSpeed = camSpeed;

    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.z = sin(glm::radians(pitch));
    front = glm::normalize(front);

    right = glm::normalize(glm::cross(front, worldUp));
    up    = glm::normalize(glm::cross(right, front));

    ubo.proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10000.0f);
    ubo.view = glm::lookAt(position, position + front, up);
    ubo.proj[1][1] *= -1;

    return ubo;
}

} // Namespace gcep