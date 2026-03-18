#pragma once

/// @file script_interface.hpp
/// @brief ABI contract between the engine and script DLLs.
/// Keep this header free of engine includes - it must compile standalone.

#ifdef _WIN32
    #define SCRIPT_API extern "C" __declspec(dllexport)
#else
    #define SCRIPT_API extern "C" __attribute__((visibility("default")))
#endif

typedef void* ScriptInstance;
typedef unsigned int EntityID;

/// @brief Context passed to every script call.
/// Do not store this pointer - it is valid only for the duration of the call.
struct ScriptContext
{
    EntityID entityId;   ///< ECS ID of the entity this script is attached to.
    float    deltaTime;  ///< Seconds since last frame.

    void* registry;  ///< Opaque ECS::Registry*
};