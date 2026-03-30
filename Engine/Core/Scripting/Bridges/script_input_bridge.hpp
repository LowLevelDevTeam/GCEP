#pragma once

/// @file script_input_bridge.hpp
/// @brief Engine-side factory for the @c ScriptInputAPI vtable.

// Internals
#include <Scripting/API/script_interface.hpp>

namespace gcep
{
    /// @brief Returns a pointer to the singleton @c ScriptInputAPI vtable.
    ///
    /// The returned pointer is valid for the lifetime of the engine process and
    /// must not be freed by the caller. It is embedded into a @c ScriptContext
    /// by @c ScriptHotReloadManager::makeContext() and subsequently used by the
    /// @c Input helper class inside scripts to poll keyboard and mouse state
    /// without including any engine headers.
    ///
    /// @return Non-owning pointer to the engine's @c ScriptInputAPI function table.
    const ScriptInputAPI* getScriptInputAPI();

} // namespace gcep
