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

using Transform      = ComponentRef<ScriptTransform,       ScriptComponentType::Transform>;
using WorldTransform = ComponentRef<ScriptWorldTransform,  ScriptComponentType::WorldTransform>;
using Physics        = ComponentRef<ScriptPhysicsComponent,ScriptComponentType::Physics>;
using Camera         = ComponentRef<ScriptCameraComponent, ScriptComponentType::Camera>;
using PointLight     = ComponentRef<ScriptPointLightComponent, ScriptComponentType::PointLight>;
using SpotLight      = ComponentRef<ScriptSpotLightComponent,  ScriptComponentType::SpotLight>;

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
