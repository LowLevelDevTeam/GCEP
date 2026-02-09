#include <iostream>
#include "Window/Window.hpp"

#include "../Engine/Core/Audio/AudioSystem.h"
#include <thread>

int main() {
    std::cout << "Hello, World!" << std::endl;
    Window& window = Window::getInstance();
    window.initWindow();

    gcep::AudioSystem* audioSystem = gcep::AudioSystem::getInstance();
    std::shared_ptr<gcep::AudioSource> music = audioSystem->loadAudio("jazz.mp3");

    music->setLooping(true);
    music->setPitch(0.5f);
    music->play();
    std::cout << "Playing music for 10 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    music->pause();
    std::cout << "Pausing music for 3 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    music->setPitch(2.0f);
    music->play();;
    std::cout << "Resuming music for 10 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(360));
    music->stop();

    return 0;
}