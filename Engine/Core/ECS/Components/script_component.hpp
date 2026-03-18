#pragma once

#include <ECS/headers/component_registry.hpp>
#include <string>

typedef void* ScriptInstance;

namespace gcep::ECS
{
    struct ScriptComponent
    {
        std::string   scriptName;
        ECS::EntityID entityId = ECS::INVALID_VALUE;

        static inline bool _gcep_registered =
            gcep::ECS::ComponentRegistry::instance().reg<ScriptComponent>();
    };

    struct ScriptRuntimeData
    {
        ScriptInstance instance         = nullptr;
        bool           started          = false;
        int            loadedScriptIndex = -1;

        static inline bool _gcep_registered = false;
    };

} // namespace gcep::ECS