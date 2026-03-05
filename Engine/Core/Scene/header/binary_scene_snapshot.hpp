#pragma once
#include <ECS/headers/archive.hpp>
#include <Scene/header/scene.hpp>

namespace gcep::SLS
{
    class Scene;
    class binarySnapShot
    {
    public:
        explicit binarySnapShot(Scene& scene) : m_scene(scene) {};

        void serializeAll(SER::FileArchive& archive);
        void deserializeAll(SER::FileArchive& archive);

    private:
        Scene& m_scene;
    };
}

//#include <Scene/detail/binary_scene_snapshot.inl>
