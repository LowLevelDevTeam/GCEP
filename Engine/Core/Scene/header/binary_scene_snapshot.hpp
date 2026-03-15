#pragma once

// Internals
#include <ECS/headers/archive.hpp>
#include <Log/log.hpp>
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
} // namespace gcep::SLS

// .inl include in scene.hpp
