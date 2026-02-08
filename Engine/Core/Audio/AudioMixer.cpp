#include "AudioMixer.h"

#include "AudioDevice.h"
#include "AudioSource.h"

#include <algorithm>

namespace gce
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

            auto buffer = source->getBuffer();
            uint64_t playhead = source->getPlayHeadPosition();
            uint64_t availableFrames = buffer->getFrameCount() - playhead;
            uint32_t framesToMix = static_cast<uint32_t>(
                std::min<uint64_t>(frameCount, availableFrames));

            for (uint32_t i = 0; i < framesToMix; ++i)
            {
                for (uint32_t ch = 0; ch < channels; ++ch)
                {
                    float sample =
                        buffer->getSample(playhead + i, ch) * source->getVolume();

                    output[i * channels + ch] += sample;
                }
            }

            source->advancePlayHead(framesToMix);
        }

        // Final clamp
        for (uint32_t i = 0; i < frameCount * channels; ++i)
        {
            output[i] = std::clamp(output[i], -1.0f, 1.0f);
        }
    }
} // gce
