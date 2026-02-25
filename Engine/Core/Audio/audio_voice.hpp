#pragma once

namespace gcep
{
    class AudioBuffer;

    struct AudioVoice
    {
        const AudioBuffer* buffer = nullptr;

        double* playHead = nullptr;
        double pitch = 1.0;

        float volume = 1.0f;
        float panLeft = 1.0f;
        float panRight = 1.0f;
        float attenuation = 1.0f;

        bool isLooping = false;
        bool isActive = false;
    };
} // gcep
