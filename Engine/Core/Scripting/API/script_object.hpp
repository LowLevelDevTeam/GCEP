#pragma once
/// @file script_object.hpp
/// @brief Unity-style component accessors for GCEngine scripts.
///        Include this instead of script_ecs_helpers.hpp.

// Internals
#include "key_codes.hpp"
#include "script_components.hpp"
#include "script_interface.hpp"

// STD
#include <cstring>
#include <cstdio>

// Component type IDs
// Must stay in sync with SType in script_ecs_bridge.cpp.
enum class ScriptComponentType : unsigned int
{
    Transform      = 0,
    WorldTransform = 1,
    Physics        = 2,
    Camera         = 3,
    PointLight     = 4,
    SpotLight      = 5,
    Hierarchy      = 6,
};

// Component handle wrapper

template<typename T, ScriptComponentType ID>
struct ComponentRef
{
    T*                   _ptr = nullptr;
    const ScriptContext* _ctx = nullptr;

    explicit operator bool() const { return _ptr != nullptr; }
    T*       operator->()    const { return _ptr; }
    T&       operator*()     const { return *_ptr; }

    /// Destroy this component on the entity.
    void destroy()
    {
        if (!_ctx || !_ptr) return;

        _ctx->ecs->removeComponent(_ctx->registry, _ctx->entityId, static_cast<unsigned int>(ID));
        _ptr = nullptr;
    }
};

// ── ScriptTransform specialisation ───────────────────────────────────────────
// Adds rotation helpers that go through the ScriptECSAPI vtable so the engine
// keeps its quaternion and Euler display-cache in sync at all times.
// eulerRadians is NOT accessible from scripts — use the methods below.
//
// All angles are in RADIANS unless the _deg suffix is used.

template<>
struct ComponentRef<ScriptTransform, ScriptComponentType::Transform>
{
    ScriptTransform*     _ptr = nullptr;
    const ScriptContext* _ctx = nullptr;

    explicit operator bool() const { return _ptr != nullptr; }
    ScriptTransform*       operator->() const { return _ptr; }
    ScriptTransform&       operator*()  const { return *_ptr; }

    void destroy()
    {
        if (!_ctx || !_ptr) return;
        _ctx->ecs->removeComponent(_ctx->registry, _ctx->entityId,
                                   static_cast<unsigned int>(ScriptComponentType::Transform));
        _ptr = nullptr;
    }

    // ── Absolute rotation setters ─────────────────────────────────────────────

    /// Set rotation from Euler angles in radians (XYZ / pitch-yaw-roll order).
    void setRotation(float x, float y, float z) const
    {
        if (_ctx) _ctx->ecs->setRotationEuler(_ctx->registry, _ctx->entityId, x, y, z);
    }
    void setRotation(ScriptVec3 e) const { setRotation(e.x, e.y, e.z); }

    /// Set rotation from Euler angles in degrees.
    void setRotationDeg(float x, float y, float z) const
    {
        constexpr float kToRad = 3.14159265f / 180.f;
        setRotation(x * kToRad, y * kToRad, z * kToRad);
    }
    void setRotationDeg(ScriptVec3 e) const { setRotationDeg(e.x, e.y, e.z); }

    // ── Per-axis absolute setters ─────────────────────────────────────────────

    /// Replace only the pitch (X-axis) component; Y and Z read from current rotation.
    void setRotationX(float x) const
    {
        float e[3] = {}; if (_ctx) { _ctx->ecs->getRotationEuler(_ctx->registry, _ctx->entityId, e); setRotation(x, e[1], e[2]); }
    }
    /// Replace only the yaw (Y-axis) component; X and Z read from current rotation.
    void setRotationY(float y) const
    {
        float e[3] = {}; if (_ctx) { _ctx->ecs->getRotationEuler(_ctx->registry, _ctx->entityId, e); setRotation(e[0], y, e[2]); }
    }
    /// Replace only the roll (Z-axis) component; X and Y read from current rotation.
    void setRotationZ(float z) const
    {
        float e[3] = {}; if (_ctx) { _ctx->ecs->getRotationEuler(_ctx->registry, _ctx->entityId, e); setRotation(e[0], e[1], z); }
    }

    // ── Incremental rotation (delta) ──────────────────────────────────────────

    /// Add an incremental Euler rotation in radians on top of the current rotation.
    void rotate(float x, float y, float z) const
    {
        if (_ctx) _ctx->ecs->rotateEuler(_ctx->registry, _ctx->entityId, x, y, z);
    }
    void rotate(ScriptVec3 e) const { rotate(e.x, e.y, e.z); }

    /// Add an incremental Euler rotation in degrees on top of the current rotation.
    void rotateDeg(float x, float y, float z) const
    {
        constexpr float kToRad = 3.14159265f / 180.f;
        rotate(x * kToRad, y * kToRad, z * kToRad);
    }
    void rotateDeg(ScriptVec3 e) const { rotateDeg(e.x, e.y, e.z); }

    // Per-axis incremental helpers
    void rotateX(float x) const { rotate(x,   0.f, 0.f); }
    void rotateY(float y) const { rotate(0.f, y,   0.f); }
    void rotateZ(float z) const { rotate(0.f, 0.f, z  ); }

    void rotateXDeg(float x) const { rotateDeg(x,   0.f, 0.f); }
    void rotateYDeg(float y) const { rotateDeg(0.f, y,   0.f); }
    void rotateZDeg(float z) const { rotateDeg(0.f, 0.f, z  ); }

    // ── Rotation read-back ────────────────────────────────────────────────────

    /// Current rotation as a quaternion (w, x, y, z).
    ScriptQuat getRotation() const
    {
        float q[4] = { 1.f, 0.f, 0.f, 0.f };
        if (_ctx) _ctx->ecs->getRotationQuat(_ctx->registry, _ctx->entityId, q);
        return { q[0], q[1], q[2], q[3] };
    }

    /// Current rotation as Euler angles in radians (XYZ order).
    ScriptVec3 getEuler() const
    {
        float e[3] = {};
        if (_ctx) _ctx->ecs->getRotationEuler(_ctx->registry, _ctx->entityId, e);
        return { e[0], e[1], e[2] };
    }

    /// Current rotation as Euler angles in degrees (XYZ order).
    ScriptVec3 getEulerDeg() const
    {
        constexpr float kToDeg = 180.f / 3.14159265f;
        const ScriptVec3 e = getEuler();
        return { e.x * kToDeg, e.y * kToDeg, e.z * kToDeg };
    }
};

// ── ScriptCameraComponent specialisation ─────────────────────────────────────
// Routes all reads and writes through ScriptCameraAPI so the engine's
// projection / view state stays authoritative.
// Do NOT write to _ptr fields directly — always use the methods below.

template<>
struct ComponentRef<ScriptCameraComponent, ScriptComponentType::Camera>
{
    ScriptCameraComponent* _ptr = nullptr;
    const ScriptContext*   _ctx = nullptr;

    explicit operator bool() const { return _ptr != nullptr; }

    void destroy()
    {
        if (!_ctx || !_ptr) return;
        _ctx->ecs->removeComponent(_ctx->registry, _ctx->entityId,
                                   static_cast<unsigned int>(ScriptComponentType::Camera));
        _ptr = nullptr;
    }

    // ── Projection ────────────────────────────────────────────────────────────

    [[nodiscard]] float getFovY()  const { return api() ? api()->getFovY (_ctx->registry, _ctx->entityId) : 60.f;    }
    [[nodiscard]] float getNearZ() const { return api() ? api()->getNearZ(_ctx->registry, _ctx->entityId) : 0.01f;   }
    [[nodiscard]] float getFarZ()  const { return api() ? api()->getFarZ (_ctx->registry, _ctx->entityId) : 1000.f;  }
    [[nodiscard]] bool  isMain()   const { return api() ? api()->isMain  (_ctx->registry, _ctx->entityId) : false;   }

    void setFovY (float v) const { if (api()) api()->setFovY (_ctx->registry, _ctx->entityId, v); }
    void setNearZ(float v) const { if (api()) api()->setNearZ(_ctx->registry, _ctx->entityId, v); }
    void setFarZ (float v) const { if (api()) api()->setFarZ (_ctx->registry, _ctx->entityId, v); }
    void setMain (bool  v) const { if (api()) api()->setMain (_ctx->registry, _ctx->entityId, v); }

    // ── Position ──────────────────────────────────────────────────────────────

    [[nodiscard]] ScriptVec3 getPosition() const
    {
        float v[3] = {};
        if (api()) api()->getPosition(_ctx->registry, _ctx->entityId, v);
        return { v[0], v[1], v[2] };
    }
    void setPosition(float x, float y, float z) const
    {
        if (api()) api()->setPosition(_ctx->registry, _ctx->entityId, x, y, z);
    }
    void setPosition(ScriptVec3 p) const { setPosition(p.x, p.y, p.z); }

    /// Move by (dx, dy, dz) in world space.
    void translate(float dx, float dy, float dz) const
    {
        if (api()) api()->translate(_ctx->registry, _ctx->entityId, dx, dy, dz);
    }
    void translate(ScriptVec3 d) const { translate(d.x, d.y, d.z); }

    // ── Orientation (degrees) ─────────────────────────────────────────────────

    [[nodiscard]] float getPitch() const { return api() ? api()->getPitch(_ctx->registry, _ctx->entityId) : 0.f;     }
    [[nodiscard]] float getYaw()   const { return api() ? api()->getYaw  (_ctx->registry, _ctx->entityId) : -180.f;  }

    /// Overwrite pitch (degrees). Clamped to [-89, 89] by the bridge.
    void setPitch(float deg) const { if (api()) api()->setPitch(_ctx->registry, _ctx->entityId, deg); }

    /// Overwrite yaw (degrees).
    void setYaw(float deg) const { if (api()) api()->setYaw(_ctx->registry, _ctx->entityId, deg); }

    /// Add a delta to pitch (degrees). Result is clamped to [-89, 89].
    void addPitch(float deg) const { if (api()) api()->addPitch(_ctx->registry, _ctx->entityId, deg); }

    /// Add a delta to yaw (degrees).
    void addYaw(float deg) const { if (api()) api()->addYaw(_ctx->registry, _ctx->entityId, deg); }

private:
    [[nodiscard]] const ScriptCameraAPI* api() const
    {
        return (_ctx && _ctx->cameraApi) ? _ctx->cameraApi : nullptr;
    }
};

class GameObject
{
public:
    explicit GameObject(const ScriptContext* ctx) : m_ctx(ctx) {}

    // ── Identity ──────────────────────────────────────────────────────────────

    EntityID id() const { return m_ctx->entityId; }

    /// Returns the entity's Tag string, or "" if none.
    const char* tag() const
    {
        static char buf[256];
        m_ctx->ecs->getTag(m_ctx->registry, m_ctx->entityId, buf, sizeof(buf));
        return buf;
    }
    void setTag(const char* t) const
    {
        m_ctx->ecs->setTag(m_ctx->registry, m_ctx->entityId, t);
    }

    // ── Component access ──────────────────────────────────────────────────────

    template<typename T, ScriptComponentType ID>
    ComponentRef<T, ID> getComponent() const
    {
        if (!m_ctx->ecs->hasComponent(m_ctx->registry, m_ctx->entityId, static_cast<unsigned int>(ID)))
            return {};

        return { static_cast<T*>(m_ctx->ecs->getComponent(m_ctx->registry, m_ctx->entityId, static_cast<unsigned int>(ID))), m_ctx };
    }

    template<typename T, ScriptComponentType ID>
    ComponentRef<T, ID> addComponent() const
    {
        return { static_cast<T*>(m_ctx->ecs->addComponent(m_ctx->registry, m_ctx->entityId, static_cast<unsigned int>(ID))), m_ctx };
    }

    template<typename T, ScriptComponentType ID>
    bool hasComponent() const
    {
        return m_ctx->ecs->hasComponent(m_ctx->registry, m_ctx->entityId, static_cast<unsigned int>(ID));
    }

    template<typename T, ScriptComponentType ID>
    ComponentRef<T, ID> getOrAddComponent() const
    {
        return hasComponent<T, ID>() ? getComponent<T, ID>() : addComponent<T, ID>();
    }

    // ── Hierarchy ─────────────────────────────────────────────────────────────

    EntityID parent()      const { return m_ctx->ecs->getParent(m_ctx->registry,      m_ctx->entityId); }
    EntityID firstChild()  const { return m_ctx->ecs->getFirstChild(m_ctx->registry,  m_ctx->entityId); }
    EntityID nextSibling() const { return m_ctx->ecs->getNextSibling(m_ctx->registry, m_ctx->entityId); }

    bool hasParent()    const { return parent()     != GCEP_INVALID_ENTITY; }
    bool hasChildren()  const { return firstChild() != GCEP_INVALID_ENTITY; }

    // ── Mesh info ─────────────────────────────────────────────────────────────

    const char* meshPath() const
    {
        static char buf[512];
        m_ctx->ecs->getMeshPath(m_ctx->registry, m_ctx->entityId, buf, sizeof(buf));
        return buf;
    }

    // ── Physics ───────────────────────────────────────────────────────────────

    ScriptVec3 velocity() const
    {
        float v[3]{};
        m_ctx->ecs->getLinearVelocity(m_ctx->registry, m_ctx->entityId, v);
        return { v[0], v[1], v[2] };
    }
    void setVelocity(float x, float y, float z) const
    {
        m_ctx->ecs->setLinearVelocity(m_ctx->registry, m_ctx->entityId, x, y, z);
    }
    void setVelocity(ScriptVec3 v) const { setVelocity(v.x, v.y, v.z); }

    void addForce  (float x, float y, float z) const { m_ctx->ecs->addForce  (m_ctx->registry, m_ctx->entityId, x, y, z); }
    void addImpulse(float x, float y, float z) const { m_ctx->ecs->addImpulse(m_ctx->registry, m_ctx->entityId, x, y, z); }
    void addForce  (ScriptVec3 f) const { addForce  (f.x, f.y, f.z); }
    void addImpulse(ScriptVec3 i) const { addImpulse(i.x, i.y, i.z); }

private:
    const ScriptContext* m_ctx;
};

// ── Convenience aliases — the part that looks like Unity ─────────────────────

using Transform      = ComponentRef<ScriptTransform,          ScriptComponentType::Transform>;
using WorldTransform = ComponentRef<ScriptWorldTransform,     ScriptComponentType::WorldTransform>;
using Physics        = ComponentRef<ScriptPhysicsComponent,   ScriptComponentType::Physics>;
using Camera         = ComponentRef<ScriptCameraComponent,    ScriptComponentType::Camera>;
using PointLight     = ComponentRef<ScriptPointLightComponent,ScriptComponentType::PointLight>;
using SpotLight      = ComponentRef<ScriptSpotLightComponent, ScriptComponentType::SpotLight>;

// Typed getComponent shortcuts so scripts read cleanly
#define GET_TRANSFORM(gameObject)       (gameObject).getComponent<ScriptTransform,          ScriptComponentType::Transform>()
#define GET_WORLD_TRANSFORM(gameObject) (gameObject).getComponent<ScriptWorldTransform,     ScriptComponentType::WorldTransform>()
#define GET_PHYSICS(gameObject)         (gameObject).getComponent<ScriptPhysicsComponent,   ScriptComponentType::Physics>()
#define GET_CAMERA(gameObject)          (gameObject).getComponent<ScriptCameraComponent,    ScriptComponentType::Camera>()
#define GET_POINT_LIGHT(gameObject)     (gameObject).getComponent<ScriptPointLightComponent,ScriptComponentType::PointLight>()
#define GET_SPOT_LIGHT(gameObject)      (gameObject).getComponent<ScriptSpotLightComponent, ScriptComponentType::SpotLight>()

/// @brief Unity-style Input accessor. Mirrors UnityEngine.Input.
/// Only valid when ctx->inputApi != nullptr (camera entity is active).
class Input
{
public:
    explicit Input(const ScriptContext* ctx) : m_ctx(ctx) {}

    [[nodiscard]] bool valid() const { return m_ctx && m_ctx->input && m_ctx->inputApi; }

    // Keys
    [[nodiscard]] bool getKeyDown(gcep::Key key) const
    { return getKeyDown(static_cast<int>(key)); }
    [[nodiscard]] bool getKey(gcep::Key key) const
    { return getKey(static_cast<int>(key)); }
    [[nodiscard]] bool getKeyUp(gcep::Key key) const
    { return getKeyUp(static_cast<int>(key)); }

    // Mouse buttons
    [[nodiscard]] bool getMouseButtonDown(gcep::MouseButton btn) const
    { return getMouseButtonDown(static_cast<int>(btn)); }
    [[nodiscard]] bool getMouseButton(gcep::MouseButton btn) const
    { return getMouseButton(static_cast<int>(btn)); }
    [[nodiscard]] bool getMouseButtonUp(gcep::MouseButton btn) const
    { return getMouseButtonUp(static_cast<int>(btn)); }

    // Callback binding
    void bindKey(const char* name, gcep::Key key, gcep::TriggerOn trigger,
                 void(*callback)(void*), void* userData = nullptr) const
    { bindKey(name, static_cast<int>(key), static_cast<unsigned int>(trigger), callback, userData); }

    // ── Mouse axes ────────────────────────────────────────────────────────────

    /// Screen-space cursor position in pixels.
    [[nodiscard]] ScriptVec3 mousePosition() const
    {
        if (!valid()) return {};
        double x, y;
        m_ctx->inputApi->getMousePos(m_ctx->input, &x, &y);
        return { static_cast<float>(x), static_cast<float>(y), 0.f };
    }

    /// Cursor movement since last frame (pixels).
    [[nodiscard]] ScriptVec3 mouseDelta() const
    {
        if (!valid()) return {};
        double x, y;
        m_ctx->inputApi->getMouseDelta(m_ctx->input, &x, &y);
        return { static_cast<float>(x), static_cast<float>(y), 0.f };
    }

    /// Scroll wheel delta this frame.
    [[nodiscard]] ScriptVec3 mouseScroll() const
    {
        if (!valid()) return {};
        double x, y;
        m_ctx->inputApi->getMouseScroll(m_ctx->input, &x, &y);
        return { static_cast<float>(x), static_cast<float>(y), 0.f };
    }

    // ── Callback bindings ─────────────────────────────────────────────────────
    // triggerOn: 0=Pressed, 1=Held, 2=Released

    void bindKey(const char* name, int key, unsigned int triggerOn, void(*callback)(void*), void* userData = nullptr) const
    {
        if (valid())
            m_ctx->inputApi->addKeyBinding(m_ctx->input, name, key, triggerOn, callback, userData);
    }

    void bindMouseKey(const char* name, int key, unsigned int triggerOn, void(*callback)(void*), void* userData = nullptr) const
    {
        if (valid())
            m_ctx->inputApi->addMouseBinding(m_ctx->input, name, key, triggerOn, callback, userData);
    }

    void unbind(const char* name) const
    {
        if (valid()) m_ctx->inputApi->removeBinding(m_ctx->input, name);
    }

    void clearAllBindings() const
    {
        if (valid()) m_ctx->inputApi->clearBindings(m_ctx->input);
    }

private:
    // ── Keys ──────────────────────────────────────────────────────────────────

    /// True on the exact frame the key went down.
    [[nodiscard]] bool getKeyDown(int key) const
    { return valid() && m_ctx->inputApi->isPressed(m_ctx->input, key); }

    /// True every frame the key is held.
    [[nodiscard]] bool getKey(int key) const
    { return valid() && m_ctx->inputApi->isHeld(m_ctx->input, key); }

    /// True on the exact frame the key came up.
    [[nodiscard]] bool getKeyUp(int key) const
    { return valid() && m_ctx->inputApi->isReleased(m_ctx->input, key); }

    // ── Mouse buttons ─────────────────────────────────────────────────────────

    [[nodiscard]] bool getMouseButtonDown(int btn) const
    { return valid() && m_ctx->inputApi->isMousePressed(m_ctx->input, btn); }

    [[nodiscard]] bool getMouseButton(int btn) const
    { return valid() && m_ctx->inputApi->isMouseHeld(m_ctx->input, btn); }

    [[nodiscard]] bool getMouseButtonUp(int btn) const
    { return valid() && m_ctx->inputApi->isMouseReleased(m_ctx->input, btn); }

private:
    const ScriptContext* m_ctx;
};
