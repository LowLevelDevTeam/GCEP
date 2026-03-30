#pragma once

/// @file script_api.hpp
/// @brief Public scripting API — entry-point macro for user script shared libraries.

#include "script_object.hpp"

/// @brief Wires up the five required C-linkage entry points for a script type.
///
/// Place exactly one @c DECLARE_SCRIPT(DataType) at file scope in every script
/// translation unit. The macro exports the following symbols via @c SCRIPT_API:
///
/// | Symbol            | Signature                                         | Role                          |
/// |-------------------|---------------------------------------------------|-------------------------------|
/// | @c scriptCreate   | @c ScriptInstance ()                              | Heap-allocates a @p DataType. |
/// | @c scriptDestroy  | @c void (ScriptInstance)                          | Deletes the instance.         |
/// | @c scriptOnStart  | @c void (ScriptInstance, const ScriptContext*)    | Forwards to @c onStart().     |
/// | @c scriptOnUpdate | @c void (ScriptInstance, const ScriptContext*)    | Forwards to @c onUpdate().    |
/// | @c scriptOnEnd    | @c void (ScriptInstance, const ScriptContext*)    | Forwards to @c onEnd().       |
///
/// @p DataType must be a struct or class that provides @c onStart(), @c onUpdate(),
/// and @c onEnd() methods matching the @c ScriptContext* signature. The macro casts
/// the opaque @c ScriptInstance pointer back to @p DataType* before each dispatch.
///
/// @param DataType  The concrete script struct/class defined in the same file.
///
/// ### Example
/// @code
/// struct MyScript
/// {
///     void onStart (const ScriptContext* ctx) { /* ... */ }
///     void onUpdate(const ScriptContext* ctx) { /* ... */ }
///     void onEnd   (const ScriptContext* ctx) { /* ... */ }
/// };
/// DECLARE_SCRIPT(MyScript)
/// @endcode
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
