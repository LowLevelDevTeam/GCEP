#include "AudioDevice.h"

namespace gce
{
#pragma region Constructors/Destructor
    AudioDevice::AudioDevice()
    {
    }

    AudioDevice::~AudioDevice()
    {
    }
#pragma endregion

#pragma region Public Methods
    bool AudioDevice::initialize(int channels, int sampleRate)
    {
    }

    void AudioDevice::shutdown()
    {
    }

    ma_device const *AudioDevice::getDeviceHandle() const
    {
    }
#pragma endregion

} // gce