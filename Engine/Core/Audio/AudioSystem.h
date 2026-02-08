#pragma once

// Core
#include "Audio.h"

// STL
#include <memory>
#include <unordered_map>
#include <vector>

namespace gce
{
    class AudioSystem
    {
    public:
        explicit AudioSystem(AudioDevice* device);
        ~AudioSystem();

        std::shared_ptr<AudioSource> loadAudio(const std::string& filepath);

        void stopAll();

    private:
        AudioDevice* m_device;
        AudioMixer m_mixer;

        std::unordered_map<std::string, std::shared_ptr<AudioBuffer>> m_buffers;
        std::vector<std::shared_ptr<AudioSource>> m_sources;
    };
} // gce