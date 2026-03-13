/// @file Gizmo_LightSprite.slang
/// @brief Additive-blended light sprite billboard.
///
/// The vertex shader generates a screen-aligned quad from a single world-space
/// position (no vertex buffer). The fragment shader draws a smooth radial
/// falloff glow.  Additive blending makes the sprite stack and glow correctly
/// even in empty space.
///
/// Push constants carry everything: world position, light color, and the
/// half-size of the sprite in world units.
///
/// Draw call: 6 vertices, 0 indices, no vertex buffer.
/// Pipeline:  - depth test ON, depth write OFF
///            - additive blend (src=One, dst=One)
///            - two color attachments (scene color + motion; sprite writes 0
///              to motion so TAA is unaffected)

struct SpritePushConstants
{
    float4x4 viewProj;      ///< Current (unjittered) view-projection matrix.
    float3   worldPos;      ///< World-space centre of the light.
    float    halfSize;      ///< World-space half-width/height of the sprite.
    float3   color;         ///< Linear-space light color (pre-multiplied by a
    float    _pad;
    float3   cameraRight;   ///< Camera right vector (world space) for billboarding.
    float    _pad2;
    float3   cameraUp;      ///< Camera up vector (world space) for billboarding.
    float    _pad3;
};

[[vk::push_constant]]
SpritePushConstants pc;

// ─── Vertex shader ───────────────────────────────────────────────────────────

struct VSOut
{
    float4 pos    : SV_Position;
    float2 uv;    ///< [-1,1] within the quad; used for the radial gradient.
};

/// Unit quad corners in [-1, 1] per-axis.  Two triangles, CCW winding.
static const float2 kQuadCorners[6] =
{
    float2(-1.0f,  1.0f),
    float2(-1.0f, -1.0f),
    float2( 1.0f, -1.0f),
    float2(-1.0f,  1.0f),
    float2( 1.0f, -1.0f),
    float2( 1.0f,  1.0f),
};

[shader("vertex")]
VSOut vertMain(uint vertexIndex : SV_VertexID)
{
    VSOut output;

    float2 corner = kQuadCorners[vertexIndex];

    // Billboard: move the corner in camera-space right/up directions (world space).
    float3 worldCorner = pc.worldPos
                       + pc.cameraRight * (corner.x * pc.halfSize)
                       + pc.cameraUp    * (corner.y * pc.halfSize);

    output.pos = mul(pc.viewProj, float4(worldCorner, 1.0f));
    output.uv  = corner;   // [-1,1]

    return output;
}

// ─── Fragment shader ─────────────────────────────────────────────────────────

struct FSOut
{
    float4 sceneColor  : SV_Target0;   ///< Additive contribution to scene color.
    float4 motionVec   : SV_Target1;   ///< Zero – sprites have no motion.
};

[shader("fragment")]
FSOut fragMain(VSOut input)
{
    FSOut output;

    // Smooth radial falloff:  1 at center, 0 at edge, C1 continuous.
    float dist    = length(input.uv);              // 0 → 1 (corner = sqrt(2))
    float falloff = 1.0f - smoothstep(0.0f, 1.0f, dist);
    falloff       = falloff * falloff;             // square for a tighter hot spot

    // Discard fully transparent fragments to avoid unnecessary blending.
    if (falloff < 0.001f)
        discard;

    output.sceneColor = float4(pc.color * falloff, falloff);
    output.motionVec  = float4(0.0f, 0.0f, 0.0f, 0.0f);
    return output;
}
