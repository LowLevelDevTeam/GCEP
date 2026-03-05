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
            context->log("SampleScript loaded");
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


// Exported functions for plugin management DO NOT DELETE
GCE_SCRIPT_API gcep::scripting::ScriptPlugin* GCE_CreateScriptPlugin()
{
    return &g_plugin;
}

GCE_SCRIPT_API void GCE_DestroyScriptPlugin(gcep::scripting::ScriptPlugin* /*plugin*/)
{
}
