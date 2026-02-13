#pragma once
#include "Editor/Inputs/Inputs.hpp"
#include <GLFW/glfw3.h>

namespace gcep
{

    class Camera
    {

        public:

        Camera(Inputs* inputs);



        private:

        void moveForward();
        private:
        int m_positionX = 0;

    };
} // GCEP
