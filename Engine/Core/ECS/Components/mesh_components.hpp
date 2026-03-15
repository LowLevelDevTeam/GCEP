#pragma once

// Internals
#include <ECS/headers/component_registry.hpp>

namespace gcep::ECS
{
    struct MeshComponent
    {
        std::string filePath;
        static inline bool _gcep_registered =
                ComponentRegistry::instance().reg<MeshComponent>();
    };
} // namespace gcep::ECS
