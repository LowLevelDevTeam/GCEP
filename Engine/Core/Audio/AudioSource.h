#pragma once

// STL
#include <memory>
#include <atomic>

// Core
#include "AudioBuffer.h"

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

        void advancePlayHead(double frames);

        void setLooping(bool isLooping);
        void setPitch(float pitch);
        void setVolume(float volume);

        [[nodiscard("Call play() if you want to play the audio source.")]] bool isPlaying() const;
        [[nodiscard("Call setLooping(bool isLooping) if you want to change the isLooping attribute.")]] bool isLooping() const;

        [[nodiscard]] const std::shared_ptr<AudioBuffer>& getBuffer() const;
        [[nodiscard]] double getPlayHeadPosition() const;
        [[nodiscard]] float getPitch() const;
        [[nodiscard]] float getVolume() const;

    private:
        std::shared_ptr<AudioBuffer> m_buffer;

        std::atomic<bool> m_isPlaying{ false };
        bool m_isLooping = false;

        double m_frameCursor = 0;
        float m_pitch = 1.f;
        float m_volume = 1.f;

        friend class AudioDevice;
    };
} // gcep
