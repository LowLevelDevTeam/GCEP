#pragma once

/// @file script_api.hpp
/// @brief Public scripting API — the only header a user script needs to include.

#include "script_interface.hpp"

// DECLARE_SCRIPT(DataType)
#define DECLARE_SCRIPT(DataType)                                                                                      \
    SCRIPT_API ScriptInstance scriptCreate()                                                                          \
    {                                                                                                                 \
        return new DataType{};                                                                                        \
    }                                                                                                                 \
    SCRIPT_API void scriptDestroy(ScriptInstance inst)                                                                \
    {                                                                                                                 \
        delete static_cast<DataType*>(inst);                                                                          \
    }                                                                                                                 \
    SCRIPT_API void scriptOnStart(ScriptInstance inst, const ScriptContext* ctx)                                      \
    {                                                                                                                 \
        static_cast<DataType*>(inst)->onStart(ctx);                                                                   \
    }                                                                                                                 \
    SCRIPT_API void scriptOnUpdate(ScriptInstance inst, const ScriptContext* ctx)                                     \
    {                                                                                                                 \
        static_cast<DataType*>(inst)->onUpdate(ctx);                                                                  \
    }                                                                                                                 \
    SCRIPT_API void scriptOnEnd(ScriptInstance inst, const ScriptContext* ctx)                                        \
    {                                                                                                                 \
        static_cast<DataType*>(inst)->onEnd(ctx);                                                                     \
    }                                                                                                                 \
    SCRIPT_API void scriptOnCollisionEnter(ScriptInstance inst, const ScriptContext* ctx, const CollisionInfo* info)  \
    {                                                                                                                 \
        static_cast<DataType*>(inst)->onCollisionEnter(ctx, info);                                                    \
    }                                                                                                                 \
    SCRIPT_API void scriptOnCollisionExit(ScriptInstance inst, const ScriptContext* ctx, const CollisionInfo* info)   \
    {                                                                                                                 \
        static_cast<DataType*>(inst)->onCollisionExit(ctx, info);                                                     \
    }                                                                                                                 \
    SCRIPT_API void scriptOnTriggerEnter(ScriptInstance inst, const ScriptContext* ctx, const CollisionInfo* info)    \
    {                                                                                                                 \
        static_cast<DataType*>(inst)->onTriggerEnter(ctx, info);                                                      \
    }                                                                                                                 \
    SCRIPT_API void scriptOnTriggerExit(ScriptInstance inst, const ScriptContext* ctx, const CollisionInfo* info)     \
    {                                                                                                                 \
        static_cast<DataType*>(inst)->onTriggerExit(ctx, info);                                                       \
    }
