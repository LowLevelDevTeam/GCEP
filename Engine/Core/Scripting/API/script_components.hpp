#pragma once

/// @file script_components.hpp
/// @brief Plain-data mirrors of engine components exposed to the scripting layer.
///
/// Every struct here is a byte-for-byte replica of its engine-side counterpart.
/// This contract is critical: the ECS bridge casts raw component pointers directly
/// to these types, so field order, size, and alignment MUST remain identical to
/// the engine structs at all times. Never add vtables, @c std::string, or any
/// heap-owning member.

// ── Primitives ────────────────────────────────────────────────────────────────

/// @brief Three-component floating-point vector (x, y, z).
struct ScriptVec3 { float x, y, z; };

/// @brief Unit quaternion (w, x, y, z).
struct ScriptQuat { float w, x, y, z; };

// ── Transform ─────────────────────────────────────────────────────────────────

/// @brief Local-space transform. Mirrors @c gcep::ECS::Transform.
///
/// Position and scale default to the identity transform; rotation defaults to
/// the identity quaternion. Alignas(16) matches the engine struct's layout so
/// that the ECS bridge can cast component pointers directly to this type.
///
/// @note Do not write the rotation field directly from scripts. Use the
///       rotation helpers on @c ComponentRef<ScriptTransform, ...> so the
///       engine keeps its quaternion and Euler display-cache in sync.
struct alignas(16) ScriptTransform
{
    alignas(16) ScriptVec3 position { 0.f, 0.f, 0.f }; ///< Local-space position.
    alignas(16) ScriptVec3 scale    { 1.f, 1.f, 1.f }; ///< Local-space scale (uniform 1 by default).
    alignas(16) ScriptQuat rotation { 1.f, 0.f, 0.f, 0.f }; ///< Local-space orientation as a unit quaternion.
};

// ── WorldTransform ────────────────────────────────────────────────────────────

/// @brief World-space transform. Mirrors @c gcep::ECS::WorldTransform.
///
/// Computed and owned by the engine; treat as read-only from scripts.
/// Layout identical to @c ScriptTransform.
struct alignas(16) ScriptWorldTransform
{
    alignas(16) ScriptVec3 position { 0.f, 0.f, 0.f }; ///< World-space position.
    alignas(16) ScriptVec3 scale    { 1.f, 1.f, 1.f }; ///< World-space scale.
    alignas(16) ScriptQuat rotation { 1.f, 0.f, 0.f, 0.f }; ///< World-space orientation as a unit quaternion.
};

// ── PhysicsComponent ──────────────────────────────────────────────────────────

/// @brief Collision shape primitive. Maps to Jolt shape types on the engine side.
enum class ScriptShapeType : unsigned int
{
    Cube     = 0, ///< Axis-aligned box collider.
    Sphere   = 1, ///< Sphere collider.
    Cylinder = 2, ///< Cylinder collider.
    Capsule  = 3, ///< Capsule collider (cylinder + hemi-spheres).
};

/// @brief Rigid-body simulation mode.
enum class ScriptMotionType : unsigned int
{
    Static    = 0, ///< Immovable; only participates in collision response.
    Dynamic   = 1, ///< Fully simulated; affected by forces and gravity.
    Kinematic = 2, ///< Moved by script; collides but ignores forces.
};

/// @brief Physics body descriptor. Mirrors @c gcep::ECS::PhysicsComponent.
struct ScriptPhysicsComponent
{
    bool             isTrigger  = false;                   ///< When @c true the body fires overlap events but does not resolve collisions.
    ScriptShapeType  shapeType  = ScriptShapeType::Cube;   ///< Primitive shape used for collision detection.
    ScriptMotionType motionType = ScriptMotionType::Static; ///< Simulation mode (Static / Dynamic / Kinematic).
};

// ── CameraComponent ───────────────────────────────────────────────────────────

/// @brief Perspective camera descriptor. Mirrors @c gcep::ECS::CameraComponent.
///
/// All reads and writes should go through the @c ComponentRef<ScriptCameraComponent, ...>
/// accessor methods, which route via @c ScriptCameraAPI so the engine's
/// projection and view matrices remain authoritative.
///
/// @warning Do NOT write to fields directly — always use the ComponentRef helpers.
struct ScriptCameraComponent
{
    float fovYDeg      = 60.f;   ///< Vertical field of view in degrees.
    float nearZ        = 0.01f;  ///< Near clip plane distance (world units).
    float farZ         = 1000.f; ///< Far clip plane distance (world units).
    bool  isMainCamera = true;   ///< Whether this camera drives the primary viewport.

    /// @brief World-space eye position.
    /// Write via @c ComponentRef::setPosition() / @c translate() — not directly.
    ScriptVec3 position = { 0.f, 0.f, 0.f };

    /// @brief Vertical rotation in degrees. Clamped to ±89° by the bridge.
    float pitch = 0.f;

    /// @brief Horizontal rotation in degrees (0° = negative-X axis).
    float yaw   = -180.f;
};

// ── PointLightComponent ───────────────────────────────────────────────────────

/// @brief Omnidirectional point-light descriptor. Mirrors @c gcep::ECS::PointLightComponent.
struct ScriptPointLightComponent
{
    ScriptVec3 position  { 0.f, 0.f, 0.f }; ///< World-space light origin.
    ScriptVec3 color     { 1.f, 1.f, 1.f }; ///< Linear-space RGB colour (1,1,1 = white).
    float      intensity = 1.f;             ///< Radiant intensity scalar.
    float      radius    = 10.f;            ///< Attenuation cutoff radius (world units).
};

// ── SpotLightComponent ────────────────────────────────────────────────────────

/// @brief Cone spot-light descriptor. Mirrors @c gcep::ECS::SpotLightComponent.
struct ScriptSpotLightComponent
{
    ScriptVec3 position       {  0.f,  1.f, 0.f }; ///< World-space light origin.
    ScriptVec3 direction      {  0.f, -1.f, 0.f }; ///< Normalised world-space cone axis.
    ScriptVec3 color          {  1.f,  1.f, 1.f }; ///< Linear-space RGB colour.
    float      intensity      = 1.f;               ///< Radiant intensity scalar.
    float      radius         = 20.f;              ///< Attenuation cutoff radius (world units).
    float      innerCutoffDeg = 15.f;              ///< Full-intensity inner cone half-angle (degrees).
    float      outerCutoffDeg = 30.f;              ///< Penumbra outer cone half-angle (degrees); must be > @c innerCutoffDeg.
    bool       showGizmo      = true;              ///< Show the cone gizmo in the editor viewport.
};

// ── HierarchyComponent ────────────────────────────────────────────────────────

/// @brief Scene-graph linkage node. Mirrors @c gcep::ECS::HierarchyComponent.
///
/// Forms a singly-linked sibling list anchored at the parent's @c firstChild.
/// Sentinel value @c 0xffffffff (@c GCEP_INVALID_ENTITY) means "no entity".
struct ScriptHierarchyComponent
{
    unsigned int parent      = 0xffffffff; ///< Parent entity ID, or @c GCEP_INVALID_ENTITY if root.
    unsigned int firstChild  = 0xffffffff; ///< First child entity ID, or @c GCEP_INVALID_ENTITY if leaf.
    unsigned int nextSibling = 0xffffffff; ///< Next sibling entity ID in the parent's child list.
    unsigned int prevSibling = 0xffffffff; ///< Previous sibling entity ID in the parent's child list.
};
