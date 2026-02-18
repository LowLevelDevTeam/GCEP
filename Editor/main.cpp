#include <iostream>
#include <thread>

#include <glm/glm.hpp>

#include "Window/Window.hpp"
#include "Engine/Core/Audio/AudioSystem.h"

int main() {
    std::cout << "Hello, World!" << std::endl;
    Window& window = Window::getInstance();
    window.initWindow();

    gcep::AudioSystem* audioSystem = gcep::AudioSystem::getInstance();

    std::shared_ptr<gcep::AudioSource> music = audioSystem->loadAudio("mdk.mp3");
    music->setLooping(true);
    music->setVolume(1.0f);
    music->setPitch(1.0f);
    music->setSpatialized(true);
    music->play();

    float angle = 0.0f;
    const float radius = 5.0f;

    while (true) {
        angle += 0.01f;

        glm::vec3 position;
        position.x = std::cos(angle) * radius;
        position.y = 0.0f;
        position.z = std::sin(angle) * radius;

        music->setPosition(position);

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    music->stop();

    return 0;
}