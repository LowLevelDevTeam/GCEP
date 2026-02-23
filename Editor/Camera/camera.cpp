
#include "camera.hpp"
#include <iostream>
#include <ostream>


namespace gcep
{


    void Camera::moveForward()
    {
        std::cout<<"move forward"<<std::endl;
        position += front * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
    void Camera::moveBackward()
    {
        std::cout<<"move backward"<<std::endl;
        position -= front * m_camSpeed * ImGui::GetIO().DeltaTime;;
    }
    void Camera::moveLeft()
    {
        std::cout<<"move left"<<std::endl;
        position -= right * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
    void Camera::moveRight()
    {
        std::cout<<"move right"<<std::endl;
        position += right * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
    void Camera::moveUp()
    {
        std::cout<<"move up"<<std::endl;
        position += up * m_camSpeed * ImGui::GetIO().DeltaTime;
    }
    void Camera::moveDown()
    {
        std::cout<<"move down"<<std::endl;
        position -= up * m_camSpeed * ImGui::GetIO().DeltaTime;
    }

    void Camera::rotateUp()
    {
        std::cout<<"rotate up"<<std::endl;
        pitch += m_camSpeed * 180 / glm::pi<float>() * ImGui::GetIO().DeltaTime;

    }
    void Camera::rotateDown()
    {
        std::cout<<"rotate down"<<std::endl;
        pitch -= m_camSpeed * 180 / glm::pi<float>() * ImGui::GetIO().DeltaTime;
    }
    void Camera::rotateLeft()
    {
        std::cout<<"rotate left"<<std::endl;
        yaw += m_camSpeed * 180 / glm::pi<float>() * ImGui::GetIO().DeltaTime;
    }
    void Camera::rotateRight()
    {
        std::cout<<"rotate right"<<std::endl;
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



        ubo.model = glm::rotate(glm::mat4(1.0f), 0 * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view  = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
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

        ubo.model = glm::rotate(glm::mat4(1.0f), 0 * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj  = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);
        ubo.view = glm::lookAt(position, position + front, glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj[1][1] *= -1;

        return ubo;
    }
} // GCEP