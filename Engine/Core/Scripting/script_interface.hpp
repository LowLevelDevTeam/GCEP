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

/// @brief Collision/trigger event data passed to onCollisionEnter/Exit and onTriggerEnter/Exit.
/// Contact point and normal are only populated for Enter events (not available on Exit).
struct CollisionInfo
{
    EntityID otherEntity; ///< ECS ID of the other entity involved in the collision.
    float    contactX;    ///< World-space contact point X (Enter events only).
    float    contactY;    ///< World-space contact point Y (Enter events only).
    float    contactZ;    ///< World-space contact point Z (Enter events only).
    float    normalX;     ///< Contact normal X, pointing away from otherEntity (Enter events only).
    float    normalY;     ///< Contact normal Y (Enter events only).
    float    normalZ;     ///< Contact normal Z (Enter events only).
};