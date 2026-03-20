#pragma once

/// @file script_api.hpp
/// @brief Public scripting API.

#include "script_object.hpp"

// DECLARE_SCRIPT(DataType)
#define DECLARE_SCRIPT(DataType)                                                  \
    SCRIPT_API ScriptInstance scriptCreate()                                      \
    {                                                                             \
        return new DataType{};                                                    \
    }                                                                             \
    SCRIPT_API void scriptDestroy(ScriptInstance inst)                            \
    {                                                                             \
        delete static_cast<DataType*>(inst);                                      \
    }                                                                             \
    SCRIPT_API void scriptOnStart(ScriptInstance inst, const ScriptContext* ctx)  \
    {                                                                             \
        static_cast<DataType*>(inst)->onStart(ctx);                               \
    }                                                                             \
    SCRIPT_API void scriptOnUpdate(ScriptInstance inst, const ScriptContext* ctx) \
    {                                                                             \
        static_cast<DataType*>(inst)->onUpdate(ctx);                              \
    }                                                                             \
    SCRIPT_API void scriptOnEnd(ScriptInstance inst, const ScriptContext* ctx)    \
    {                                                                             \
        static_cast<DataType*>(inst)->onEnd(ctx);                                 \
    }
