#pragma once

// Internals
#include <ECS/headers/component_registry.hpp>

namespace gcep::ECS
{
    struct HierarchyComponent
    {
        ECS::EntityID parent      = ECS::INVALID_VALUE;
        ECS::EntityID firstChild  = ECS::INVALID_VALUE;
        ECS::EntityID nextSibling = ECS::INVALID_VALUE;
        ECS::EntityID prevSibling = ECS::INVALID_VALUE;
        static inline bool _gcep_registered =
        gcep::ECS::ComponentRegistry::instance().reg<HierarchyComponent>();
    };
} // namespace gcep::ECS