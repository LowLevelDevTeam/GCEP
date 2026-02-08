#pragma once

// Core
#include "AudioBuffer.h"

// STL
#include <memory>
#include <atomic>

namespace gce
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

        void advancePlayHead(uint64_t frames);

        void setLooping(bool isLooping);
        void setVolume(float volume);

        [[nodiscard("Call play() if you want to play the audio source.")]] bool isPlaying() const;
        [[nodiscard("Call setLooping(bool isLooping) if you want to change the isLooping attribute.")]] bool isLooping() const;

        [[nodiscard]] const std::shared_ptr<AudioBuffer>& getBuffer() const;
        [[nodiscard]] uint64_t getPlayHeadPosition() const;
        [[nodiscard]] float getVolume() const;

    private:
        std::shared_ptr<AudioBuffer> m_buffer;

        std::atomic<bool> m_isPlaying{ false };
        bool m_isLooping = false;

        uint64_t m_frameCursor = 0;

        float m_volume = 1.f;

        friend class AudioDevice;
    };
} // gce
