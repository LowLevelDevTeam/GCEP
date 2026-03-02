#include <Engine/Core/Scripting/ScriptAPI.hpp>

#include <glm/glm.hpp>
#include <string>

namespace
{
    struct CounterState
    {
        int value = 0;
    };

    void onLoad(gcep::scripting::ScriptContext* context, void** state)
    {
        if (!state)
        {
            return;
        }
        *state = new CounterState{};
        if (context && context->log)
        {
            context->log("SampleScript loaded");
        }
    }

    void onUpdate(gcep::scripting::ScriptContext* context, void* state)
    {
        auto* counter = static_cast<CounterState*>(state);
        if (!counter)
        {
            return;
        }
        counter->value += 1;
        if (context && context->log)
        {
            std::string message = "tick YAY: " + std::to_string(counter->value);
            context->log(message.c_str());
        }

        gcep::scripting::getMesh(context)->transform.position.x += 0.01f;

        if (auto* mesh = gcep::scripting::getMesh(context))
        {
            glm::vec3 pos = mesh->transform.position;
            pos.z -= 0.01f;
            gcep::scripting::setMeshPosition(context, pos);
        }
    }

    void onUnload(gcep::scripting::ScriptContext* context, void* state)
    {
        delete static_cast<CounterState*>(state);
        if (context && context->log)
        {
            context->log("SampleScript unloaded");
        }
    }

    gcep::scripting::ScriptPlugin g_plugin{
        nullptr,
        &onLoad,
        &onUpdate,
        &onUnload
    };
}

GCE_SCRIPT_API gcep::scripting::ScriptPlugin* GCE_CreateScriptPlugin()
{
    return &g_plugin;
}

GCE_SCRIPT_API void GCE_DestroyScriptPlugin(gcep::scripting::ScriptPlugin* /*plugin*/)
{
}
