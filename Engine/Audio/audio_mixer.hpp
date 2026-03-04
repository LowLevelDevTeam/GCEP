#pragma once

// STL
#include <cstdint>
#include <vector>

namespace gcep
{
    class AudioListener;
    struct AudioVoice;

    class AudioMixer
    {
    public:
        explicit AudioMixer(uint32_t channels);

        void submitVoices(const AudioVoice* voices, size_t voiceCount);

        void mix(float* output, uint32_t frameCount);

        void setListener(AudioListener* listener);

    private:
        AudioListener* m_listener = nullptr;
        uint32_t m_channels = 0;

        const AudioVoice* m_voices = nullptr;
        size_t m_voiceCount = 0;
    };
} // gcep