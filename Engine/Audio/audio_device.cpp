#include "audio_device.hpp"

// STL
#include <cstring>

// Core
#include "audio_mixer.hpp"
#include "audio_source.hpp"

namespace gcep
{
#pragma region Constructors/Destructor
    AudioDevice::AudioDevice() = default;

    AudioDevice::~AudioDevice()
    {
		shutdown();
    }
#pragma endregion

#pragma region Public Methods
    bool AudioDevice::initialize(const uint32_t sampleRate, const uint32_t channels)
    {
		m_channels = channels;

		ma_device_config config = ma_device_config_init(ma_device_type_playback);
		config.playback.format = ma_format_f32;
		config.playback.channels = channels;
		config.sampleRate = sampleRate;
		config.dataCallback = (ma_device_data_proc)audioCallback;
		config.pUserData = this;

		if (ma_device_init(nullptr, &config, &m_device) != MA_SUCCESS) return false;

		return ma_device_start(&m_device) == MA_SUCCESS;
    }

    void AudioDevice::shutdown()
    {
    	if (m_device.state.value != ma_device_state_uninitialized)
    	{
    		ma_device_uninit(&m_device);
    	}
    }

    void AudioDevice::setMixer(AudioMixer* mixer)
	{
	    m_mixer = mixer;
    }

    uint32_t AudioDevice::getChannels() const
	{
	    return m_channels;
    }
#pragma endregion

#pragma region Private Methods
    void AudioDevice::audioCallback(const ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
    {
    	const auto* self = static_cast<AudioDevice*>(pDevice->pUserData);
    	const auto out = static_cast<float*>(pOutput);

    	if (self && self->m_mixer)
    	{
    		self->m_mixer->mix(out, frameCount);
    	}
    	else
    	{
    		std::memset(out, 0,
				frameCount * pDevice->playback.channels * sizeof(float));
    	}
    }
#pragma endregion
} // gcep
