//
// Created by cleme on 13/02/2026.
//

#include "camera.hpp"

#include <iostream>
#include <ostream>

namespace gcep
{
    void Camera::moveForward()
    {
        std::cout<<"move forward"<<std::endl;
        m_positionX += 1;
    }
    Camera::Camera(Inputs* inputs)
    {
        inputs->addTrackedKey(GLFW_KEY_W, std::bind(&Camera::moveForward, this));
    }
} // GCEP