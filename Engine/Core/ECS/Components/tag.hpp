#pragma once

// Internals
#include <ECS/headers/component_registry.hpp>

namespace gcep::ECS
{
    struct Tag
    {
        std::string name;
        static inline bool _gcep_registered =
        gcep::ECS::ComponentRegistry::instance().reg<Tag>();
    };
} // namespace gcep::ECS
