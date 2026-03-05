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
            context->log("DWAFAFSTFGEWGSDGDGSDFEGES");
        }
    }

    void onUpdate(gcep::scripting::ScriptContext* context, void* state)
    {
        // execute every tick
        if (auto* tc = gcep::scripting::getTransform(context))
        {
            //tc->position.x += 1.f * static_cast<float>(context->deltaSeconds);
        }
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
GCE_REGISTER_SCRIPT("Script1", g_plugin);

