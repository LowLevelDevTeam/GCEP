#pragma once
/// @file script_ecs_bridge.hpp
/// @brief Engine-internal bridge — do NOT ship to script authors.
///        The only file allowed to include both script_interface.hpp
///        and <ECS/...> headers simultaneously.

// Internals
#include <Scripting/API/script_interface.hpp>

namespace gcep
{
    /// Returns the singleton ScriptECSAPI vtable.
    /// Store the result in ScriptContext::ecs before every script call.
    /// The pointer is valid for the entire lifetime of the engine.
    const ScriptECSAPI*    getScriptECSAPI();

    /// Returns the singleton ScriptCameraAPI vtable.
    /// Store the result in ScriptContext::cameraApi before every script call.
    /// The pointer is valid for the entire lifetime of the engine.
    const ScriptCameraAPI* getScriptCameraAPI();

} // namespace gcep