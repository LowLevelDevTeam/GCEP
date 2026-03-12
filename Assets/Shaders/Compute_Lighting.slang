struct GPULight
{
    float3  position;
    uint    type;       // 0 = point, 1 = spot
    float3  color;
    float   intensity;
    float3  direction;
    float   radius;
    float   innerCos;   // full-brightness edge
    float   outerCos;   // zero-brightness edge
    float2  _pad;
};

struct LightMetaUBO
{
    uint  lightCount;
    float _pad0;
    float _pad1;
    float _pad2;
};

struct LightingPushConstants
{
    float4x4 invViewProj;
    float3   cameraPos;
    float    _pad0;
    float2   screenSize;
    float2   _pad1;
};

// ============================================================================
// Bindings
// ============================================================================

[[vk::binding(0, 0)]] StructuredBuffer<GPULight>   lights;
[[vk::binding(1, 0)]] ConstantBuffer<LightMetaUBO> lightMeta;
[[vk::binding(2, 0)]] Texture2D<float4>            sceneColor;
[[vk::binding(3, 0)]] Texture2D<float>             sceneDepth;
[[vk::binding(4, 0)]] RWTexture2D<float4>          litOutput;

[[vk::push_constant]]
LightingPushConstants pc;

// ============================================================================
// Helpers
// ============================================================================

float3 worldPosFromDepth(int2 px, float depth)
{
    float2 uv    = (float2(px) + 0.5f) / pc.screenSize;
    float2 ndc   = uv * 2.0f - 1.0f;
    float4 clip  = float4(ndc, depth, 1.0f);
    float4 worldH = mul(pc.invViewProj, clip);
    return worldH.xyz / worldH.w;
}

float windowing(float dist, float radius)
{
    float r4 = dist / radius;
    r4 = r4 * r4 * r4 * r4;
    float t = saturate(1.0f - r4);
    return t * t;
}

/// Evaluate one light at a world-space surface point.
float3 evaluateLight(GPULight L, float3 pos, float3 N, float3 V)
{
    float3 toLight = L.position - pos;
    float  dist    = length(toLight);
    if (dist >= L.radius) return 0.0f;

    float3 l    = toLight / dist;
    float  att  = (1.0f / max(dist * dist, 0.0001f)) * windowing(dist, L.radius);

    // Spot angular falloff
    float spot = 1.0f;
    if (L.type == 1u)
    {
        float cosA = dot(-l, normalize(L.direction));
        spot = smoothstep(L.outerCos, L.innerCos, cosA);
        if (spot <= 0.0f) return 0.0f;
    }

    float  NdotL   = abs(dot(N, l));
    float3 diffuse = L.color * L.intensity * NdotL * att * spot;

    // Blinn-Phong specular
    float3 H     = normalize(l + V);
    float  NdotH = max(dot(N, H), 0.0f);
    float3 spec  = L.color * L.intensity * pow(NdotH, 32.0f) * att * spot;

    return diffuse + spec;
}

// ============================================================================
// Compute entry point
// ============================================================================

[shader("compute")]
[numthreads(8, 8, 1)]
void compMain(uint3 id : SV_DispatchThreadID)
{
    int2 px = int2(id.xy);
    if (any(px >= int2(pc.screenSize))) return;

    float4 col   = sceneColor.Load(int3(px, 0));
    float  depth = sceneDepth.Load(int3(px, 0));

    if (depth > 1.0f)
    {
        litOutput[px] = col;
        return;
    }

    float3 pos = worldPosFromDepth(px, depth);

    // Reconstruct normal
    float  dR   = sceneDepth.Load(int3(px + int2(1, 0), 0));
    float  dU   = sceneDepth.Load(int3(px + int2(0, 1), 0));
    float3 posR = worldPosFromDepth(px + int2(1, 0), dR);
    float3 posU = worldPosFromDepth(px + int2(0, 1), dU);
    float3 N    = normalize(cross(posU - pos, posR - pos));

    float3 V = normalize(pc.cameraPos - pos);

    float3 radiance = 0.0f;
    for (uint i = 0u; i < lightMeta.lightCount; ++i)
        radiance += evaluateLight(lights[i], pos, N, V);

    litOutput[px] = float4(col.rgb + radiance, col.a);
}
