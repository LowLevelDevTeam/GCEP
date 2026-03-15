#include "audio_buffer.hpp"

// Externals
#include <miniaudio.h>

// STL
#include <filesystem>
#include <iostream>

namespace gcep
{
#pragma region Constructors/Destructors
	AudioBuffer::AudioBuffer() = default;

	AudioBuffer::AudioBuffer(uint32_t channels, uint32_t frameCount, std::vector<float>&& samples)
	    : m_channels(channels), m_frameCount(frameCount), m_samples(std::move(samples))
	{}

    AudioBuffer::~AudioBuffer()
    {
		unload();
    }
#pragma endregion

#pragma region Public Methods
    bool AudioBuffer::loadFromFile(const std::string &fileName)
    {
        clear();

		ma_decoder decoder;
		ma_decoder_config config = ma_decoder_config_init(ma_format_f32, GCEP_CHANNELS, GCEP_SAMPLE_RATE);

    	std::cout << "[AudioBuffer]: Loading from path: " << std::filesystem::current_path().string() << std::endl;

        if (ma_decoder_init_file(fileName.c_str(), &config, &decoder) != MA_SUCCESS) return false;

		m_channels = decoder.outputChannels;
		m_sampleRate = GCEP_SAMPLE_RATE;

		ma_uint64 totalFrames = 0;
		ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);

		m_samples.resize(static_cast<size_t>(totalFrames * m_channels));
		ma_decoder_read_pcm_frames(&decoder, m_samples.data(), totalFrames, nullptr);

		m_frameCount = totalFrames;

		ma_decoder_uninit(&decoder);
		return true;
    }

    void AudioBuffer::unload()
    {
		clear();
    }

    float const *AudioBuffer::getSamples() const
    {
		return m_samples.data();
    }

    float AudioBuffer::getSample(size_t frame, size_t channel) const
	{
		if (frame >= m_frameCount || channel >= m_channels) return 0.0f;
    	return m_samples[frame * m_channels + channel];
    }

    uint64_t AudioBuffer::getFrameCount() const
    {
		return m_frameCount;
    }

    uint32_t AudioBuffer::getChannels() const
    {
		return m_channels;
    }

    uint32_t AudioBuffer::getSampleRate() const
    {
		return m_sampleRate;
    }
#pragma endregion

#pragma region Private Methods
    void AudioBuffer::clear()
    {
        m_samples.clear();
        m_frameCount = 0;
        m_channels = 0;
        m_sampleRate = 0;
    }
#pragma endregion
} // gcep
