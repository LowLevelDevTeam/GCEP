#pragma once

//STL
#include <string>
#include <vector>

namespace gce
{
    class AudioBuffer
    {
    public:
        AudioBuffer();
        ~AudioBuffer();

        bool loadFromFile(const std::string& fileName);
        void unload();

        [[nodiscard]] float const* getData() const;
        [[nodiscard]] int getSampleCount() const;
        [[nodiscard]] int getChannels() const;
        [[nodiscard]] int getSampleRate() const;

    private:
        void clearBuffer();

    private:
        std::vector<float> m_samples;
        int m_sampleCount;
        int m_channels;
        int m_sampleRate;

    };

} // gce
