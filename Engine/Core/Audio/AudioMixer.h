#pragma once

// STL
#include <memory>
#include <mutex>
#include <vector>

// Libs
#include <glm/vec3.hpp>

namespace gcep
{
    class AudioDevice;
    class AudioListener;
    class AudioSource;

    class AudioMixer
    {
    public:
        AudioMixer(AudioDevice* device, size_t bufferFrames);

        void registerSource(const std::shared_ptr<AudioSource>& source);
        void unregisterSource(const std::shared_ptr<AudioSource>& source);

        void mix(float* output, uint32_t frameCount);

        void setListener(AudioListener* listener);

    public:
        static constexpr size_t GCEP_BUFFER_FRAMES = 4096;

    private:

        float computeAttenuation(const AudioSource& source) const;

        void computeStereoPan(const AudioSource& source, float &outLeft, float &outRight) const;

    private:
        AudioDevice* m_device = nullptr;
        AudioListener* m_listener = nullptr;

        std::vector<std::shared_ptr<AudioSource>> m_sources;
        std::vector<float> m_mixBuffer;
        size_t m_bufferFrames = 0;

        std::mutex m_mutex;
    };
} // gcep