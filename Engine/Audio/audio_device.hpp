#pragma once

// STL
#include <memory>

// Libs
#include <miniaudio.h>

namespace gcep
{
    class AudioMixer;
    class AudioSource;

    /**
    * @brief Audio device wrapper around miniaudio.
    *
    * AudioDevice is responsible for:
    * - Initializing and shutting down the audio hardware
    * - Managing the miniaudio playback device
    * - Forwarding audio callbacks to the AudioMixer
    *
    * This class does not perform any mixing by itself.
    * All audio mixing logic is delegated to AudioMixer.
    *
    * There should generally be only one AudioDevice instance
    * for the entire engine.
    */
    class AudioDevice
    {
    public:
        /**
        * @brief Constructs an uninitialized audio device.
        *
        * The device must be explicitly initialized by calling initialize()
        * before any sound can be played.
        */
        AudioDevice();

        /**
        * @brief Destroys the audio device and releases all resources.
        *
        * If the device is still initialized, shutdown() is automatically called.
        */
        ~AudioDevice();

        /**
        * @brief Initializes the audio playback device.
        *
        * This function creates and starts a miniaudio playback device.
        * The audio callback will be invoked on a separate audio thread.
        *
        * @param sampleRate Desired sample rate (0 lets miniaudio choose a default)
        * @param channels   Number of output channels (e.g. 2 for stereo)
        *
        * @return true if initialization succeeded, false otherwise
        */
        [[nodiscard("You should always check if the audio device initialized correctly.")]] bool initialize(uint32_t sampleRate = 0, uint32_t channels = 0);

        /**
        * @brief Shuts down the audio device.
        *
        * Stops playback and releases all miniaudio resources.
        * Safe to call multiple times.
        */
        void shutdown();

        /**
        * @brief Assigns an AudioMixer to this device.
        *
        * The mixer will be used inside the audio callback to generate
        * the final mixed audio buffer sent to the sound card.
        *
        * @param mixer Pointer to the AudioMixer instance (non-owning)
        */
        void setMixer(AudioMixer* mixer);

        /**
        * @brief Returns the number of output channels.
        *
        * This value is defined during device initialization and
        * is used by the AudioMixer to correctly interleave samples.
        *
        * @return Number of output channels
        */
        [[nodiscard]] uint32_t getChannels() const;

    private:

        /**
        * @brief Miniaudio playback callback.
        *
        * This function is called by miniaudio on the audio thread whenever
        * it needs more audio data.
        *
        * The callback forwards the output buffer to the AudioMixer,
        * which fills it with mixed audio samples.
        *
        * @param pDevice     Pointer to the miniaudio device
        * @param pOutput     Output buffer to be filled (float samples)
        * @param pInput      Unused (playback-only device)
        * @param frameCount  Number of frames requested
        */
		static void audioCallback(const ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

    private:
        /** Native miniaudio device handle */
        ma_device m_device{};

        /** Audio mixer used to generate the final output buffer (non-owning) */
        AudioMixer* m_mixer = nullptr;

        /** Number of output channels configured for this device */
        uint32_t m_channels = 0;
    };
} // gcep
