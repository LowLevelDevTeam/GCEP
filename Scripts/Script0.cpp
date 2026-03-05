#include <Engine/Core/Scripting/ScriptAPI.hpp>

#include <string>
#include <Engine/Core/PhysicsWrapper/Component/physics_component.hpp>
#include <Engine/Core/PhysicsWrapper/physics_system.hpp>

namespace
{

    void onLoad(gcep::scripting::ScriptContext* context, void** state)
    {
        if (!state)
        {
            return;
        }

        if (context && context->log)
        {
            context->log("fsebgyusfioisdfhbshfoidslk loaded");
        }
    }

    void onUpdate(gcep::scripting::ScriptContext* context, void* state)
    {
        // execute every tick
    }

    void onUnload(gcep::scripting::ScriptContext* context, void* state)
    {
        if (context && context->log)
        {
            context->log("SampleScript unloaded");
        }
    }

    // Define the plugin with the function pointers DO NOT DELETE
    gcep::scripting::ScriptPlugin g_plugin{
        nullptr,
        &onLoad,
        &onUpdate,
        &onUnload
    };
}

