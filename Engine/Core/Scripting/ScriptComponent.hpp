#pragma once

#include "ScriptHost.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>

namespace gcep::scripting
{
    struct ScriptComponent
    {
        std::shared_ptr<ScriptHost> host;
        std::uint32_t meshId = UINT32_MAX;

        ScriptComponent() = default;

        ScriptComponent(std::filesystem::path libraryPath, std::uint32_t meshIdValue)
            : host(std::make_shared<ScriptHost>(std::move(libraryPath))), meshId(meshIdValue)
        {
        }

        explicit ScriptComponent(std::filesystem::path libraryPath)
            : host(std::make_shared<ScriptHost>(std::move(libraryPath)))
        {
        }
    };
}
