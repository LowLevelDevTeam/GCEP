//
// Created by nbakkali on 06/02/2026.
//

#include "AudioSource.h"

namespace gce
{
#pragma region Constructors/Destructor
    AudioSource::AudioSource()
    {
    }

    AudioSource::~AudioSource()
    {
    }
#pragma endregion

#pragma region Public Methods
    void AudioSource::setBuffer(std::shared_ptr<AudioBuffer> audioBuffer)
    {
    }

    void AudioSource::play()
    {
    }

    void AudioSource::pause()
    {
    }

    void AudioSource::stop()
    {
    }

    void AudioSource::setLooping(bool isLooping)
    {
    }

    bool AudioSource::isLooping() const
    {
    }
#pragma endregion

#pragma region Private Methods
    void AudioSource::resetPlayback()
    {
    }
#pragma endregion
} // gce