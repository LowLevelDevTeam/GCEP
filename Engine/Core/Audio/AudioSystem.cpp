#include "AudioSystem.h"

// STL
#include <iostream>

namespace gcep
{
    AudioSystem::AudioSystem()
        : m_device(), m_mixer(&m_device, AudioMixer::GCEP_BUFFER_FRAMES)
    {
        if (!m_device.initialize(gcep::AudioBuffer::GCEP_SAMPLE_RATE, gcep::AudioBuffer::GCEP_CHANNELS))
        {
            std::cerr << "[AudioSystem]: Failed to initialize audio device.\n";
        }

        m_device.setMixer(&m_mixer);

        // Temporary: create an audio listener and giving it to the mixer
        m_mixer.setListener(&m_listener);
    }

    AudioSystem::~AudioSystem()
    {
        stopAll();
        m_device.shutdown();
        delete s_instance;
    }

    std::shared_ptr<AudioSource> AudioSystem::loadAudio(const std::string &filepath)
    {
        // Check if buffer already exists
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
            {
                std::cerr << "[AudioSystem]: Failed to load audio file: " << filepath << "\n";
                return nullptr;
            }

            m_buffers.emplace(filepath, buffer);
        }

		auto source = std::make_shared<AudioSource>();
        source->setBuffer(buffer);

        m_sources.push_back(source);
		// Register the source with the mixer
        m_mixer.registerSource(source);

		return source;
    }

    void AudioSystem::stopAll()
    {
        for (auto& source : m_sources)
        {
            source->stop();
        }

        m_sources.clear();
	}

    AudioSystem* AudioSystem::getInstance()
    {
        if (!s_instance)
        {
            s_instance = new AudioSystem();
        }

        return s_instance;
    }

    AudioSystem* AudioSystem::s_instance = nullptr;
} // gcep