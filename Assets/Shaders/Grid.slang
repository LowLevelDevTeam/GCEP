struct SceneUBO
{
    float3 lightDir;
    float  cellSize;      // default: 1.0
    float3 lightColor;
    float  thickEvery;    // default: 10.0
    float3 ambientColor;
    float  fadeDistance;  // default: 100.0
    float3 cameraPos;
    float  lineWidth;     // default: 1.0
};

[[vk::binding(0, 0)]]
ConstantBuffer<SceneUBO> scene;

struct GridPC
{
    float4x4 invViewProj;
    float4x4 viewProj;
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
    float2 ndc : TEXCOORD0;
};

float3 unproject(float2 ndc, float z)
{
    float4 world  = mul(pc.invViewProj, float4(ndc.x, ndc.y, z, 1.0));
    return world.xyz / world.w;
}

[shader("vertex")]
VSOutput vertMain(uint vid : SV_VertexID)
{
    const float2 pos[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };

    VSOutput o;
    o.clipPos = float4(pos[vid], 0.0, 1.0);
    o.ndc     = pos[vid];
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
    float2 line2 = 1.0 - clamp(edge - scene.lineWidth, 0.0, 1.0);
    return max(line2.x, line2.y);
}

[shader("fragment")]
FSOutput fragMain(VSOutput i)
{
    float2 ndc = i.ndc;  // linearly interpolated NDC — this IS correct

    float3 nearPoint = unproject(ndc, 0.0);
    float3 farPoint  = unproject(ndc, 1.0);

    float denom = farPoint.z - nearPoint.z;
    if (abs(denom) < 1e-5) discard;

    float t = -nearPoint.z / denom;
    if (t < 0.0) discard;

    float3 worldPos = nearPoint + t * (farPoint - nearPoint);

    float thin  = gridLine(worldPos.xy, scene.cellSize);
    float thick = gridLine(worldPos.xy, scene.cellSize * scene.thickEvery);

    float4 color = lerp(
        kThinColor  * thin,
        kThickColor * thick,
        thick
    );

    float2 fw    = max(fwidth(worldPos.xy), 1e-4);
    float  xAxis = 1.0 - clamp(abs(worldPos.y) / (fw.y * scene.lineWidth * 3.0), 0.0, 1.0);
    float  zAxis = 1.0 - clamp(abs(worldPos.x) / (fw.x * scene.lineWidth * 3.0), 0.0, 1.0);

    float dist = length(worldPos.xy - scene.cameraPos.xy);

    color = lerp(color, kXAxisColor, xAxis * kXAxisColor.a);
    color = lerp(color, kYAxisColor, zAxis * kYAxisColor.a);

    float fade = 1.0 - clamp(dist / scene.fadeDistance, 0.0, 1.0);
    fade      *= fade;
    color.a   *= fade;

    if (color.a < 0.001) discard;

    float4 clip = mul(pc.viewProj, float4(worldPos, 1.0));

    FSOutput o;
    o.color = color;
    o.depth = clip.z / clip.w;
    return o;
}