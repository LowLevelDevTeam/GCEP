#include "audio_system.hpp"

// STL
#include <algorithm>
#include <iostream>

// Libs
#include <glm/glm.hpp>

namespace gcep
{
    AudioSystem* AudioSystem::s_instance = nullptr;

    AudioSystem::AudioSystem()
        : m_device()
        , m_mixer(AudioBuffer::GCEP_CHANNELS)
    {
        if (!m_device.initialize(AudioBuffer::GCEP_SAMPLE_RATE, AudioBuffer::GCEP_CHANNELS))
        {
            std::cerr << "[AudioSystem] Failed to initialize audio device\n";
        }

        m_device.setMixer(&m_mixer);
        m_mixer.setListener(&m_listener);
    }

    AudioSystem::~AudioSystem()
    {
        stopAll();
        m_device.shutdown();
        delete s_instance;
    }

    std::shared_ptr<AudioSource> AudioSystem::loadAudio(const std::string& filepath)
    {
        auto it = m_buffers.find(filepath);

        std::shared_ptr<AudioBuffer> buffer;

        if (it != m_buffers.end())
        {
            buffer = it->second;
        }
        else
        {
            buffer = std::make_shared<AudioBuffer>();
            if (!buffer->loadFromFile(filepath))
                return nullptr;

            m_buffers.emplace(filepath, buffer);
        }

        auto source = std::make_shared<AudioSource>();
        source->setBuffer(buffer);

        m_sources.push_back(source);
        return source;
    }

    void AudioSystem::update()
    {
        buildAudioVoices();
        m_mixer.submitVoices(m_voiceBuffer.data(), m_voiceBuffer.size());
    }

    void AudioSystem::buildAudioVoices()
    {
        m_voiceBuffer.clear();
        m_voiceBuffer.reserve(m_sources.size());

        for (const auto& source : m_sources)
        {
            if (!source->isPlaying() || !source->getBuffer())
                continue;

            AudioVoice voice{};
            voice.buffer = source->getBuffer().get();
            voice.playHead = source->getFrameCursor();
            voice.isActive = true;
            voice.isLooping = source->isLooping();
            voice.pitch = source->getPitch();
            voice.volume = source->getVolume();

            voice.panLeft = 1.0f;
            voice.panRight = 1.0f;
            voice.attenuation = 1.0f;

            if (source->isSpatialized())
            {
                const glm::vec3 delta =
                    source->getPosition() - m_listener.getPosition();

                const float distance = glm::length(delta);
                const float clamped =
                    std::clamp(distance, source->getMinDistance(), source->getMaxDistance());

                voice.attenuation = 1.0f / (1.0f + clamped);

                const float pan =
                    std::clamp(delta.x / (distance + 0.0001f), -1.0f, 1.0f);

                voice.panLeft = 0.5f * (1.0f - pan);
                voice.panRight = 0.5f * (1.0f + pan);
            }

            m_voiceBuffer.push_back(voice);
        }
    }

    void AudioSystem::stopAll()
    {
        for (auto& source : m_sources)
        {
            source->stop();
        }

        m_sources.clear();
        m_voiceBuffer.clear();
    }

    AudioSystem* AudioSystem::getInstance()
    {
        if (!s_instance)
        {
            s_instance = new AudioSystem();
        }

        return s_instance;
    }
} // gcep
