#pragma once

// STL
#include <memory>
#include <unordered_map>
#include <vector>

// Core
#include "audio.hpp"

namespace gcep
{
    class AudioDevice;
    class AudioMixer;
    class AudioListener;
    class AudioSource;
    class AudioBuffer;

    class AudioSystem
    {
    public:
        AudioSystem();
        ~AudioSystem();

        std::shared_ptr<AudioSource> loadAudio(const std::string& filepath);

        void update();

        void stopAll();

        [[nodiscard]] static AudioSystem* getInstance();

    private:
        void buildAudioVoices();

    private:
        static AudioSystem* s_instance;

        AudioDevice m_device;
        AudioMixer m_mixer;
        AudioListener m_listener;

        std::unordered_map<std::string, std::shared_ptr<AudioBuffer>> m_buffers;
        std::vector<std::shared_ptr<AudioSource>> m_sources;

        std::vector<AudioVoice> m_voiceBuffer;
    };
} // gcep