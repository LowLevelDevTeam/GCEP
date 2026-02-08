#include <iostream>
#include "Window/Window.hpp"

#include "../Engine/Core/Audio/AudioSystem.h"
#include <thread>

int main() {
    std::cout << "Hello, World!" << std::endl;
    Window& window = Window::getInstance();
    window.initWindow();

    /* Audio tests
    gce::AudioDevice audioDevice;
    if (!audioDevice.initialize(gce::AudioBuffer::GCE_SAMPLE_RATE, gce::AudioBuffer::GCE_CHANNELS))
    {
        std::cerr << "Failed to initialize audio device.\n";
        return -1;
    }

    gce::AudioSystem audioSystem(&audioDevice);

    auto music = audioSystem.loadAudio("put_a_music_file_here.wav");
    music->setLooping(true);
    music->play();

    std::cout << "Playing music for 10 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    music->pause();

    std::cout << "Pausing music for 3 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    music->play();

    std::cout << "Resuming music for 10 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    music->stop();
    audioDevice.shutdown();
     */

    return 0;
}