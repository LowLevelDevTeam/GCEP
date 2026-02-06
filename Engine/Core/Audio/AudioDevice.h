#pragma once

// Libs
#include <miniaudio.h>

// STL
#include <memory>

namespace gce
{
    class AudioDevice
    {
    public:
        AudioDevice();
        ~AudioDevice();

        bool initialize(int channels, int sampleRate);
        void shutdown();

        [[nodiscard]] ma_device const* getDeviceHandle() const;

    private:
        ma_device = m_device;
        bool m_isInitialized = false;
    };
} // gce