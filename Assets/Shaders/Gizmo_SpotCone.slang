/// @file Gizmo_SpotCone.slang
/// @brief Spot-light cone trajectory gizmo.
///
/// Renders a wireframe cone that shows exactly where a spot light shines:
///   - 4 edge lines radiating from the apex to the base circle.
///   - A 32-segment circle at the outer-cutoff distance (base of the cone).
///   - An inner circle at the same distance showing the inner cutoff angle.
///   - A centre-axis line (apex → base centre).
///
/// The entire cone geometry is procedurally generated in the vertex shader
/// from push constants — no vertex buffer is needed.
///
/// Vertex count per cone = 4*2 + 32*2 + 32*2 + 2 = 138 vertices (69 lines).
/// Draw with vk::PrimitiveTopology::eLineList, 138 vertices per spot light.
///
/// Pipeline state:
///   - Line topology, depth test ON, depth write OFF
///   - Alpha blend (src=SrcAlpha, dst=OneMinusSrcAlpha)
///   - Two color attachments (scene color + motion; cone writes 0 to motion)

static const uint  kEdgeLines   = 4u;
static const uint  kCircleSegs  = 32u;
// vertex budget per cone:
//   4 edge lines     = 4*2 = 8
//   outer circle     = 32*2 = 64
//   inner circle     = 32*2 = 64
//   axis line        = 2
//   total            = 138
static const uint  kVertsPerCone = kEdgeLines * 2u + kCircleSegs * 2u + kCircleSegs * 2u + 2u;

static const float kPi = 3.14159265358979f;

struct ConePushConstants
{
    float4x4 viewProj;
    float3   apex;        // world-space tip of the cone (= light position)
    float    length;      // distance along axis to draw the cone
    float3   direction;   // normalised world-space axis of the cone
    float    outerCos;    // cos(outerCutoffDeg) — outer circle radius factor
    float3   color;       // gizmo line colour
    float    innerCos;    // cos(innerCutoffDeg)
    float    alpha;       // overall transparency (0=invisible, 1=opaque)
    uint     visible;     // 1 = draw cone, 0 = suppress (all verts collapse to origin)
    float2   _pad;
};

[[vk::push_constant]]
ConePushConstants pc;

// ─── Helpers ─────────────────────────────────────────────────────────────────

/// Build an orthonormal basis (right, up) perpendicular to `dir`.
void buildBasis(float3 dir, out float3 right, out float3 up)
{
    float3 ref = (abs(dir.x) < 0.9f) ? float3(1.0f, 0.0f, 0.0f)
                                      : float3(0.0f, 1.0f, 0.0f);
    right = normalize(cross(dir, ref));
    up    = cross(right, dir);
}

/// Point on the base circle of the outer cone at angle `theta`.
float3 outerCirclePoint(float theta)
{
    float3 right, up;
    buildBasis(pc.direction, right, up);

    // radius at the cone base = length * tan(acos(outerCos))
    float sinOuter = sqrt(max(0.0f, 1.0f - pc.outerCos * pc.outerCos));
    float r = pc.length * (sinOuter / max(pc.outerCos, 0.0001f));

    float3 centre = pc.apex + pc.direction * pc.length;
    return centre + (cos(theta) * right + sin(theta) * up) * r;
}

/// Point on the inner circle.
float3 innerCirclePoint(float theta)
{
    float3 right, up;
    buildBasis(pc.direction, right, up);

    float sinInner = sqrt(max(0.0f, 1.0f - pc.innerCos * pc.innerCos));
    float r = pc.length * (sinInner / max(pc.innerCos, 0.0001f));

    float3 centre = pc.apex + pc.direction * pc.length;
    return centre + (cos(theta) * right + sin(theta) * up) * r;
}

// ─── Vertex shader ───────────────────────────────────────────────────────────

struct VSOut
{
    float4 pos   : SV_Position;
    float4 color : COLOR0;
};

[shader("vertex")]
VSOut vertMain(uint vid : SV_VertexID)
{
    VSOut o;
    o.color = float4(pc.color, pc.alpha);

    // If the gizmo is toggled off, collapse every vertex to a degenerate
    // clip-space position outside the viewport so the rasterizer discards
    // every line segment without touching any other state.
    if (pc.visible == 0u)
    {
        o.pos = float4(2.0f, 2.0f, 2.0f, 1.0f); // outside NDC cube
        return o;
    }

    float3 worldPos;
    uint   v = vid;

    // ── Segment 1: 4 edge lines (apex → outer circle at 4 cardinal angles) ──
    // Each line = 2 verts: even = apex, odd = rim point
    if (v < kEdgeLines * 2u)
    {
        uint lineIdx = v / 2u;
        uint isEnd   = v & 1u;
        float theta  = float(lineIdx) * (kPi * 0.5f); // 0, 90, 180, 270 deg
        worldPos = (isEnd == 0u) ? pc.apex : outerCirclePoint(theta);
    }
    else
    {
        v -= kEdgeLines * 2u;

        // ── Segment 2: outer circle ──────────────────────────────────────────
        if (v < kCircleSegs * 2u)
        {
            uint segIdx  = v / 2u;
            uint isEnd   = v & 1u;
            float t0 = float(segIdx)       * (2.0f * kPi / float(kCircleSegs));
            float t1 = float(segIdx + 1u)  * (2.0f * kPi / float(kCircleSegs));
            worldPos = (isEnd == 0u) ? outerCirclePoint(t0) : outerCirclePoint(t1);
        }
        else
        {
            v -= kCircleSegs * 2u;

            // ── Segment 3: inner circle (dimmer) ─────────────────────────────
            if (v < kCircleSegs * 2u)
            {
                uint segIdx = v / 2u;
                uint isEnd  = v & 1u;
                float t0 = float(segIdx)      * (2.0f * kPi / float(kCircleSegs));
                float t1 = float(segIdx + 1u) * (2.0f * kPi / float(kCircleSegs));
                worldPos = (isEnd == 0u) ? innerCirclePoint(t0) : innerCirclePoint(t1);
                // Tint inner circle slightly dimmer to distinguish it
                o.color.rgb *= 0.55f;
            }
            else
            {
                v -= kCircleSegs * 2u;

                // ── Segment 4: axis line ─────────────────────────────────────
                // v == 0 → apex,  v == 1 → base centre
                float3 centre = pc.apex + pc.direction * pc.length;
                worldPos = (v == 0u) ? pc.apex : centre;
                o.color.a *= 0.6f; // axis line is more transparent
            }
        }
    }

    o.pos = mul(pc.viewProj, float4(worldPos, 1.0f));
    return o;
}

// ─── Fragment shader ─────────────────────────────────────────────────────────

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
