#include "audio_source.hpp"

namespace gcep
{
	AudioSource::AudioSource() = default;
	AudioSource::~AudioSource() = default;

	void AudioSource::setBuffer(const std::shared_ptr<AudioBuffer>& buffer)
	{
		m_buffer = buffer;
	}

	void AudioSource::play()
	{
		if (!m_buffer)
			return;

		m_isPlaying = true;
	}

	void AudioSource::pause()
	{
		m_isPlaying = false;
	}

	void AudioSource::stop()
	{
		m_isPlaying = false;
	}

	void AudioSource::setLooping(bool isLooping)
	{
		m_isLooping = isLooping;
	}

	void AudioSource::setPitch(float pitch)
	{
		m_pitch = pitch > 0.0f ? pitch : 0.01f;
	}

	void AudioSource::setVolume(float volume)
	{
		m_volume = volume;
	}

	void AudioSource::setPosition(const Vector3<float>& position)
	{
		m_position = position;
	}

	void AudioSource::setSpatialized(bool isSpatialized)
	{
		m_isSpatialized = isSpatialized;
	}

	void AudioSource::setMinDistance(float minDistance)
	{
		m_minDistance = minDistance;
	}

	void AudioSource::setMaxDistance(float maxDistance)
	{
		m_maxDistance = maxDistance;
	}

	bool AudioSource::isPlaying() const
	{
		return m_isPlaying;
	}

	bool AudioSource::isLooping() const
	{
		return m_isLooping;
	}

	bool AudioSource::isSpatialized() const
	{
		return m_isSpatialized;
	}

	const std::shared_ptr<AudioBuffer>& AudioSource::getBuffer() const
	{
		return m_buffer;
	}

	double* AudioSource::getFrameCursor() const
	{
		return (double*)&m_frameCursor;
	}

	float AudioSource::getPitch() const
	{
		return m_pitch;
	}

	float AudioSource::getVolume() const
	{
		return m_volume;
	}

	Vector3<float>& AudioSource::getPosition()
	{
		return m_position;
	}

	float AudioSource::getMinDistance() const
	{
		return m_minDistance;
	}

	float AudioSource::getMaxDistance() const
	{
		return m_maxDistance;
	}
} // gcep
