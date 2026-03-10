#pragma once

#ifdef _WIN32
    #define SCRIPT_API extern "C" __declspec(dllexport)
#else
    #define SCRIPT_API extern "C" __attribute__((visibility("default")))
#endif

typedef void* ScriptInstance;

// @brief exposed values for scripts
struct ScriptContext {
    unsigned int entityId;
    float        deltaTime;
    void*        ecsWorld;
    void*        inputManager;
};

// each scripts has to export those 5 functions :
//   script_create   -> give script data
//   script_destroy  -> release script data
//   script_on_start -> called on start
//   script_on_update-> called each frame
//   script_on_end   -> called on destroy
