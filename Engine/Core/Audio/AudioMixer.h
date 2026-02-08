#pragma once

// STL
#include <memory>
#include <mutex>
#include <vector>

namespace gce
{
    class AudioDevice;
    class AudioSource;

    class AudioMixer
    {
    public:
        AudioMixer(AudioDevice* device, size_t bufferFrames);

        void registerSource(const std::shared_ptr<AudioSource>& source);
        void unregisterSource(const std::shared_ptr<AudioSource>& source);

        void mix(float* output, uint32_t frameCount);

    public:
        static constexpr size_t GCE_BUFFER_FRAMES = 4096;

    private:
        AudioDevice* m_device;

        std::vector<std::shared_ptr<AudioSource>> m_sources;
        std::vector<float> m_mixBuffer;
        size_t m_bufferFrames;

        std::mutex m_mutex;
    };
} // gce