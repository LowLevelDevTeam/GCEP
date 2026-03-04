#include <Engine/Core/Scripting/ScriptAPI.hpp>

#include <string>
#include <Engine/Core/PhysicsWrapper/Component/physics_component.hpp>
#include <Engine/Core/PhysicsWrapper/physics_system.hpp>

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
        //static bool foo = true;
        auto* counter = static_cast<CounterState*>(state);
        if (!counter)
        {
            return;
        }
        counter->value += 1;
        if (context && context->log)
        {
            /*std::string message = "tick YAY: " + std::to_string(counter->value);
            context->log(message.c_str());*/
        }


        //gcep::Quaternion quat;
        //gcep::scripting::getMesh(context)->transform.position.x += 0.01f;
        //if (foo)
        //{
            auto& physicsSystem = context->physicsSystem;

            auto& physicsComp = context->registry->getComponent<gcep::PhysicsComponent>(0);


            physicsComp.linearVelocity.x += 100.f;
            physicsSystem->addImpulse(physicsComp.getBodyID(), gcep::Vector3<float>(25000.f, 0.f, 0.f));

            context->log("addForce");
        //}
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
