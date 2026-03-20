#pragma once

/// @file script_interface.hpp
/// @brief ABI contract — engine-free, standalone.

#ifdef _WIN32
    #define SCRIPT_API extern "C" __declspec(dllexport)
#else
    #define SCRIPT_API extern "C" __attribute__((visibility("default")))
#endif

typedef void*        ScriptInstance;
typedef unsigned int EntityID;
typedef unsigned int ComponentTypeID;
typedef void*        ComponentHandle;

#define GCEP_INVALID_ENTITY 0xffffffff

/// @brief ABI-stable vtable.
struct ScriptECSAPI
{
    // Component access
    bool            (*hasComponent)   (void* reg, EntityID id, ComponentTypeID t);
    ComponentHandle (*getComponent)   (void* reg, EntityID id, ComponentTypeID t);
    ComponentHandle (*addComponent)   (void* reg, EntityID id, ComponentTypeID t);
    void            (*removeComponent)(void* reg, EntityID id, ComponentTypeID t);

    // Entity lifetime
    EntityID (*createEntity)(void* reg);
    void     (*destroyEntity)(void* reg, EntityID id);

    // Hierarchy
    EntityID (*getParent)      (void* reg, EntityID id);
    EntityID (*getFirstChild)  (void* reg, EntityID id);
    EntityID (*getNextSibling) (void* reg, EntityID id);

    // String helpers (for components with std::string fields)
    /// Returns chars written.
    int  (*getMeshPath)    (void* reg, EntityID id, char* buf, int bufLen);
    int  (*getTexturePath) (void* reg, EntityID id, char* buf, int bufLen);
    int  (*getTag)         (void* reg, EntityID id, char* buf, int bufLen);
    void (*setTag)         (void* reg, EntityID id, const char* tag);

    // Physics runtime
    void (*getLinearVelocity) (void* reg, EntityID id, float* outXYZ);
    void (*setLinearVelocity) (void* reg, EntityID id, float x, float y, float z);
    void (*addForce)          (void* reg, EntityID id, float x, float y, float z);
    void (*addImpulse)        (void* reg, EntityID id, float x, float y, float z);
};

struct ScriptInputAPI
{
    // Polling
    bool (*isPressed) (void* input, int key);
    bool (*isHeld)    (void* input, int key);
    bool (*isReleased)(void* input, int key);

    bool (*isMousePressed) (void* input, int button);
    bool (*isMouseHeld)    (void* input, int button);
    bool (*isMouseReleased)(void* input, int button);

    void (*getMousePos)   (void* input, double* outX, double* outY);
    void (*getMouseDelta) (void* input, double* outX, double* outY);
    void (*getMouseScroll)(void* input, double* outX, double* outY);

    // Callback bindings
    // TriggerOn: 0=Pressed, 1=Held, 2=Released
    void (*addKeyBinding)   (void* input, const char* name, int key,
                             unsigned int triggerOn, void(*callback)(void*), void* userData);
    void (*addMouseBinding) (void* input, const char* name, int key,
                             unsigned int triggerOn, void(*callback)(void*), void* userData);
    void (*removeBinding)(void* input, const char* name);
    void (*clearBindings)(void* input);
};

/// @brief Context passed to each user script.
struct ScriptContext
{
    EntityID              entityId;
    float                 deltaTime;
    void*                 registry;
    const ScriptECSAPI*   ecs;
    void*                 input;    ///< opaque InputSystem*  (may be null)
    const ScriptInputAPI* inputApi; ///< null if no input system bound
};
