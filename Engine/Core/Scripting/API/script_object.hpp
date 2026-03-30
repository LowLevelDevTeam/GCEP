#pragma once

/// @file script_object.hpp
/// @brief Unity-style component accessors and input helper for GCEngine scripts.
///
/// Provides @c ComponentRef<T,ID>, specialisations for @c ScriptTransform and
/// @c ScriptCameraComponent, the @c GameObject facade, and the @c Input helper.
/// Include this file (via @c script_api.hpp) in every user script.

// Internals
#include "key_codes.hpp"
#include "script_components.hpp"
#include "script_interface.hpp"

// STD
#include <cstring>
#include <cstdio>

// ── Component type IDs ────────────────────────────────────────────────────────

/// @brief Stable integer identifiers for component types recognised by the scripting bridge.
///
/// Values must stay in sync with the @c SType enum in @c script_ecs_bridge.cpp.
/// Do not reorder or insert entries without updating both sides.
enum class ScriptComponentType : unsigned int
{
    Transform      = 0, ///< Local-space @c ScriptTransform.
    WorldTransform = 1, ///< World-space @c ScriptWorldTransform (read-only from scripts).
    Physics        = 2, ///< @c ScriptPhysicsComponent.
    Camera         = 3, ///< @c ScriptCameraComponent.
    PointLight     = 4, ///< @c ScriptPointLightComponent.
    SpotLight      = 5, ///< @c ScriptSpotLightComponent.
    Hierarchy      = 6, ///< @c ScriptHierarchyComponent (read-only from scripts).
};

// ── Generic component handle wrapper ─────────────────────────────────────────

/// @brief Type-safe, non-owning reference to an ECS component retrieved via the scripting bridge.
///
/// Wraps the raw pointer returned by @c ScriptECSAPI::getComponent() and pairs it
/// with the @c ScriptContext so that @c destroy() can route back through the vtable.
/// Behaves like an optional pointer: check @c operator bool() before dereferencing.
///
/// @tparam T   The concrete component data type (e.g. @c ScriptPhysicsComponent).
/// @tparam ID  The @c ScriptComponentType enum value that identifies this component.
template<typename T, ScriptComponentType ID>
struct ComponentRef
{
    T*                   _ptr = nullptr; ///< Raw pointer to the component data; @c nullptr if not present.
    const ScriptContext* _ctx = nullptr; ///< Context used for ECS vtable calls; @c nullptr if unbound.

    /// @brief Returns @c true when the reference holds a valid (non-null) component pointer.
    explicit operator bool() const { return _ptr != nullptr; }

    /// @brief Arrow operator — access component fields directly.
    T* operator->() const { return _ptr; }

    /// @brief Dereference — returns a reference to the component data.
    T& operator*()  const { return *_ptr; }

    /// @brief Removes this component from the owning entity via the ECS vtable.
    ///
    /// Calls @c ScriptECSAPI::removeComponent() and nulls @c _ptr. Safe to call
    /// on an unbound or already-null ref (no-op). After this call the ComponentRef
    /// is invalid.
    void destroy()
    {
        if (!_ctx || !_ptr) return;
        _ctx->ecs->removeComponent(_ctx->registry, _ctx->entityId, static_cast<unsigned int>(ID));
        _ptr = nullptr;
    }
};

// ── ScriptTransform specialisation ───────────────────────────────────────────
// Routes rotation reads and writes through the ScriptECSAPI vtable so that the
// engine keeps its quaternion and Euler display-cache in sync at all times.
//
// eulerRadians is NOT directly accessible from scripts. Use the methods below.
// All angles are in RADIANS unless the _deg suffix is used.

/// @brief @c ComponentRef specialisation for the local-space transform component.
///
/// All rotation mutations route through @c ScriptECSAPI::setRotationEuler() /
/// @c rotateEuler() so the engine's internal quaternion and editor Euler cache
/// remain consistent. Direct writes to @c _ptr->rotation are therefore
/// unsupported and will silently fall out of sync.
///
/// @note Position and scale can still be written directly via @c _ptr->position
///       and @c _ptr->scale — only rotation requires the indirect path.
template<>
struct ComponentRef<ScriptTransform, ScriptComponentType::Transform>
{
    ScriptTransform*     _ptr = nullptr; ///< Raw pointer to the local transform data.
    const ScriptContext* _ctx = nullptr; ///< Bound scripting context.

    /// @brief Returns @c true when the reference holds a valid component pointer.
    explicit operator bool() const { return _ptr != nullptr; }

    /// @brief Arrow operator — access transform fields directly.
    ScriptTransform*   operator->() const { return _ptr; }

    /// @brief Dereference operator.
    ScriptTransform&   operator*()  const { return *_ptr; }

    /// @brief Removes the transform component from the owning entity.
    void destroy()
    {
        if (!_ctx || !_ptr) return;
        _ctx->ecs->removeComponent(_ctx->registry, _ctx->entityId,
                                   static_cast<unsigned int>(ScriptComponentType::Transform));
        _ptr = nullptr;
    }

    // ── Absolute rotation setters ─────────────────────────────────────────────

    /// @brief Sets the entity's rotation to the given Euler angles in radians (XYZ / pitch-yaw-roll order).
    ///
    /// Delegates to @c ScriptECSAPI::setRotationEuler() so the engine's quaternion
    /// and Euler display-cache remain in sync.
    ///
    /// @param x  Pitch (X-axis rotation) in radians.
    /// @param y  Yaw   (Y-axis rotation) in radians.
    /// @param z  Roll  (Z-axis rotation) in radians.
    void setRotation(float x, float y, float z) const
    {
        if (_ctx) _ctx->ecs->setRotationEuler(_ctx->registry, _ctx->entityId, x, y, z);
    }

    /// @brief Sets rotation from a @c ScriptVec3 of Euler angles in radians.
    /// @param e  Euler angles (x=pitch, y=yaw, z=roll) in radians.
    void setRotation(ScriptVec3 e) const { setRotation(e.x, e.y, e.z); }

    /// @brief Sets the entity's rotation from Euler angles in degrees (XYZ order).
    ///
    /// Converts from degrees to radians internally then delegates to @c setRotation().
    ///
    /// @param x  Pitch in degrees.
    /// @param y  Yaw   in degrees.
    /// @param z  Roll  in degrees.
    void setRotationDeg(float x, float y, float z) const
    {
        constexpr float kToRad = 3.14159265f / 180.f;
        setRotation(x * kToRad, y * kToRad, z * kToRad);
    }

    /// @brief Sets rotation from a @c ScriptVec3 of Euler angles in degrees.
    /// @param e  Euler angles in degrees.
    void setRotationDeg(ScriptVec3 e) const { setRotationDeg(e.x, e.y, e.z); }

    // ── Per-axis absolute setters ─────────────────────────────────────────────

    /// @brief Overwrites only the pitch (X-axis) component; Y and Z are preserved.
    ///
    /// Reads the current Euler angles via @c getRotationEuler(), replaces X, then
    /// calls @c setRotationEuler(). All in radians.
    ///
    /// @param x  New pitch value in radians.
    void setRotationX(float x) const
    {
        float e[3] = {};
        if (_ctx) { _ctx->ecs->getRotationEuler(_ctx->registry, _ctx->entityId, e); setRotation(x, e[1], e[2]); }
    }

    /// @brief Overwrites only the yaw (Y-axis) component; X and Z are preserved.
    /// @param y  New yaw value in radians.
    void setRotationY(float y) const
    {
        float e[3] = {};
        if (_ctx) { _ctx->ecs->getRotationEuler(_ctx->registry, _ctx->entityId, e); setRotation(e[0], y, e[2]); }
    }

    /// @brief Overwrites only the roll (Z-axis) component; X and Y are preserved.
    /// @param z  New roll value in radians.
    void setRotationZ(float z) const
    {
        float e[3] = {};
        if (_ctx) { _ctx->ecs->getRotationEuler(_ctx->registry, _ctx->entityId, e); setRotation(e[0], e[1], z); }
    }

    // ── Incremental rotation (delta) ──────────────────────────────────────────

    /// @brief Applies an incremental Euler rotation in radians on top of the current rotation.
    ///
    /// Delegates to @c ScriptECSAPI::rotateEuler(). The delta is composed with
    /// the current rotation, not added component-wise to the Euler cache.
    ///
    /// @param x  Pitch delta in radians.
    /// @param y  Yaw   delta in radians.
    /// @param z  Roll  delta in radians.
    void rotate(float x, float y, float z) const
    {
        if (_ctx) _ctx->ecs->rotateEuler(_ctx->registry, _ctx->entityId, x, y, z);
    }

    /// @brief Applies an incremental Euler rotation from a @c ScriptVec3 (radians).
    /// @param e  Delta Euler angles in radians.
    void rotate(ScriptVec3 e) const { rotate(e.x, e.y, e.z); }

    /// @brief Applies an incremental Euler rotation in degrees on top of the current rotation.
    ///
    /// Converts from degrees to radians then delegates to @c rotate().
    ///
    /// @param x  Pitch delta in degrees.
    /// @param y  Yaw   delta in degrees.
    /// @param z  Roll  delta in degrees.
    void rotateDeg(float x, float y, float z) const
    {
        constexpr float kToRad = 3.14159265f / 180.f;
        rotate(x * kToRad, y * kToRad, z * kToRad);
    }

    /// @brief Applies an incremental Euler rotation from a @c ScriptVec3 (degrees).
    /// @param e  Delta Euler angles in degrees.
    void rotateDeg(ScriptVec3 e) const { rotateDeg(e.x, e.y, e.z); }

    /// @brief Applies an incremental pitch rotation in radians.
    void rotateX(float x) const { rotate(x,   0.f, 0.f); }
    /// @brief Applies an incremental yaw rotation in radians.
    void rotateY(float y) const { rotate(0.f, y,   0.f); }
    /// @brief Applies an incremental roll rotation in radians.
    void rotateZ(float z) const { rotate(0.f, 0.f, z  ); }

    /// @brief Applies an incremental pitch rotation in degrees.
    void rotateXDeg(float x) const { rotateDeg(x,   0.f, 0.f); }
    /// @brief Applies an incremental yaw rotation in degrees.
    void rotateYDeg(float y) const { rotateDeg(0.f, y,   0.f); }
    /// @brief Applies an incremental roll rotation in degrees.
    void rotateZDeg(float z) const { rotateDeg(0.f, 0.f, z  ); }

    // ── Rotation read-back ────────────────────────────────────────────────────

    /// @brief Returns the current rotation as a unit quaternion (w, x, y, z).
    ///
    /// Reads via @c ScriptECSAPI::getRotationQuat(). Returns the identity
    /// quaternion if the context is unbound.
    [[nodiscard]] ScriptQuat getRotation() const
    {
        float q[4] = { 1.f, 0.f, 0.f, 0.f };
        if (_ctx) _ctx->ecs->getRotationQuat(_ctx->registry, _ctx->entityId, q);
        return { q[0], q[1], q[2], q[3] };
    }

    /// @brief Returns the current rotation as Euler angles in radians (XYZ order).
    ///
    /// Reads via @c ScriptECSAPI::getRotationEuler(). Returns @c {0,0,0} if unbound.
    [[nodiscard]] ScriptVec3 getEuler() const
    {
        float e[3] = {};
        if (_ctx) _ctx->ecs->getRotationEuler(_ctx->registry, _ctx->entityId, e);
        return { e[0], e[1], e[2] };
    }

    /// @brief Returns the current rotation as Euler angles in degrees (XYZ order).
    ///
    /// Converts the result of @c getEuler() from radians to degrees.
    [[nodiscard]] ScriptVec3 getEulerDeg() const
    {
        constexpr float kToDeg = 180.f / 3.14159265f;
        const ScriptVec3 e = getEuler();
        return { e.x * kToDeg, e.y * kToDeg, e.z * kToDeg };
    }
};

// ── ScriptCameraComponent specialisation ─────────────────────────────────────
// Routes all reads and writes through ScriptCameraAPI so the engine's
// projection and view state stays authoritative.
//
// Do NOT write to _ptr fields directly — always use the methods below.

/// @brief @c ComponentRef specialisation for the perspective camera component.
///
/// Every getter and setter routes through @c ScriptCameraAPI (accessed via the
/// private @c api() helper) so that the engine's projection matrix and view
/// state are always authoritative. Direct field writes on @c _ptr bypass this
/// indirection and will produce incorrect rendering.
template<>
struct ComponentRef<ScriptCameraComponent, ScriptComponentType::Camera>
{
    ScriptCameraComponent* _ptr = nullptr; ///< Raw pointer to the camera component.
    const ScriptContext*   _ctx = nullptr; ///< Bound scripting context.

    /// @brief Returns @c true when the reference holds a valid camera component pointer.
    explicit operator bool() const { return _ptr != nullptr; }

    /// @brief Removes the camera component from the owning entity.
    void destroy()
    {
        if (!_ctx || !_ptr) return;
        _ctx->ecs->removeComponent(_ctx->registry, _ctx->entityId,
                                   static_cast<unsigned int>(ScriptComponentType::Camera));
        _ptr = nullptr;
    }

    // ── Projection ────────────────────────────────────────────────────────────

    /// @brief Returns the vertical field of view in degrees.
    [[nodiscard]] float getFovY()  const { return api() ? api()->getFovY (_ctx->registry, _ctx->entityId) : 60.f;    }
    /// @brief Returns the near clip plane distance (world units).
    [[nodiscard]] float getNearZ() const { return api() ? api()->getNearZ(_ctx->registry, _ctx->entityId) : 0.01f;   }
    /// @brief Returns the far clip plane distance (world units).
    [[nodiscard]] float getFarZ()  const { return api() ? api()->getFarZ (_ctx->registry, _ctx->entityId) : 1000.f;  }
    /// @brief Returns @c true if this is the main (primary viewport) camera.
    [[nodiscard]] bool  isMain()   const { return api() ? api()->isMain  (_ctx->registry, _ctx->entityId) : false;   }

    /// @brief Sets the vertical field of view in degrees.
    void setFovY (float v) const { if (api()) api()->setFovY (_ctx->registry, _ctx->entityId, v); }
    /// @brief Sets the near clip plane distance (world units).
    void setNearZ(float v) const { if (api()) api()->setNearZ(_ctx->registry, _ctx->entityId, v); }
    /// @brief Sets the far clip plane distance (world units).
    void setFarZ (float v) const { if (api()) api()->setFarZ (_ctx->registry, _ctx->entityId, v); }
    /// @brief Sets whether this is the main (primary viewport) camera.
    void setMain (bool  v) const { if (api()) api()->setMain (_ctx->registry, _ctx->entityId, v); }

    // ── Position ──────────────────────────────────────────────────────────────

    /// @brief Returns the world-space eye position.
    [[nodiscard]] ScriptVec3 getPosition() const
    {
        float v[3] = {};
        if (api()) api()->getPosition(_ctx->registry, _ctx->entityId, v);
        return { v[0], v[1], v[2] };
    }

    /// @brief Sets the world-space eye position.
    /// @param x  World-space X coordinate.
    /// @param y  World-space Y coordinate.
    /// @param z  World-space Z coordinate.
    void setPosition(float x, float y, float z) const
    {
        if (api()) api()->setPosition(_ctx->registry, _ctx->entityId, x, y, z);
    }

    /// @brief Sets the world-space eye position from a @c ScriptVec3.
    void setPosition(ScriptVec3 p) const { setPosition(p.x, p.y, p.z); }

    /// @brief Moves the camera by @p (dx, dy, dz) in world space.
    ///
    /// Equivalent to @c setPosition(getPosition() + delta), but routed through
    /// the camera API so the view matrix is updated immediately.
    ///
    /// @param dx  X-axis translation delta (world units).
    /// @param dy  Y-axis translation delta (world units).
    /// @param dz  Z-axis translation delta (world units).
    void translate(float dx, float dy, float dz) const
    {
        if (api()) api()->translate(_ctx->registry, _ctx->entityId, dx, dy, dz);
    }

    /// @brief Moves the camera by a @c ScriptVec3 delta in world space.
    void translate(ScriptVec3 d) const { translate(d.x, d.y, d.z); }

    // ── Orientation (degrees) ─────────────────────────────────────────────────

    /// @brief Returns the current pitch (vertical rotation) in degrees.
    [[nodiscard]] float getPitch() const { return api() ? api()->getPitch(_ctx->registry, _ctx->entityId) : 0.f;     }
    /// @brief Returns the current yaw (horizontal rotation) in degrees.
    [[nodiscard]] float getYaw()   const { return api() ? api()->getYaw  (_ctx->registry, _ctx->entityId) : -180.f;  }

    /// @brief Overwrites the pitch (degrees). Clamped to [-89, 89] by the bridge.
    /// @param deg  New pitch value in degrees.
    void setPitch(float deg) const { if (api()) api()->setPitch(_ctx->registry, _ctx->entityId, deg); }

    /// @brief Overwrites the yaw (degrees).
    /// @param deg  New yaw value in degrees.
    void setYaw(float deg) const { if (api()) api()->setYaw(_ctx->registry, _ctx->entityId, deg); }

    /// @brief Adds @p deg to the current pitch. Result is clamped to [-89, 89].
    /// @param deg  Pitch delta in degrees.
    void addPitch(float deg) const { if (api()) api()->addPitch(_ctx->registry, _ctx->entityId, deg); }

    /// @brief Adds @p deg to the current yaw (no clamping applied).
    /// @param deg  Yaw delta in degrees.
    void addYaw(float deg) const { if (api()) api()->addYaw(_ctx->registry, _ctx->entityId, deg); }

private:
    /// @brief Returns @c _ctx->cameraApi if both the context and the API pointer are valid.
    [[nodiscard]] const ScriptCameraAPI* api() const
    {
        return (_ctx && _ctx->cameraApi) ? _ctx->cameraApi : nullptr;
    }
};

// ── GameObject ────────────────────────────────────────────────────────────────

/// @brief Unity-style facade over a single ECS entity.
///
/// Constructed from a @c ScriptContext* and exposes component access, hierarchy
/// traversal, mesh info, and physics helpers without requiring scripts to call
/// ECS API function pointers directly. Lightweight — holds only a context pointer.
///
/// ### Typical script usage
/// @code
/// void onUpdate(const ScriptContext* ctx)
/// {
///     GameObject self(ctx);
///     auto tf = self.getComponent<ScriptTransform, ScriptComponentType::Transform>();
///     if (tf) tf->position.y += 1.f * ctx->deltaTime;
/// }
/// @endcode
class GameObject
{
public:
    /// @brief Constructs a @c GameObject bound to @p ctx's entity.
    /// @param ctx  Must not be @c nullptr and must remain valid for the lifetime of this object.
    explicit GameObject(const ScriptContext* ctx) : m_ctx(ctx) {}

    // ── Identity ──────────────────────────────────────────────────────────────

    /// @brief Returns the entity ID this @c GameObject represents.
    EntityID id() const { return m_ctx->entityId; }

    /// @brief Returns the entity's tag string, or @c "" if none is set.
    ///
    /// Writes into a static 256-byte buffer; do not store the pointer across frames.
    [[nodiscard]] const char* tag() const
    {
        static char buf[256];
        m_ctx->ecs->getTag(m_ctx->registry, m_ctx->entityId, buf, sizeof(buf));
        return buf;
    }

    /// @brief Sets the entity's tag string.
    /// @param t  Null-terminated tag string to assign.
    void setTag(const char* t) const
    {
        m_ctx->ecs->setTag(m_ctx->registry, m_ctx->entityId, t);
    }

    // ── Component access ──────────────────────────────────────────────────────

    /// @brief Returns a @c ComponentRef to an existing component, or an empty ref if not present.
    ///
    /// @tparam T   Component data type.
    /// @tparam ID  @c ScriptComponentType enum value.
    /// @return     Bound @c ComponentRef if the component exists; default-constructed (null) ref otherwise.
    template<typename T, ScriptComponentType ID>
    ComponentRef<T, ID> getComponent() const
    {
        if (!m_ctx->ecs->hasComponent(m_ctx->registry, m_ctx->entityId, static_cast<unsigned int>(ID)))
            return {};

        return { static_cast<T*>(m_ctx->ecs->getComponent(m_ctx->registry, m_ctx->entityId, static_cast<unsigned int>(ID))), m_ctx };
    }

    /// @brief Adds a component of type @c T to the entity and returns a ref to it.
    ///
    /// If the component already exists, the behaviour is defined by the ECS implementation
    /// (typically a no-op that returns the existing component).
    ///
    /// @tparam T   Component data type.
    /// @tparam ID  @c ScriptComponentType enum value.
    /// @return     Bound @c ComponentRef to the newly added component.
    template<typename T, ScriptComponentType ID>
    ComponentRef<T, ID> addComponent() const
    {
        return { static_cast<T*>(m_ctx->ecs->addComponent(m_ctx->registry, m_ctx->entityId, static_cast<unsigned int>(ID))), m_ctx };
    }

    /// @brief Returns @c true if the entity currently has a component of type @c T.
    ///
    /// @tparam T   Component data type (unused at runtime; used for type disambiguation).
    /// @tparam ID  @c ScriptComponentType enum value.
    template<typename T, ScriptComponentType ID>
    [[nodiscard]] bool hasComponent() const
    {
        return m_ctx->ecs->hasComponent(m_ctx->registry, m_ctx->entityId, static_cast<unsigned int>(ID));
    }

    /// @brief Returns a ref to an existing component or adds it if absent.
    ///
    /// Equivalent to @code hasComponent() ? getComponent() : addComponent() @endcode
    ///
    /// @tparam T   Component data type.
    /// @tparam ID  @c ScriptComponentType enum value.
    template<typename T, ScriptComponentType ID>
    ComponentRef<T, ID> getOrAddComponent() const
    {
        return hasComponent<T, ID>() ? getComponent<T, ID>() : addComponent<T, ID>();
    }

    // ── Hierarchy ─────────────────────────────────────────────────────────────

    /// @brief Returns the parent entity ID, or @c GCEP_INVALID_ENTITY if this is a root.
    [[nodiscard]] EntityID parent()      const { return m_ctx->ecs->getParent(m_ctx->registry,      m_ctx->entityId); }
    /// @brief Returns the first child entity ID, or @c GCEP_INVALID_ENTITY if childless.
    [[nodiscard]] EntityID firstChild()  const { return m_ctx->ecs->getFirstChild(m_ctx->registry,  m_ctx->entityId); }
    /// @brief Returns the next sibling entity ID in the parent's child list, or @c GCEP_INVALID_ENTITY.
    [[nodiscard]] EntityID nextSibling() const { return m_ctx->ecs->getNextSibling(m_ctx->registry, m_ctx->entityId); }

    /// @brief Returns @c true if the entity has a parent (is not a scene root).
    [[nodiscard]] bool hasParent()   const { return parent()     != GCEP_INVALID_ENTITY; }
    /// @brief Returns @c true if the entity has at least one child.
    [[nodiscard]] bool hasChildren() const { return firstChild() != GCEP_INVALID_ENTITY; }

    // ── Mesh info ─────────────────────────────────────────────────────────────

    /// @brief Returns the path of the mesh asset attached to this entity, or @c "" if none.
    ///
    /// Writes into a static 512-byte buffer; do not store the pointer across frames.
    [[nodiscard]] const char* meshPath() const
    {
        static char buf[512];
        m_ctx->ecs->getMeshPath(m_ctx->registry, m_ctx->entityId, buf, sizeof(buf));
        return buf;
    }

    // ── Physics ───────────────────────────────────────────────────────────────

    /// @brief Returns the current linear velocity of the entity's physics body (world units/s).
    ///
    /// Returns @c {0,0,0} if the entity has no dynamic physics component.
    [[nodiscard]] ScriptVec3 velocity() const
    {
        float v[3]{};
        m_ctx->ecs->getLinearVelocity(m_ctx->registry, m_ctx->entityId, v);
        return { v[0], v[1], v[2] };
    }

    /// @brief Directly sets the linear velocity of the entity's physics body.
    ///
    /// Only has an effect on @c Dynamic and @c Kinematic bodies.
    ///
    /// @param x  Velocity X component (world units/s).
    /// @param y  Velocity Y component (world units/s).
    /// @param z  Velocity Z component (world units/s).
    void setVelocity(float x, float y, float z) const
    {
        m_ctx->ecs->setLinearVelocity(m_ctx->registry, m_ctx->entityId, x, y, z);
    }

    /// @brief Sets the linear velocity from a @c ScriptVec3.
    void setVelocity(ScriptVec3 v) const { setVelocity(v.x, v.y, v.z); }

    /// @brief Applies a continuous force to the physics body (world units, accumulated per step).
    ///
    /// @param x  Force X component.
    /// @param y  Force Y component.
    /// @param z  Force Z component.
    void addForce(float x, float y, float z) const { m_ctx->ecs->addForce  (m_ctx->registry, m_ctx->entityId, x, y, z); }

    /// @brief Applies an instantaneous impulse to the physics body.
    ///
    /// @param x  Impulse X component.
    /// @param y  Impulse Y component.
    /// @param z  Impulse Z component.
    void addImpulse(float x, float y, float z) const { m_ctx->ecs->addImpulse(m_ctx->registry, m_ctx->entityId, x, y, z); }

    /// @brief Applies a continuous force from a @c ScriptVec3.
    void addForce  (ScriptVec3 f) const { addForce  (f.x, f.y, f.z); }
    /// @brief Applies an instantaneous impulse from a @c ScriptVec3.
    void addImpulse(ScriptVec3 i) const { addImpulse(i.x, i.y, i.z); }

private:
    const ScriptContext* m_ctx; ///< Non-owning pointer to the current script context.
};

// ── Convenience aliases ───────────────────────────────────────────────────────

/// @name Component type aliases
/// @brief Short, Unity-style aliases for the most common @c ComponentRef instantiations.
/// @{
using Transform      = ComponentRef<ScriptTransform,          ScriptComponentType::Transform>;
using WorldTransform = ComponentRef<ScriptWorldTransform,     ScriptComponentType::WorldTransform>;
using Physics        = ComponentRef<ScriptPhysicsComponent,   ScriptComponentType::Physics>;
using Camera         = ComponentRef<ScriptCameraComponent,    ScriptComponentType::Camera>;
using PointLight     = ComponentRef<ScriptPointLightComponent,ScriptComponentType::PointLight>;
using SpotLight      = ComponentRef<ScriptSpotLightComponent, ScriptComponentType::SpotLight>;
/// @}

/// @name Component accessor macros
/// @brief Typed @c getComponent shortcuts for cleaner script code.
///
/// Each macro expands to a @c getComponent() call on the provided @p gameObject
/// and returns the corresponding @c ComponentRef specialisation. Example:
/// @code
/// auto tf = GET_TRANSFORM(self);
/// if (tf) tf->position.y += 1.f;
/// @endcode
/// @{
#define GET_TRANSFORM(gameObject)       (gameObject).getComponent<ScriptTransform,          ScriptComponentType::Transform>()
#define GET_WORLD_TRANSFORM(gameObject) (gameObject).getComponent<ScriptWorldTransform,     ScriptComponentType::WorldTransform>()
#define GET_PHYSICS(gameObject)         (gameObject).getComponent<ScriptPhysicsComponent,   ScriptComponentType::Physics>()
#define GET_CAMERA(gameObject)          (gameObject).getComponent<ScriptCameraComponent,    ScriptComponentType::Camera>()
#define GET_POINT_LIGHT(gameObject)     (gameObject).getComponent<ScriptPointLightComponent,ScriptComponentType::PointLight>()
#define GET_SPOT_LIGHT(gameObject)      (gameObject).getComponent<ScriptSpotLightComponent, ScriptComponentType::SpotLight>()
/// @}

// ── Input ─────────────────────────────────────────────────────────────────────

/// @brief Unity-style input accessor. Mirrors the interface of @c UnityEngine.Input.
///
/// Wraps the @c ScriptInputAPI and @c ScriptContext pointers so scripts can poll
/// keyboard and mouse state without touching engine internals directly. Construct
/// from the @c ScriptContext* received in @c onUpdate().
///
/// @note Only valid when @c ctx->inputApi is non-null (i.e. a camera entity is
///       active). Always guard with @c valid() before polling.
class Input
{
public:
    /// @brief Constructs an @c Input accessor bound to @p ctx.
    /// @param ctx  Context to read input state from.
    explicit Input(const ScriptContext* ctx) : m_ctx(ctx) {}

    /// @brief Returns @c true when both the input system and API table are available.
    [[nodiscard]] bool valid() const { return m_ctx && m_ctx->input && m_ctx->inputApi; }

    // ── Keys ──────────────────────────────────────────────────────────────────

    /// @brief Returns @c true on the exact frame the key transitioned to pressed.
    /// @param key  Key code to query.
    [[nodiscard]] bool getKeyDown(gcep::Key key) const
    { return getKeyDown(static_cast<int>(key)); }

    /// @brief Returns @c true every frame the key is held down.
    /// @param key  Key code to query.
    [[nodiscard]] bool getKey(gcep::Key key) const
    { return getKey(static_cast<int>(key)); }

    /// @brief Returns @c true on the exact frame the key was released.
    /// @param key  Key code to query.
    [[nodiscard]] bool getKeyUp(gcep::Key key) const
    { return getKeyUp(static_cast<int>(key)); }

    // ── Mouse buttons ─────────────────────────────────────────────────────────

    /// @brief Returns @c true on the exact frame the mouse button was pressed.
    /// @param btn  Mouse button to query.
    [[nodiscard]] bool getMouseButtonDown(gcep::MouseButton btn) const
    { return getMouseButtonDown(static_cast<int>(btn)); }

    /// @brief Returns @c true every frame the mouse button is held.
    /// @param btn  Mouse button to query.
    [[nodiscard]] bool getMouseButton(gcep::MouseButton btn) const
    { return getMouseButton(static_cast<int>(btn)); }

    /// @brief Returns @c true on the exact frame the mouse button was released.
    /// @param btn  Mouse button to query.
    [[nodiscard]] bool getMouseButtonUp(gcep::MouseButton btn) const
    { return getMouseButtonUp(static_cast<int>(btn)); }

    // ── Mouse axes ────────────────────────────────────────────────────────────

    /// @brief Returns the screen-space cursor position in pixels as a @c ScriptVec3 (z = 0).
    [[nodiscard]] ScriptVec3 mousePosition() const
    {
        if (!valid()) return {};
        double x, y;
        m_ctx->inputApi->getMousePos(m_ctx->input, &x, &y);
        return { static_cast<float>(x), static_cast<float>(y), 0.f };
    }

    /// @brief Returns the cursor movement since the last frame in pixels (z = 0).
    [[nodiscard]] ScriptVec3 mouseDelta() const
    {
        if (!valid()) return {};
        double x, y;
        m_ctx->inputApi->getMouseDelta(m_ctx->input, &x, &y);
        return { static_cast<float>(x), static_cast<float>(y), 0.f };
    }

    /// @brief Returns the scroll-wheel delta this frame (z = 0).
    [[nodiscard]] ScriptVec3 mouseScroll() const
    {
        if (!valid()) return {};
        double x, y;
        m_ctx->inputApi->getMouseScroll(m_ctx->input, &x, &y);
        return { static_cast<float>(x), static_cast<float>(y), 0.f };
    }

    // ── Callback bindings ─────────────────────────────────────────────────────

    /// @brief Registers a named keyboard binding that fires @p callback when triggered.
    ///
    /// @param name      Unique binding name (used to remove it with @c unbind()).
    /// @param key       Key to listen for.
    /// @param trigger   When to fire: @c TriggerOn::Pressed, @c Held, or @c Released.
    /// @param callback  Function called with @p userData when the binding triggers.
    /// @param userData  Arbitrary user pointer forwarded to @p callback (may be @c nullptr).
    void bindKey(const char* name, gcep::Key key, gcep::TriggerOn trigger,
                 void(*callback)(void*), void* userData = nullptr) const
    { bindKey(name, static_cast<int>(key), static_cast<unsigned int>(trigger), callback, userData); }

    /// @brief Registers a named mouse-button binding that fires @p callback when triggered.
    ///
    /// @param name      Unique binding name.
    /// @param key       Mouse button index.
    /// @param triggerOn Raw trigger enum value (0=Pressed, 1=Held, 2=Released).
    /// @param callback  Callback function.
    /// @param userData  Forwarded to @p callback.
    void bindMouseKey(const char* name, int key, unsigned int triggerOn, void(*callback)(void*), void* userData = nullptr) const
    {
        if (valid())
            m_ctx->inputApi->addMouseBinding(m_ctx->input, name, key, triggerOn, callback, userData);
    }

    /// @brief Removes a previously registered binding by name.
    /// @param name  The binding name passed to @c bindKey() or @c bindMouseKey().
    void unbind(const char* name) const
    {
        if (valid()) m_ctx->inputApi->removeBinding(m_ctx->input, name);
    }

    /// @brief Removes all registered key and mouse bindings in one call.
    void clearAllBindings() const
    {
        if (valid()) m_ctx->inputApi->clearBindings(m_ctx->input);
    }

private:
    // ── Raw integer overloads (private) ───────────────────────────────────────

    /// @brief Returns @c true on the exact frame the key with raw code @p key was pressed.
    [[nodiscard]] bool getKeyDown(int key) const
    { return valid() && m_ctx->inputApi->isPressed(m_ctx->input, key); }

    /// @brief Returns @c true every frame the key with raw code @p key is held.
    [[nodiscard]] bool getKey(int key) const
    { return valid() && m_ctx->inputApi->isHeld(m_ctx->input, key); }

    /// @brief Returns @c true on the exact frame the key with raw code @p key was released.
    [[nodiscard]] bool getKeyUp(int key) const
    { return valid() && m_ctx->inputApi->isReleased(m_ctx->input, key); }

    /// @brief Returns @c true on the exact frame mouse button @p btn was pressed.
    [[nodiscard]] bool getMouseButtonDown(int btn) const
    { return valid() && m_ctx->inputApi->isMousePressed(m_ctx->input, btn); }

    /// @brief Returns @c true every frame mouse button @p btn is held.
    [[nodiscard]] bool getMouseButton(int btn) const
    { return valid() && m_ctx->inputApi->isMouseHeld(m_ctx->input, btn); }

    /// @brief Returns @c true on the exact frame mouse button @p btn was released.
    [[nodiscard]] bool getMouseButtonUp(int btn) const
    { return valid() && m_ctx->inputApi->isMouseReleased(m_ctx->input, btn); }

    // ── Private raw bindKey overload ──────────────────────────────────────────

    /// @brief Registers a named keyboard binding using raw integer key and trigger codes.
    void bindKey(const char* name, int key, unsigned int triggerOn, void(*callback)(void*), void* userData = nullptr) const
    {
        if (valid())
            m_ctx->inputApi->addKeyBinding(m_ctx->input, name, key, triggerOn, callback, userData);
    }

private:
    const ScriptContext* m_ctx; ///< Non-owning pointer to the current script context.
};
