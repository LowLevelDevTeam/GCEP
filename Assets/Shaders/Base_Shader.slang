struct VSInput
{
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float2 uv     : TEXCOORD0;
    uint   instanceIndex : SV_InstanceID;
    uint   baseInstance  : SV_StartInstanceLocation;
};

struct GPUMeshData
{
    float4x4 transform;
    uint     textureIndex;
    uint     firstIndex;
    uint     indexCount;
    uint     vertexOffset;
    float3   aabbMin;
    float    _pad0;
    float3   aabbMax;
    float    _pad1;
};

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
StructuredBuffer<GPUMeshData> meshData;

[[vk::binding(0, 1)]]
Sampler2D textures[];

[[vk::binding(0, 2)]]
ConstantBuffer<SceneUBO> scene;

struct PushConstants
{
    float4x4 viewProj;         // current frame (jittered)
    float4x4 prevViewProj;     // previous frame (unjittered)
};

[[vk::push_constant]]
PushConstants pc;

struct VSOutput
{
    float4 pos       : SV_Position;
    float3 worldPos  : WORLDPOS;
    float3 normal    : NORMAL;
    float2 uv        : TEXCOORD0;
    uint   texIndex  : TEXINDEX;
    float4 currentClip : CURRENT_CLIP;
    float4 prevClip    : PREV_CLIP;
};

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    GPUMeshData data = meshData[input.baseInstance];
    float4 worldPos4 = mul(data.transform, float4(input.pos, 1.0));

    VSOutput output;
    output.pos      = mul(pc.viewProj, worldPos4);
    output.worldPos = worldPos4.xyz;
    output.uv       = input.uv;
    output.texIndex = data.textureIndex;

    float3x3 normalMatrix = (float3x3)transpose(data.transform);
    output.normal = normalize(mul(normalMatrix, input.normal));

    output.currentClip = mul(pc.viewProj, worldPos4);
    output.prevClip    = mul(pc.prevViewProj,  worldPos4);

    return output;
}

struct FSOutput
{
    float4 color        : SV_Target0;
    float2 motionVector : SV_Target1;
};

[shader("fragment")]
FSOutput fragMain(VSOutput input)
{
    float4 albedo = textures[input.texIndex].Sample(input.uv);

    float3 N = normalize(input.normal);
    float3 L = normalize(scene.lightDir);

    // Diffuse - Lambert
    float  diff    = max(dot(N, L), 0.0f);
    float3 diffuse = diff * scene.lightColor;

    // Specular - Blinn-Phong
    float3 V       = normalize(scene.cameraPos - input.worldPos);
    float3 H       = normalize(L + V);
    float  spec    = pow(max(dot(N, H), 0.0f), 64.0f);
    float3 specular = spec * scene.lightColor;

    float3 lighting = scene.ambientColor + diffuse + specular;

    float2 ndcCurrent  = input.currentClip.xy / input.currentClip.w;
    float2 ndcPrevious = input.prevClip.xy    / input.prevClip.w;
    float2 motion      = (ndcCurrent - ndcPrevious) * 0.5f;

    FSOutput o;
    o.color        = float4(albedo.rgb * lighting, albedo.a);
    o.motionVector = motion;
    return o;
}