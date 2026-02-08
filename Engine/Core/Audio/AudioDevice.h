#pragma once

// Libs
#include <miniaudio.h>

// STL
#include <memory>

namespace gce
{
    class AudioMixer;
    class AudioSource;

    class AudioDevice
    {
    public:
        AudioDevice();
        ~AudioDevice();

        [[nodiscard("You should always check if the audio device initialized correctly.")]] bool initialize(uint32_t sampleRate = 0, uint32_t channels = 0);
        void shutdown();

        void setMixer(AudioMixer* mixer);

        [[nodiscard]] uint32_t getChannels() const;

    private:

		static void audioCallback(const ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

    private:
        ma_device m_device{};
        AudioMixer* m_mixer = nullptr;
        uint32_t m_channels = 0;
    };
} // gce