
#include "camera.hpp"
#include <iostream>
#include <ostream>


namespace gcep
{


    void Camera::moveForward()
    {
        position += front * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
    void Camera::moveBackward()
    {
        position -= front * m_camSpeed * ImGui::GetIO().DeltaTime;;
    }
    void Camera::moveLeft()
    {
        position -= right * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
    void Camera::moveRight()
    {
        position += right * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
    void Camera::moveUp()
    {
        position += up * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
    void Camera::moveDown()
    {
        position -= up * m_camSpeed * ImGui::GetIO().DeltaTime;
    }

    void Camera::rotateUp()
    {
        pitch += m_camSpeed * 180 / glm::pi<float>() * ImGui::GetIO().DeltaTime;

    }
    void Camera::rotateDown()
    {
        pitch -= m_camSpeed * 180 / glm::pi<float>() * ImGui::GetIO().DeltaTime;
    }
    void Camera::rotateLeft()
    {
        yaw += m_camSpeed * 180 / glm::pi<float>() * ImGui::GetIO().DeltaTime;
    }
    void Camera::rotateRight()
    {
        yaw -= m_camSpeed * 180 / glm::pi<float>() * ImGui::GetIO().DeltaTime;
    }

    Camera::Camera(Inputs* inputs)
    {
        inputs->addTrackedKey(GLFW_KEY_W, std::bind(&Camera::moveForward, this));
        inputs->addTrackedKey(GLFW_KEY_S, std::bind(&Camera::moveBackward, this));
        inputs->addTrackedKey(GLFW_KEY_A, std::bind(&Camera::moveLeft, this));
        inputs->addTrackedKey(GLFW_KEY_D, std::bind(&Camera::moveRight, this));
        inputs->addTrackedKey(GLFW_KEY_SPACE, std::bind(&Camera::moveUp, this));
        inputs->addTrackedKey(GLFW_KEY_LEFT_SHIFT, std::bind(&Camera::moveDown, this));
        inputs->addTrackedKey(GLFW_KEY_UP, std::bind(&Camera::rotateUp, this));
        inputs->addTrackedKey(GLFW_KEY_DOWN, std::bind(&Camera::rotateDown, this));
        inputs->addTrackedKey(GLFW_KEY_LEFT , std::bind(&Camera::rotateLeft, this));
        inputs->addTrackedKey(GLFW_KEY_RIGHT, std::bind(&Camera::rotateRight, this));

        position = {10.0f, 1.0f, 1.0f};
        ubo.view  = glm::lookAt(position, {0.0f, 0.0f, 0.0f}, glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj  = glm::perspective(glm::radians(45.0f), 1.f, 0.1f, 1000.0f);
        ubo.proj[1][1] *= -1;
    }

    glm::mat4 Camera::getViewMatrix()
    {
        return ubo.view;
    }


    glm::mat4 Camera::getRotationMatrix()
    {
        return glm::mat4();
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

        ubo.proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);
        ubo.view = glm::lookAt(position, position + front, up);
        ubo.proj[1][1] *= -1;

        return ubo;
    }
} // GCEP