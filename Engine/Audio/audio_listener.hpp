#pragma once

// Internals
#include <Maths/vector3.hpp>

namespace gcep
{
    class AudioListener
    {
    public:
        AudioListener();

        void setPosition(const Vector3<float>& position);
        void setForward(const Vector3<float>& forward);
        void setUp(const Vector3<float>& up);

        [[nodiscard]] Vector3<float>& getPosition();
        [[nodiscard]] Vector3<float>& getForward();
        [[nodiscard]] Vector3<float>& getUp();

    private:
        Vector3<float> m_position{0.0f, 0.0f, 0.0f};
        Vector3<float> m_forward{0.0f, 0.0f, -1.0f};
        Vector3<float> m_up{0.0f, 1.0f, 0.0f};
    };
} // namespace gcep
