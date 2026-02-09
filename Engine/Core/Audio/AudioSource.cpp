#include "AudioSource.h"

#include <cmath>

namespace gcep
{
#pragma region Constructors/Destructor
    AudioSource::AudioSource()
        : m_buffer(nullptr),
          m_isPlaying(false),
          m_isLooping(false),
		  m_frameCursor(0)
    {}

    AudioSource::~AudioSource()
    {
        stop();
    }
#pragma endregion

#pragma region Public Methods
    void AudioSource::setBuffer(const std::shared_ptr<AudioBuffer>& buffer)
    {
		m_buffer = buffer;
		m_frameCursor = 0;
    }

    void AudioSource::play()
    {
		if (!m_buffer) return;

		m_isPlaying.store(true, std::memory_order_release);
    }

    void AudioSource::pause()
    {
        m_isPlaying.store(false, std::memory_order_release);
    }

    void AudioSource::stop()
    {
		m_isPlaying.store(false, std::memory_order_release);
		m_frameCursor = 0;
    }

    void AudioSource::reset()
    {
        m_frameCursor = 0;
    }

	void AudioSource::advancePlayHead(double frames)
	{
    	if (m_buffer)
    	{
    		m_frameCursor += frames;

    		if (m_isLooping)
    		{
    			m_frameCursor %= m_buffer->getFrameCount();
    		}
    		else if (m_frameCursor >= m_buffer->getFrameCount())
    		{
    			m_isPlaying = false;
    			m_frameCursor = m_buffer->getFrameCount();
    		}
    	}
    }

    void AudioSource::setLooping(bool isLooping)
    {
		m_isLooping = isLooping;
    }

	void AudioSource::setPitch(float pitch)
    {
    	m_pitch = std::max(0.01f, pitch);
    }

	void AudioSource::setVolume(float volume)
	{
	    m_volume = volume;
    }

    bool AudioSource::isPlaying() const
    {
		return m_isPlaying.load(std::memory_order_acquire);
    }

    bool AudioSource::isLooping() const
    {
		return m_isLooping;
    }

	const std::shared_ptr<AudioBuffer>& AudioSource::getBuffer() const
	{
	    return m_buffer;
    }

	double AudioSource::getPlayHeadPosition() const
	{
	    return m_frameCursor;
    }

	float AudioSource::getPitch() const
    {
    	return m_pitch;
    }

	float AudioSource::getVolume() const
	{
		return m_volume;
	}
#pragma endregion
} // gcep