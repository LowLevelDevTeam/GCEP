#include "AudioBuffer.h"

namespace gce
{
#pragma region Constructors/Destructors
    AudioBuffer::AudioBuffer()
    {
    }

    AudioBuffer::~AudioBuffer()
    {
    }
#pragma endregion

#pragma region Public Methods
    bool AudioBuffer::loadFromFile(const std::string &fileName)
    {
    }

    void AudioBuffer::unload()
    {
    }

    float const *AudioBuffer::getData() const
    {
    }

    int AudioBuffer::getSampleCount() const
    {
    }

    int AudioBuffer::getChannels() const
    {
    }

    int AudioBuffer::getSampleRate() const
    {
    }
#pragma endregion

#pragma region Private Methods
    void AudioBuffer::clearBuffer()
    {
    }
#pragma endregion
} // gce