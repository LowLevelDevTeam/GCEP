/// @file Debug_Physics.slang
/// @brief Minimal physics debug draw shader.
///
/// Used by two pipelines that share this module:
///   - eLineList      : wireframe colliders / contacts / velocities
///   - eTriangleList  : solid or translucent shape fills
///
/// All vertex positions arrive already in world space (CPU-transformed by
/// PhysicsDebugRenderer::DrawGeometry). The vertex shader applies only viewProj.
///
/// Color is packed as R8G8B8A8_UNORM in the vertex buffer — Vulkan unpacks it
/// automatically to a float4 in [0,1] before it reaches the shader.
///
/// Two color attachments (matches the offscreen pass):
///   Target 0 : scene color
///   Target 1 : motion vectors — always zero for debug geometry

struct DebugPushConstants
{
    float4x4 viewProj;
};
[[vk::push_constant]]
DebugPushConstants pc;

// ─── Vertex shader ────────────────────────────────────────────────────────────

struct VSIn
{
    float3 position : POSITION; // location 0 — R32G32B32_SFLOAT
    float4 color    : COLOR0;   // location 1 — R8G8B8A8_UNORM (auto-unpacked to [0,1])
};

struct VSOut
{
    float4 pos   : SV_Position;
    float4 color : COLOR0;
};

[shader("vertex")]
VSOut vertMain(VSIn i)
{
    VSOut o;
    o.pos   = mul(pc.viewProj, float4(i.position, 1.0f));
    o.color = i.color;
    return o;
}

// ─── Fragment shader ──────────────────────────────────────────────────────────

struct FSOut
{
    float4 sceneColor : SV_Target0;
    float4 motionVec  : SV_Target1;
};

[shader("fragment")]
FSOut fragMain(VSOut i)
{
    FSOut o;
    o.sceneColor = i.color;
    o.motionVec  = float4(0.0f, 0.0f, 0.0f, 0.0f);
    return o;
}
