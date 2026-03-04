#pragma once

// Libs
#include <glm/vec3.hpp>

namespace gcep
{
    class AudioListener
    {
    public:
        AudioListener();

        void setPosition(const glm::vec3& position);
        void setForward(const glm::vec3& forward);
        void setUp(const glm::vec3& up);

        [[nodiscard]] const glm::vec3& getPosition() const;
        [[nodiscard]] const glm::vec3& getForward() const;
        [[nodiscard]] const glm::vec3& getUp() const;

    private:
        glm::vec3 m_position{0.0f};
        glm::vec3 m_forward{0.0f, 0.0f, -1.0f};
        glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    };
} // gcep