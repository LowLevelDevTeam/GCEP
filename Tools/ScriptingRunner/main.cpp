#include <Engine/Core/Scripting/ScriptHost.hpp>
#include <Engine/Core/Scripting/ScriptAPI.hpp>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

int main()
{
    const std::filesystem::path scriptsDir = std::filesystem::current_path() / "Scripts";
    const std::filesystem::path scriptPath = scriptsDir / (std::string("GCEngineScripts") + gcep::scripting::kSharedLibraryExtension);

    gcep::scripting::ScriptHost host(scriptPath);
    std::string error;
    if (!host.load(&error))
    {
        std::cerr << "Failed to load script: " << error << std::endl;
        return 1;
    }

    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    for (int i = 0; i < 120; ++i)
    {
        auto now = clock::now();
        const std::chrono::duration<double> delta = now - last;
        last = now;

        host.update(delta.count());
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}

