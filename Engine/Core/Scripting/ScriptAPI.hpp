/*#pragma once

#include <cstddef>
#include <vector>

#include <Engine/Core/ECS/headers/registry.hpp>
#include <Engine/Core/PhysicsWrapper/physics_system.hpp>
#include <Engine/RHI/Mesh/Mesh.hpp>

#if defined(_WIN32)
    #if defined(GCE_SCRIPT_BUILD)
        #define GCE_SCRIPT_API extern "C" __declspec(dllexport)
    #else
        #define GCE_SCRIPT_API extern "C"
    #endif
#else
    #define GCE_SCRIPT_API extern "C" __attribute__((visibility("default")))
#endif

namespace gcep::scripting
{
    struct ScriptContext
    {
        double deltaSeconds = 0.0;
        void (*log)(const char* message) = nullptr;
        void* userData = nullptr;
        ECS::Registry* registry = nullptr;
        ECS::EntityID entity = ECS::INVALID_VALUE;
        rhi::vulkan::Mesh* mesh = nullptr;
        gcep::PhysicsSystem* physicsSystem = nullptr;
    };

    struct ScriptPlugin
    {
        void* state = nullptr;
        void (*onLoad)(ScriptContext* context, void** state) = nullptr;
        void (*onUpdate)(ScriptContext* context, void* state) = nullptr;
        void (*onUnload)(ScriptContext* context, void* state) = nullptr;
    };

    // -- Multi-plugin registry ------------------------------------------

    struct ScriptRegistryEntry
    {
        const char* name = nullptr;
        ScriptPlugin plugin{};
    };

    // Global registry (one per DLL, lives in the DLL address space)
    inline std::vector<ScriptRegistryEntry>& getScriptRegistry()
    {
        static std::vector<ScriptRegistryEntry> s_registry;
        return s_registry;
    }

    // RAII helper � its constructor pushes an entry into the registry
    struct ScriptAutoRegister
    {
        ScriptAutoRegister(const char* name, ScriptPlugin plugin)
        {
            getScriptRegistry().push_back({name, plugin});
        }
    };

    // -- DLL export symbols (resolved by ScriptHost) --------------------

    using GetScriptCountFn  = std::size_t (*)();
    using GetScriptEntryFn  = ScriptRegistryEntry* (*)(std::size_t index);
    using GetScriptEntryByNameFn = ScriptRegistryEntry* (*)(const char* name);

    inline constexpr const char* kGetCountSymbol       = "GCE_GetScriptCount";
    inline constexpr const char* kGetEntrySymbol       = "GCE_GetScriptEntry";
    inline constexpr const char* kGetEntryByNameSymbol = "GCE_GetScriptEntryByName";

    #if defined(_WIN32)
    inline constexpr const char* kSharedLibraryExtension = ".dll";
    #elif defined(__APPLE__)
    inline constexpr const char* kSharedLibraryExtension = ".dylib";
    #else
    inline constexpr const char* kSharedLibraryExtension = ".so";
    #endif

    // -- Convenience helpers --------------------------------------------

    inline rhi::vulkan::Mesh* getMesh(ScriptContext* context)
    {
        return context ? context->mesh : nullptr;
    }

    /// Get the authoritative ECS::Transform from the ECS for this script's mesh.
    /// All transform modifications MUST go through the ECS, because the renderer
    /// syncs mesh.transform FROM the ECS each frame.
    inline ECS::Transform* getTransform(ScriptContext* context)
    {
        if (!context || !context->registry || !context->mesh)
            return nullptr;
        if (!context->registry->hasComponent<ECS::Transform>(context->mesh->id))
            return nullptr;
        return &context->registry->getComponent<ECS::Transform>(context->mesh->id);
    }

    inline void setMeshPosition(ScriptContext* context, const Vector3<float>& position)
    {
        if (auto* tc = getTransform(context))
        {
            tc->position = position;
        }
    }

    inline void setMeshRotation(ScriptContext* context, const Quaternion& rotation)
    {
        if (auto* tc = getTransform(context))
        {
            tc->rotation = rotation;
        }
    }

    inline void setMeshScale(ScriptContext* context, const Vector3<float>& scale)
    {
        if (auto* tc = getTransform(context))
        {
            tc->scale = scale;
        }
    }
}

// -- Macro that users put at the end of each script file ----------------
// Usage: GCE_REGISTER_SCRIPT("Script0", g_plugin)
#define GCE_CONCAT_IMPL(a, b) a##b
#define GCE_CONCAT(a, b) GCE_CONCAT_IMPL(a, b)
#define GCE_REGISTER_SCRIPT(scriptName, pluginVar) \
    static gcep::scripting::ScriptAutoRegister GCE_CONCAT(_gce_auto_reg_, __COUNTER__)(scriptName, pluginVar)

*/