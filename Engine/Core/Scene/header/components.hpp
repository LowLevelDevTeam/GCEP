#pragma once
#include <ECS/headers/component_registry.hpp>
#include "Maths/vector3.hpp"


namespace gcep::SLS
{
    struct Transform                         // ← plus de : gcep::ECS::Component<Transform>
    {
        Vector3<float> position = { 0.0f, 0.0f, 0.0f };
        Vector3<float> rotation = { 0.0f, 0.0f, 0.0f };
        Vector3<float> scale    = { 1.0f, 1.0f, 1.0f };
        static inline bool _gcep_registered =
        ECS::ComponentRegistry::instance().reg<Transform>();
    };
    struct WorldTransform
    {
        Vector3<float> position = { 0.0f, 0.0f, 0.0f };
        Vector3<float> rotation = { 0.0f, 0.0f, 0.0f };
        Vector3<float> scale    = { 1.0f, 1.0f, 1.0f };
        static inline bool _gcep_registered =
        ECS::ComponentRegistry::instance().reg<WorldTransform>();
    };

    struct Tag
    {
        std::string name;
        static inline bool _gcep_registered =
        ECS::ComponentRegistry::instance().reg<Tag>();
    };

    struct HierarchyComponent
    {
        ECS::EntityID parent      = ECS::INVALID_VALUE;
        ECS::EntityID firstChild  = ECS::INVALID_VALUE;
        ECS::EntityID nextSibling = ECS::INVALID_VALUE;
        ECS::EntityID prevSibling = ECS::INVALID_VALUE;

        static inline bool _gcep_registered =
            ECS::ComponentRegistry::instance().reg<HierarchyComponent>();
    };
}