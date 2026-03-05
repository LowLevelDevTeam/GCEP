# GCEngine Scripts

This folder builds a hot-reloadable C++ script library for the engine.

## Build Output
The shared library is copied to the build folder:
- `${build}/Scripts/GCEngineScripts.dll` on Windows
- `${build}/Scripts/GCEngineScripts.so` on Linux
- `${build}/Scripts/GCEngineScripts.dylib` on macOS

## Script API
Scripts must export these C symbols:
- `GCE_CreateScriptPlugin()` returns a `ScriptPlugin*`
- `GCE_DestroyScriptPlugin(ScriptPlugin*)`

See `SampleScript.cpp` for a minimal implementation.

## Hot Reload
The engine loads the library from `${build}/Scripts`. A background watcher polls the file
timestamp and triggers reload when it changes. The engine unloads, copies to a temporary
name, and reloads the module.

## Auto Build
When the editor runs, it watches `Scripts/SampleScript.cpp`. On change it invokes:
`cmake --build <build_dir> --target GCEngineScripts`.
