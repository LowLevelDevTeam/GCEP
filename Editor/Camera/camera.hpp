#pragma once
#include "Editor/Inputs/Inputs.hpp"
#include <glm/glm.hpp>
#include <imgui.h>
#include "RHI/Vulkan/VulkanRHI.hpp"
#include "Editor/Window/Window.hpp"

namespace gcep
{

    class Camera
    {

        public:

        Camera(Inputs* inputs, Window* window);

        glm::mat4 getViewMatrix();
        glm::mat4 getRotationMatrix();

        rhi::vulkan::UniformBufferObject update(float aspect, float camSpeed);

        public:

        glm::vec3 velocity;
        glm::vec3 position;
        glm::vec3 front = glm::vec3(-1.0f, 0.0f, 0.0f);
        //vertical rotation
        float pitch {0.f};
        //horizontal rotation
        float yaw {-180.f};

        private:

        void moveForward();
        void moveBackward();
        void moveLeft();
        void moveRight();
        void moveUp();
        void moveDown();
        void rotate();
        private:
        rhi::vulkan::UniformBufferObject ubo{};
        float m_camSpeed = 2.0f;

        Window* window;

        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 worldUp = glm::vec3(0.0f, 0.0f, 1.0f);


    };
} // GCEP
