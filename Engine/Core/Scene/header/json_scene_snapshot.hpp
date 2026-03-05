#pragma once
#include <Engine/Core/ECS/headers/json_archive.hpp>
#include <Engine/Core/ECS/headers/component_registry.hpp>
#include <Engine/Core/Scene/header/scene.hpp>

namespace gcep::SLS
{
    class JsonSnapShot
    {
    public:
        explicit JsonSnapShot(Scene& scene) : m_scene(scene) {}

        void serializeAll(const std::string& path);
        void deserializeAll(const std::string& path);

    private:
        Scene& m_scene;
    };
}

#include <Engine/Core/Scene/detail/json_scene_snapshot.inl>