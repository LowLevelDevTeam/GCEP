#pragma once

// STL
#include <memory>

// Core
#include "audio_buffer.hpp"
#include "Engine/Core/Maths/vector3.hpp"

namespace gcep
{
    class AudioBuffer;

    class AudioSource
    {
    public:
        AudioSource();
        ~AudioSource();

        void setBuffer(const std::shared_ptr<AudioBuffer>& buffer);

        void play();
        void pause();
        void stop();
        void reset();

        void setLooping(bool isLooping);
        void setPitch(float pitch);
        void setVolume(float volume);

        void setPosition(const Vector3<float>& position);
        void setSpatialized(bool isSpatialized);

        void setMinDistance(float minDistance);
        void setMaxDistance(float maxDistance);

        [[nodiscard("Call play() if you want to play the audio source.")]] bool isPlaying() const;
        [[nodiscard("Call setLooping(bool isLooping) if you want to change the isLooping attribute.")]] bool isLooping() const;
        [[nodiscard("Call setSpatialized(bool isSpatialized) if you want to change the spatialization of this source.")]] bool isSpatialized() const;

        [[nodiscard]] const std::shared_ptr<AudioBuffer>& getBuffer() const;
        [[nodiscard]] double* getFrameCursor() const;
        [[nodiscard]] float getPitch() const;
        [[nodiscard]] float getVolume() const;
        [[nodiscard]] Vector3<float>& getPosition();
        [[nodiscard]] float getMinDistance() const;
        [[nodiscard]] float getMaxDistance() const;

    private:
        std::shared_ptr<AudioBuffer> m_buffer;

        bool m_isPlaying = false;
        bool m_isLooping = false;

        double m_frameCursor = 0;
        float m_pitch = 1.f;
        float m_volume = 1.f;

        Vector3<float> m_position{0.0f, 0.0f, 0.0f};
        bool m_isSpatialized = false;

        float m_minDistance = 1.0f;
        float m_maxDistance = 50.f;

        friend class AudioDevice;
    };
} // gcep
