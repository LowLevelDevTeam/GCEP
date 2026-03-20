#pragma once
/// @file script_components.hpp
/// @brief Plain-data mirrors of engine components.
///        Layouts MUST stay byte-for-byte identical to the engine structs.
///        No engine headers. No std::string. No vtables.

// Primitives

struct ScriptVec3 { float x, y, z; };
struct ScriptQuat { float w, x, y, z; }; // matches gcep::Quaternion(w,x,y,z)

// Transform - mirrors gcep::ECS::Transform
struct alignas(16) ScriptTransform
{
    alignas(16) ScriptVec3 position     { 0.f, 0.f, 0.f };
    alignas(16) ScriptVec3 scale        { 1.f, 1.f, 1.f };
    alignas(16) ScriptQuat rotation     { 1.f, 0.f, 0.f, 0.f };
    alignas(16) ScriptVec3 eulerRadians { 0.f, 0.f, 0.f };
};

// WorldTransform, mirrors gcep::ECS::WorldTransform
struct alignas(16) ScriptWorldTransform
{
    alignas(16) ScriptVec3 position     { 0.f, 0.f, 0.f };
    alignas(16) ScriptVec3 scale        { 1.f, 1.f, 1.f };
    alignas(16) ScriptQuat rotation     { 1.f, 0.f, 0.f, 0.f };
    alignas(16) ScriptVec3 eulerRadians { 0.f, 0.f, 0.f };
};

// PhysicsComponent, mirrors gcep::ECS::PhysicsComponent
enum class ScriptShapeType  : unsigned int { Cube = 0, Sphere, Cylinder, Capsule };
enum class ScriptMotionType : unsigned int { Static = 0, Dynamic, Kinematic };

struct ScriptPhysicsComponent
{
    bool             isTrigger  = false;
    ScriptShapeType  shapeType  = ScriptShapeType::Cube;
    ScriptMotionType motionType = ScriptMotionType::Static;
};

// CameraComponent, mirrors gcep::ECS::CameraComponent
struct ScriptCameraComponent
{
    float fovYDeg     = 60.f;
    float nearZ       = 0.01f;
    float farZ        = 1000.f;
    bool  isMainCamera = true;
};

// PointLightComponent, mirrors gcep::ECS::PointLightComponent
struct ScriptPointLightComponent
{
    ScriptVec3 position  { 0.f, 0.f, 0.f };
    ScriptVec3 color     { 1.f, 1.f, 1.f };
    float      intensity = 1.f;
    float      radius    = 10.f;
};

// SpotLightComponent, mirrors gcep::ECS::SpotLightComponent
struct ScriptSpotLightComponent
{
    ScriptVec3 position       {  0.f,  1.f, 0.f };
    ScriptVec3 direction      {  0.f, -1.f, 0.f };
    ScriptVec3 color          {  1.f,  1.f, 1.f };
    float      intensity      = 1.f;
    float      radius         = 20.f;
    float      innerCutoffDeg = 15.f;
    float      outerCutoffDeg = 30.f;
    bool       showGizmo      = true;
};

// HierarchyComponent, mirrors gcep::ECS::HierarchyComponent
struct ScriptHierarchyComponent
{
    unsigned int parent      = 0xffffffff;
    unsigned int firstChild  = 0xffffffff;
    unsigned int nextSibling = 0xffffffff;
    unsigned int prevSibling = 0xffffffff;
};
