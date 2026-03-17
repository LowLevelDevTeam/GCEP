#pragma once

// Internals
#include <ECS/headers/component_registry.hpp>

typedef void* ScriptInstance;

namespace gcep::ECS
{
    struct ScriptComponent
    {
        std::string    scriptName;
        ScriptInstance instance          = nullptr; ///< DLL symbol, do not store, can change.
        bool           started           = false;
        int            loadedScriptIndex = -1;
        ECS::EntityID  entityId          = ECS::INVALID_VALUE;
        static inline bool _gcep_registered =
            gcep::ECS::ComponentRegistry::instance().reg<ScriptComponent>();
    };
} // namespace gcep::ECS
