#include "audio_mixer.hpp"

// Internals
#include "audio_buffer.hpp"
#include "audio_voice.hpp"

// STL
#include <algorithm>

namespace gcep
{
    AudioMixer::AudioMixer(uint32_t channels)
        : m_channels(channels)
    {}

    void AudioMixer::submitVoices(const AudioVoice* voices, size_t voiceCount)
    {
        m_voices = voices;
        m_voiceCount = voiceCount;
    }

    void AudioMixer::mix(float* output, uint32_t frameCount)
    {
        std::fill(output, output + frameCount * m_channels, 0.0f);

        for (size_t v = 0; v < m_voiceCount; ++v)
        {
            const AudioVoice& voice = m_voices[v];

            if (!voice.isActive || !voice.buffer)
                continue;

            const AudioBuffer* buffer = voice.buffer;
            const uint64_t bufferFrames = buffer->getFrameCount();

            double& playHead = *voice.playHead;

            for (uint32_t i = 0; i < frameCount; ++i)
            {
                const uint64_t frameA = static_cast<uint64_t>(playHead);
                const uint64_t frameB = frameA + 1;

                if (frameA >= bufferFrames)
                    break;

                const float t = static_cast<float>(playHead - frameA);

                const float aL = buffer->getSample(frameA, 0);
                const float bL = frameB < bufferFrames ? buffer->getSample(frameB, 0) : 0.0f;
                const float aR = buffer->getSample(frameA, 1);
                const float bR = frameB < bufferFrames ? buffer->getSample(frameB, 1) : 0.0f;

                const float sampleL = ((1.0f - t) * aL + t * bL)
                    * voice.volume * voice.attenuation * voice.panLeft;

                const float sampleR = ((1.0f - t) * aR + t * bR)
                    * voice.volume * voice.attenuation * voice.panRight;

                output[i * 2 + 0] += sampleL;
                output[i * 2 + 1] += sampleR;

                playHead += voice.pitch;

                if (!voice.isLooping && playHead >= bufferFrames)
                {
                    playHead = bufferFrames;
                    break;
                }
            }
        }

        for (uint32_t i = 0; i < frameCount * m_channels; ++i)
        {
            output[i] = std::clamp(output[i], -1.0f, 1.0f);
        }
    }

    void AudioMixer::setListener(AudioListener *listener)
    {
        m_listener = listener;
    }

} // gcep
