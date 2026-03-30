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

    // Rotation helpers — always write through these, never touch ScriptQuat directly.
    // All angles are in RADIANS.

    /// Overwrite the entity's rotation with the given Euler angles (XYZ / pitch-yaw-roll).
    /// Updates both rotation quaternion and the editor's Euler display-cache atomically.
    void (*setRotationEuler)(void* reg, EntityID id, float x, float y, float z);

    /// Concatenate an incremental Euler rotation on top of the current rotation.
    /// Equivalent to: rotation = glm::quat(delta) * rotation, then normalise.
    /// Updates the display-cache as well.
    void (*rotateEuler)     (void* reg, EntityID id, float x, float y, float z);

    /// Read back the current rotation as a quaternion (w, x, y, z).
    void (*getRotationQuat) (void* reg, EntityID id, float* outWXYZ);

    /// Read back the current rotation as Euler angles in radians (XYZ order).
    void (*getRotationEuler)(void* reg, EntityID id, float* outXYZ);
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

/// @brief Scripting vtable for CameraComponent operations.
///
/// Scripts should consume this API exclusively when they need to read or
/// mutate a CameraComponent — never cast the ComponentHandle and poke fields
/// directly, as the engine-side layout may differ from ScriptCameraComponent.
///
/// Pitch/yaw angles are always in DEGREES to match CameraComponent's storage
/// convention and the editor Camera class.
struct ScriptCameraAPI
{
    // ── Projection ────────────────────────────────────────────────────────

    /// Read the vertical field-of-view (degrees).
    float (*getFovY)   (void* reg, EntityID id);
    /// Overwrite the vertical field-of-view (degrees). Clamped to (0, 180).
    void  (*setFovY)   (void* reg, EntityID id, float fovYDeg);

    /// Read the near-clip distance.
    float (*getNearZ)  (void* reg, EntityID id);
    /// Set the near-clip distance. Must be > 0.
    void  (*setNearZ)  (void* reg, EntityID id, float nearZ);

    /// Read the far-clip distance.
    float (*getFarZ)   (void* reg, EntityID id);
    /// Set the far-clip distance. Must be > nearZ.
    void  (*setFarZ)   (void* reg, EntityID id, float farZ);

    /// Returns non-zero if this camera is the scene's main/active camera.
    bool  (*isMain)    (void* reg, EntityID id);
    /// Promote or demote this camera as the main camera.
    void  (*setMain)   (void* reg, EntityID id, bool isMain);

    // ── Position ─────────────────────────────────────────────────────────

    /// Read the world-space eye position into outXYZ[3].
    void  (*getPosition)(void* reg, EntityID id, float* outXYZ);

    /// Overwrite the world-space eye position.
    void  (*setPosition)(void* reg, EntityID id, float x, float y, float z);

    /// Translate the camera by (dx, dy, dz) in world space.
    void  (*translate)  (void* reg, EntityID id, float dx, float dy, float dz);

    // ── Orientation ───────────────────────────────────────────────────────
    // All angles are in DEGREES, matching CameraComponent storage.

    /// Read the current pitch (vertical rotation, degrees).
    float (*getPitch)   (void* reg, EntityID id);

    /// Overwrite pitch (degrees). Clamped to [-89, 89].
    void  (*setPitch)   (void* reg, EntityID id, float pitch);

    /// Add a delta to pitch (degrees). The result is clamped to [-89, 89].
    void  (*addPitch)   (void* reg, EntityID id, float deltaDeg);

    /// Read the current yaw (horizontal rotation, degrees).
    float (*getYaw)     (void* reg, EntityID id);

    /// Overwrite yaw (degrees). Unclamped — wraps naturally via trig.
    void  (*setYaw)     (void* reg, EntityID id, float yaw);

    /// Add a delta to yaw (degrees).
    void  (*addYaw)     (void* reg, EntityID id, float deltaDeg);
};

/// @brief Context passed to each user script.
struct ScriptContext
{
    EntityID              entityId;
    float                 deltaTime;
    void*                 registry;
    const ScriptECSAPI*   ecs;
    void*                 input;          ///< opaque InputSystem*  (may be null)
    const ScriptInputAPI* inputApi;       ///< null if no input system bound
    const ScriptCameraAPI* cameraApi;     ///< null if camera system not bound
};
