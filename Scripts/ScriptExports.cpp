#include <Engine/Core/Scripting/ScriptAPI.hpp>
#include <cstring>

// ── Single export point for the entire DLL ─────────────────────────────
// All script files auto-register via GCE_REGISTER_SCRIPT().
// These three functions let the engine query the registry at runtime.

GCE_SCRIPT_API std::size_t GCE_GetScriptCount()
{
    return gcep::scripting::getScriptRegistry().size();
}

GCE_SCRIPT_API gcep::scripting::ScriptRegistryEntry* GCE_GetScriptEntry(std::size_t index)
{
    auto& reg = gcep::scripting::getScriptRegistry();
    if (index >= reg.size())
    {
        return nullptr;
    }
    return &reg[index];
}

GCE_SCRIPT_API gcep::scripting::ScriptRegistryEntry* GCE_GetScriptEntryByName(const char* name)
{
    if (!name)
    {
        return nullptr;
    }
    for (auto& entry : gcep::scripting::getScriptRegistry())
    {
        if (entry.name && std::strcmp(entry.name, name) == 0)
        {
            return &entry;
        }
    }
    return nullptr;
}

