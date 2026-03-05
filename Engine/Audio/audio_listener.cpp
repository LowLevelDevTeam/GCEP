#include "audio_listener.hpp"

namespace gcep
{
    AudioListener::AudioListener()
    {}

    void AudioListener::setPosition(const Vector3<float>& position)
    {
        m_position = position;
    }

    void AudioListener::setForward(const Vector3<float>& forward)
    {
        m_forward = forward;
    }

    void AudioListener::setUp(const Vector3<float>& up)
    {
        m_up = up;
    }

    Vector3<float>& AudioListener::getPosition()
    {
        return m_position;
    }

    Vector3<float>& AudioListener::getForward()
    {
        return m_forward;
    }

    Vector3<float>& AudioListener::getUp()
    {
        return m_up;
    }
} // gcep