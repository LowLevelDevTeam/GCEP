#pragma once

// Core
#include "AudioBuffer.h"

// STL
#include <memory>

namespace gce
{
    class AudioSource
    {
    public:
        AudioSource();
        ~AudioSource();

        void setBuffer(std::shared_ptr<AudioBuffer> audioBuffer);

        void play();
        void pause();
        void stop();
        void setLooping(bool isLooping);

        [[nodiscard]] bool isLooping() const;

    private:
        void resetPlayback();

    private:
        std::shared_ptr<AudioBuffer> m_buffer;

        bool m_isPlaying;
        bool m_isLooping;
        float m_playbackPosition;
    };
} // gce
