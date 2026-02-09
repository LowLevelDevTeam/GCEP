#include "AudioMixer.h"

// STL
#include <algorithm>

// Libs
#include <glm/glm.hpp>

// Core
#include "AudioDevice.h"
#include "AudioListener.h"
#include "AudioSource.h"

namespace gcep
{
    AudioMixer::AudioMixer(AudioDevice* device, size_t bufferFrames)
        : m_device(device), m_bufferFrames(bufferFrames)
    {
        m_mixBuffer.resize(bufferFrames * 2, 0.0f);
    }

    void AudioMixer::registerSource(const std::shared_ptr<AudioSource>& source)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sources.push_back(source);
    }

    void AudioMixer::unregisterSource(const std::shared_ptr<AudioSource>& source)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sources.erase(std::remove(m_sources.begin(), m_sources.end(), source), m_sources.end());
    }

    void AudioMixer::mix(float* output, uint32_t frameCount)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        const uint32_t channels = m_device->getChannels();
        std::fill(output, output + frameCount * channels, 0.0f);

        for (auto& source : m_sources)
        {
            if (!source->isPlaying() || !source->getBuffer())
                continue;

            const auto& buffer = source->getBuffer();
            const uint64_t bufferFrames = buffer->getFrameCount();

            double playHead = source->getPlayHeadPosition();
            const double pitch = source->getPitch();
            const float volume = source->getVolume();

            // --- Spatial factors ---
            float attenuation = computeAttenuation(*source);

            float panL = 1.0f;
            float panR = 1.0f;
            computeStereoPan(*source, panL, panR);

            for (uint32_t i = 0; i < frameCount; ++i)
            {
                uint64_t frameA = static_cast<uint64_t>(playHead);
                uint64_t frameB = frameA + 1;

                if (frameA >= bufferFrames)
                    break;

                const float t = static_cast<float>(playHead - static_cast<double>(frameA));

                // Interpolated mono/stereo sample
                float sampleL = 0.0f;
                float sampleR = 0.0f;

                {
                    float aL = buffer->getSample(frameA, 0);
                    float bL = (frameB < bufferFrames) ? buffer->getSample(frameB, 0) : 0.0f;
                    sampleL = (1.0f - t) * aL + t * bL;

                    float aR = buffer->getSample(frameA, 1);
                    float bR = (frameB < bufferFrames) ? buffer->getSample(frameB, 1) : 0.0f;
                    sampleR = (1.0f - t) * aR + t * bR;
                }

                // Apply volume + attenuation + pan
                output[i * 2 + 0] += sampleL * volume * attenuation * panL;
                output[i * 2 + 1] += sampleR * volume * attenuation * panR;

                playHead += pitch;

                if (!source->isLooping() && playHead >= bufferFrames)
                {
                    source->stop();
                    break;
                }

                if (source->isLooping() && playHead >= bufferFrames)
                {
                    playHead = std::fmod(playHead, static_cast<double>(bufferFrames));
                }
            }

            source->setPlayHeadPosition(playHead);
        }

        // Final clamp
        for (uint32_t i = 0; i < frameCount * channels; ++i)
        {
            output[i] = std::clamp(output[i], -1.0f, 1.0f);
        }
    }

    void AudioMixer::setListener(AudioListener *listener)
    {
        m_listener = listener;
    }

    float AudioMixer::computeAttenuation(const AudioSource& source) const
    {
        if (!m_listener || !source.isSpatialized())
            return 1.0f;

        const glm::vec3 delta = source.getPosition() - m_listener->getPosition();
        const float distance = glm::length(delta);

        // Simple physically-plausible falloff
        return 1.0f / (1.0f + distance);
    }

    void AudioMixer::computeStereoPan(const AudioSource& source, float &outLeft, float &outRight) const
    {
        if (!m_listener || !source.isSpatialized())
        {
            outLeft = 1.0f;
            outRight = 1.0f;
            return;
        }

        const glm::vec3 delta = source.getPosition() - m_listener->getPosition();
        const float distance = glm::length(delta) + 0.0001f;

        const float pan = std::clamp(delta.x / distance, -1.0f, 1.0f);

        outLeft  = 0.5f * (1.0f - pan);
        outRight = 0.5f * (1.0f + pan);
    }
} // gcep
