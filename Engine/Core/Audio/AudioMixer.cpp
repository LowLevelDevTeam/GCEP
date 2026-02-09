#include "AudioMixer.h"

#include "AudioDevice.h"
#include "AudioSource.h"

#include <algorithm>

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

        for (uint32_t i = 0; i < frameCount; ++i)
        {
            for (auto& source : m_sources)
            {
                if (!source->isPlaying() || !source->getBuffer())
                    continue;

                const auto& buffer = source->getBuffer();
                double playHead = source->getPlayHeadPosition();
                uint64_t frameA = static_cast<uint64_t>(playHead);
                uint64_t frameB = frameA + 1;

                if (frameA >= buffer->getFrameCount())
                    continue;

                float t = static_cast<float>(playHead - frameA);

                for (uint32_t ch = 0; ch < channels; ++ch)
                {
                    float sampleA = buffer->getSample(frameA, ch);
                    float sampleB = (frameB < buffer->getFrameCount()) ? buffer->getSample(frameB, ch) : 0.0f;

                    output[i * channels + ch] += (1.0f - t) * sampleA + t * sampleB;
                    output[i * channels + ch] *= source->getVolume();
                }

                // Advance playhead by pitch (double)
                source->advancePlayHead(source->getPitch());
            }
        }

        // Clamp final output
        for (uint32_t i = 0; i < frameCount * channels; ++i)
            output[i] = std::clamp(output[i], -1.0f, 1.0f);
    }

} // gcep
