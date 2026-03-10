/*#include <Engine/Core/Scripting/ScriptAPI.hpp>

#include <cmath>
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
            context->log("OUIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII");
        }
    }

    void onUpdate(gcep::scripting::ScriptContext* context, void* state)
    {
        // execute every tick � rotate around X axis at 1 rad/s
        if (auto* tc = gcep::scripting::getTransform(context))
        {
            const float angle = 1.f * static_cast<float>(context->deltaSeconds);
            const float halfAngle = angle * 2.f;

            // Incremental rotation quaternion around X axis
            gcep::Quaternion delta(
                std::cos(halfAngle),   // w
                0.f,                   // x
                0.f,                   // y
                std::sin(halfAngle)    // z
            );

            // Compose: current * delta
            gcep::Quaternion& q = tc->rotation;
            gcep::Quaternion result(
                q.w * delta.w - q.x * delta.x - q.y * delta.y - q.z * delta.z,
                q.w * delta.x + q.x * delta.w + q.y * delta.z - q.z * delta.y,
                q.w * delta.y - q.x * delta.z + q.y * delta.w + q.z * delta.x,
                q.w * delta.z + q.x * delta.y - q.y * delta.x + q.z * delta.w
            );
            result.Normalize();
            q = result;
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

// Register this script � DO NOT DELETE
GCE_REGISTER_SCRIPT("Script0", g_plugin);
*/
