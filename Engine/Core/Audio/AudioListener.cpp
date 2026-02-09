#include "AudioListener.h"

namespace gcep
{
    AudioListener::AudioListener()
    {}

    void AudioListener::setPosition(const glm::vec3& position)
    {
        m_position = position;
    }

    void AudioListener::setForward(const glm::vec3& forward)
    {
        m_forward = forward;
    }

    void AudioListener::setUp(const glm::vec3& up)
    {
        m_up = up;
    }

    const glm::vec3& AudioListener::getPosition() const
    {
        return m_position;
    }

    const glm::vec3& AudioListener::getForward() const
    {
        return m_forward;
    }

    const glm::vec3& AudioListener::getUp() const
    {
        return m_up;
    }
} // gcep