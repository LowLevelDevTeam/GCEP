#pragma once
#include <vector>
#include <Engine/Core/Scripting/ScriptAPI.hpp>

namespace gcep::scripting {
    inline void clearScriptRegistry() {
        getScriptRegistry().clear();
    }
}
