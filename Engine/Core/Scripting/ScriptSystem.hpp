#pragma once

#include <filesystem>
#include <vector>

#include <ScriptBuildConfig.hpp>
#include <Engine/Core/Scripting/ScriptComponent.hpp>
#include <Engine/Core/ECS/headers/registry.hpp>
#include <Engine/Core/RHI/Vulkan/VulkanMesh.hpp>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
#else
    #include <unistd.h>
#endif


namespace gcep::scripting
{
    class ScriptSystem
    {
    public:

        void init(ECS::Registry* registry, std::vector<rhi::vulkan::VulkanMesh>& meshData);

        void setMeshList(std::vector<rhi::vulkan::VulkanMesh>* meshes);
        void update(ECS::Registry& registry, double deltaSeconds);

    private:
        std::filesystem::path getExecutableDir();
        std::filesystem::path findScriptLibrary();
        std::filesystem::path findScriptSource();
        std::filesystem::path findBuildDir();

#if defined(_WIN32)
        std::string toShortPath(const std::string& path);
#endif

        rhi::vulkan::VulkanMesh* findMesh(std::uint32_t meshId) const;

    private:
        std::vector<rhi::vulkan::VulkanMesh>* m_meshes = nullptr;

        ECS::Registry* m_registry = nullptr;
    };
}
