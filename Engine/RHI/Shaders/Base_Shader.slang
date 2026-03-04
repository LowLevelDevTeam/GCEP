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
    float  _pad0;
    float3 lightColor;
    float  _pad1;
    float3 ambientColor;
    float  _pad2;
    float3 cameraPos;
    float  _pad3;
};

[[vk::binding(0, 0)]]
StructuredBuffer<GPUMeshData> meshData;

struct VSOutput
{
    float4 pos       : SV_Position;
    float3 worldPos  : WORLDPOS;
    float3 normal    : NORMAL;
    float2 uv        : TEXCOORD0;
    uint   texIndex  : TEXINDEX;
};

struct PushConstants
{
    float4x4 viewProj;
    float shininess;
    float _pad[3];
};

[[vk::push_constant]]
PushConstants pc;

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

    return output;
}

[[vk::binding(0, 1)]]
Sampler2D textures[];

[[vk::binding(0, 2)]]
ConstantBuffer<SceneUBO> scene;

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target
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
    float  spec    = pow(max(dot(N, H), 0.0f), pc.shininess);
    float3 specular = spec * scene.lightColor;

    float3 lighting = scene.ambientColor + diffuse + specular;

    return float4(albedo.rgb * lighting, albedo.a);
}