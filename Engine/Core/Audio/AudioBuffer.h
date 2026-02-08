#pragma once

// STL
#include <cstdint>
#include <string>
#include <vector>

namespace gce
{
    class AudioBuffer
    {
    public:
        AudioBuffer();
        explicit AudioBuffer(uint32_t channels, uint32_t frameCount, std::vector<float>&& samples);
        ~AudioBuffer();

        [[nodiscard]] bool loadFromFile(const std::string& fileName);
        void unload();

        [[nodiscard]] const float* getSamples() const;
        [[nodiscard]] float getSample(size_t frame, size_t channel) const;
        [[nodiscard]] uint64_t getFrameCount() const;
        [[nodiscard]] uint32_t getChannels() const;
        [[nodiscard]] uint32_t getSampleRate() const;

    public:

        static constexpr uint32_t GCE_SAMPLE_RATE = 44100;
        static constexpr uint32_t GCE_CHANNELS = 2;

    private:
        void clear();

    private:
        std::vector<float> m_samples;
        uint64_t m_frameCount = 0;
        uint32_t m_channels = 0;
        uint32_t m_sampleRate = 0;
    };

} // gce
