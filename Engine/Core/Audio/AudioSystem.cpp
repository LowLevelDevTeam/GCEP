#include "AudioSystem.h"

// STL
#include <iostream>

namespace gce
{
    AudioSystem::AudioSystem(AudioDevice *device)
        : m_device(device), m_mixer(device, AudioMixer::GCE_BUFFER_FRAMES)
    {
        m_device->setMixer(&m_mixer);
    }

    AudioSystem::~AudioSystem()
    {
         stopAll();
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
} // gce