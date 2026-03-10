/*#pragma once

#include "ScriptHost.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace gcep::scripting
{
    struct ScriptComponent
    {
        std::shared_ptr<ScriptHost> host;
        std::uint32_t meshId = UINT32_MAX;
        std::string scriptName;

        ScriptComponent() = default;

        ScriptComponent(std::shared_ptr<ScriptHost> sharedHost, std::uint32_t meshIdValue, std::string name)
            : host(std::move(sharedHost)), meshId(meshIdValue), scriptName(std::move(name))
        {
        }

        ScriptComponent(std::filesystem::path libraryPath, std::uint32_t meshIdValue)
            : host(std::make_shared<ScriptHost>(libraryPath)), meshId(meshIdValue)
        {
        }

        explicit ScriptComponent(std::filesystem::path libraryPath)
            : host(std::make_shared<ScriptHost>(libraryPath))
        {
        }
    };
}*/
