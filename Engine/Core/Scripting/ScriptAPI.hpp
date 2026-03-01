#pragma once

#include <cstddef>

#include <Engine/Core/ECS/headers/registry.hpp>
#include <Engine/Core/RHI/Vulkan/VulkanMesh.hpp>

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
        rhi::vulkan::VulkanMesh* mesh = nullptr;
    };

    struct ScriptPlugin
    {
        void* state = nullptr;
        void (*onLoad)(ScriptContext* context, void** state) = nullptr;
        void (*onUpdate)(ScriptContext* context, void* state) = nullptr;
        void (*onUnload)(ScriptContext* context, void* state) = nullptr;
    };

    using CreateScriptPluginFn = ScriptPlugin* (*)();
    using DestroyScriptPluginFn = void (*)(ScriptPlugin* plugin);

    inline constexpr const char* kCreateSymbol = "GCE_CreateScriptPlugin";
    inline constexpr const char* kDestroySymbol = "GCE_DestroyScriptPlugin";

    #if defined(_WIN32)
    inline constexpr const char* kSharedLibraryExtension = ".dll";
    #elif defined(__APPLE__)
    inline constexpr const char* kSharedLibraryExtension = ".dylib";
    #else
    inline constexpr const char* kSharedLibraryExtension = ".so";
    #endif

    inline rhi::vulkan::VulkanMesh* getMesh(ScriptContext* context)
    {
        return context ? context->mesh : nullptr;
    }

    inline void setMeshPosition(ScriptContext* context, const glm::vec3& position)
    {
        if (auto* mesh = getMesh(context))
        {
            mesh->transform.position = position;
        }
    }

    inline void setMeshRotation(ScriptContext* context, const glm::vec3& rotation)
    {
        if (auto* mesh = getMesh(context))
        {
            mesh->transform.rotation = rotation;
        }
    }

    inline void setMeshScale(ScriptContext* context, const glm::vec3& scale)
    {
        if (auto* mesh = getMesh(context))
        {
            mesh->transform.scale = scale;
        }
    }
}
