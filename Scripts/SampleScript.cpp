/*#include <Engine/Core/Scripting/ScriptAPI.hpp>

#include <string>
#include <ECS/Components/physics_component.hpp>
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
        static bool foo = true;
        auto* counter = static_cast<CounterState*>(state);
        if (!counter)
        {
            return;
        }
        counter->value += 1;
        if (context && context->log)
        {
            /*std::string message = "tick YAY: " + std::to_string(counter->value);
            context->log(message.c_str());
        }

        //gcep::Quaternion quat;
        //gcep::scripting::getMesh(context)->transform.position.x += 0.01f;
        if (foo)
        {
            auto& physicsSystem = context->physicsSystem;
            auto& physicsComp = context->registry->getComponent<gcep::ECS::PhysicsComponent>(0);

            // z = 0 -> cr�pe
            //physicsComp.force = gcep::Vector3<float>{1.0f, 1.0f, 0.0f};
            // modifie la scale
            //physicsComp.force += gcep::Vector3<float>{0.0f, 0.0f, 10.0f};
            //physicsComp.angularImpulse += gcep::Vector3<float>{0.0f, 0.0f, 10.0f};
            //foo = false;
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

// Register this script � DO NOT DELETE
GCE_REGISTER_SCRIPT("SampleScript", g_plugin);
*/