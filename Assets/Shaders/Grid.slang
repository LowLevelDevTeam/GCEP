struct SceneUBO
{
    float3 lightDir;
    float  _pad0;
    float3 lightColor;
    float  _pad1;
    float3 ambientColor;
    float  _pad2;
    float3 cameraPos;
    float  _pad3;
};

[[vk::binding(0, 0)]]
ConstantBuffer<SceneUBO> scene;

struct GridPC
{
    float4x4 invView;
    float4x4 invProj;
    float4x4 viewProj;
    float    cellSize;      // default: 1.0
    float    thickEvery;    // default: 10.0
    float    fadeDistance;  // default: 100.0
    float    lineWidth;     // default: 1.0
};

[[vk::push_constant]]
GridPC pc;

static const float4 kThinColor  = float4(0.50, 0.50, 0.50, 0.40);
static const float4 kThickColor = float4(0.75, 0.75, 0.75, 0.80);
static const float4 kXAxisColor = float4(0.90, 0.25, 0.25, 1.00);
static const float4 kYAxisColor = float4(0.25, 0.90, 0.25, 1.00);

struct VSOutput
{
    float4 clipPos   : SV_Position;
    float3 nearPoint : TEXCOORD0;
    float3 farPoint  : TEXCOORD1;
};

float3 unproject(float2 ndc, float z)
{
    float4 view  = mul(pc.invProj, float4(ndc.x, ndc.y, z, 1.0));
    view        /= view.w;
    float4 world = mul(pc.invView, view);
    return world.xyz;
}

[shader("vertex")]
VSOutput vertMain(uint vid : SV_VertexID)
{
    const float2 pos[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };

    float2 p = pos[vid];

    VSOutput o;
    o.clipPos   = float4(p, 0.0, 1.0);
    o.nearPoint = unproject(p, 0.0);
    o.farPoint  = unproject(p, 1.0);
    return o;
}

struct FSOutput
{
    float4 color : SV_Target;
    float  depth : SV_Depth;
};

float gridLine(float2 worldXZ, float cellSz)
{
    float2 uv   = worldXZ / cellSz;
    float2 fw   = max(fwidth(uv), 1e-4);
    float2 edge = abs(frac(uv - 0.5) - 0.5) / fw;
    float2 line2 = 1.0 - clamp(edge - pc.lineWidth, 0.0, 1.0);
    return max(line2.x, line2.y);
}

[shader("fragment")]
FSOutput fragMain(VSOutput i)
{
    float denom = i.farPoint.z - i.nearPoint.z;
    if (abs(denom) < 1e-5) discard;

    float t = -i.nearPoint.z / denom;
    if (t < 0.0) discard;

    float3 worldPos = i.nearPoint + t * (i.farPoint - i.nearPoint);

    float thin  = gridLine(worldPos.xy, pc.cellSize);
    float thick = gridLine(worldPos.xy, pc.cellSize * pc.thickEvery);

    float4 color = lerp(
        kThinColor  * thin,
        kThickColor * thick,
        thick
    );

    float2 fw    = max(fwidth(worldPos.xy), 1e-4);
    float  xAxis = 1.0 - clamp(abs(worldPos.y) / (fw.y * pc.lineWidth * 3.0), 0.0, 1.0);
    float  zAxis = 1.0 - clamp(abs(worldPos.x) / (fw.x * pc.lineWidth * 3.0), 0.0, 1.0);

    float dist = length(worldPos.xy - scene.cameraPos.xy);

    color = lerp(color, kXAxisColor, xAxis * kXAxisColor.a);
    color = lerp(color, kYAxisColor, zAxis * kYAxisColor.a);

    float fade = 1.0 - clamp(dist / pc.fadeDistance, 0.0, 1.0);
    fade      *= fade;
    color.a   *= fade;

    if (color.a < 0.001) discard;

    float4 clip = mul(pc.viewProj, float4(worldPos, 1.0));

    FSOutput o;
    o.color = color;
    o.depth = clip.z / clip.w;
    return o;
}