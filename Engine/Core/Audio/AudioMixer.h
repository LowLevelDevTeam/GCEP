#pragma once

// STL
#include <memory>
#include <mutex>
#include <vector>

// Libs
#include <glm/vec3.hpp>

namespace gcep
{
    class AudioDevice;
    class AudioListener;
    class AudioSource;

    /**
    * @brief Central audio mixing unit.
    *
    * The AudioMixer is responsible for combining all active AudioSource
    * instances into a single interleaved output buffer. It performs:
    * - Sample interpolation
    * - Pitch adjustment
    * - Volume control
    * - Distance attenuation
    * - Stereo panning
    *
    * This class is called from the audio thread via AudioDevice and must
    * therefore be real-time safe.
    */
    class AudioMixer
    {
    public:
        /**
        * @brief Constructs an AudioMixer bound to an audio device.
        * @param device Audio output device.
        * @param bufferFrames Maximum number of frames processed per callback.
        */
        AudioMixer(AudioDevice* device, size_t bufferFrames);

        /**
        * @brief Registers an audio source for mixing.
        * @param source Audio source to register.
        *
        * Thread-safe.
        */
        void registerSource(const std::shared_ptr<AudioSource>& source);

        /**
        * @brief Unregisters an audio source.
        * @param source Audio source to unregister.
        *
        * Thread-safe.
        */
        void unregisterSource(const std::shared_ptr<AudioSource>& source);

        /**
        * @brief Mixes all active audio sources into the output buffer.
        * @param output Interleaved floating-point output buffer.
        * @param frameCount Number of frames to generate.
        *
        * This function is called from the audio thread.
        */
        void mix(float* output, uint32_t frameCount);

        /**
        * @brief Sets the active audio listener used for spatialization.
        * @param listener Audio listener.
        */
        void setListener(AudioListener* listener);

    public:
        static constexpr size_t GCEP_BUFFER_FRAMES = 4096;

    private:

        /**
        * @brief Computes distance attenuation for a spatialized source.
        */
        [[nodiscard]] float computeAttenuation(const AudioSource& source) const;

        /**
        * @brief Computes stereo panning coefficients.
        */
        void computeStereoPan(const AudioSource& source, float &outLeft, float &outRight) const;

    private:
        AudioDevice* m_device = nullptr;
        AudioListener* m_listener = nullptr;

        std::vector<std::shared_ptr<AudioSource>> m_sources;
        std::vector<float> m_mixBuffer;
        size_t m_bufferFrames = 0;

        std::mutex m_mutex;
    };
} // gcep