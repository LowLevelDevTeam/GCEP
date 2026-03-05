#include <Engine/Core/Scripting/ScriptAPI.hpp>

#include <string>

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

// Register this script — DO NOT DELETE
GCE_REGISTER_SCRIPT("SCRIPT_NAME", g_plugin);

