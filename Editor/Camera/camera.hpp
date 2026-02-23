#pragma once
#include "Editor/Inputs/Inputs.hpp"
#include <glm/glm.hpp>
#include <imgui.h>
#include "RHI/Vulkan/VulkanRHI.hpp"

namespace gcep
{

    class Camera
    {

        public:

        Camera(Inputs* inputs);

        glm::mat4 getViewMatrix();
        glm::mat4 getRotationMatrix();

        rhi::vulkan::UniformBufferObject update(float aspect, float camSpeed);

        public:

        glm::vec3 velocity;
        glm::vec3 position;
        //vertical rotation
        float pitch {0.f};
        //horizontal rotation
        float yaw {0.f};

        private:

        void moveForward();
        void moveBackward();
        void moveLeft();
        void moveRight();
        void moveUp();
        void moveDown();
        void rotateUp();
        void rotateDown();
        void rotateLeft();
        void rotateRight();
        private:
        rhi::vulkan::UniformBufferObject ubo{};
        float m_camSpeed;


        glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 worldUp = glm::vec3(0.0f, 0.0f, 1.0f);


    };
} // GCEP
