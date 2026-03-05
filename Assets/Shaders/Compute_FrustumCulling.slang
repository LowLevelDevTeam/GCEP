struct ObjectData
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

struct DrawIndexedIndirectCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;
};

struct FrustumPlanes
{
    float4 planes[6];
    uint   objectCount;
};

[[vk::binding(0, 0)]] StructuredBuffer<ObjectData> objects;
[[vk::binding(1, 0)]] RWStructuredBuffer<DrawIndexedIndirectCommand> commands;
[[vk::binding(2, 0)]] RWStructuredBuffer<uint> drawCount;

[[vk::push_constant]] FrustumPlanes frustum;

bool aabbInFrustum(float3 min, float3 max)
{
    for (int i = 0; i < 6; ++i)
    {
        float3 n = frustum.planes[i].xyz;
        float  d = frustum.planes[i].w;
        float3 pos = float3(
            n.x > 0.0f ? max.x : min.x,
            n.y > 0.0f ? max.y : min.y,
            n.z > 0.0f ? max.z : min.z
        );
        if (dot(n, pos) + d < 0.0f) return false;
    }
    return true;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void compMain(uint3 dispatchID : SV_DispatchThreadID)
{
    uint idx = dispatchID.x;
    if (idx >= frustum.objectCount) return;

    ObjectData obj = objects[idx];

    // Transform all 8 AABB corners through the model matrix,
    // then take the new world-space min/max
    float3 corners[8] = {
        float3(obj.aabbMin.x, obj.aabbMin.y, obj.aabbMin.z),
        float3(obj.aabbMax.x, obj.aabbMin.y, obj.aabbMin.z),
        float3(obj.aabbMin.x, obj.aabbMax.y, obj.aabbMin.z),
        float3(obj.aabbMax.x, obj.aabbMax.y, obj.aabbMin.z),
        float3(obj.aabbMin.x, obj.aabbMin.y, obj.aabbMax.z),
        float3(obj.aabbMax.x, obj.aabbMin.y, obj.aabbMax.z),
        float3(obj.aabbMin.x, obj.aabbMax.y, obj.aabbMax.z),
        float3(obj.aabbMax.x, obj.aabbMax.y, obj.aabbMax.z),
    };

    float3 worldMin = float3( 1e30f,  1e30f,  1e30f);
    float3 worldMax = float3(-1e30f, -1e30f, -1e30f);

    for (int i = 0; i < 8; ++i)
    {
        float3 worldCorner = mul(obj.transform, float4(corners[i], 1.0f)).xyz;
        worldMin = min(worldMin, worldCorner);
        worldMax = max(worldMax, worldCorner);
    }

    if (!aabbInFrustum(worldMin, worldMax)) return;

    uint slot;
    InterlockedAdd(drawCount[0], 1, slot);

    commands[slot].indexCount    = obj.indexCount;
    commands[slot].instanceCount = 1;
    commands[slot].firstIndex    = obj.firstIndex;
    commands[slot].vertexOffset  = int(obj.vertexOffset);
    commands[slot].firstInstance = idx;
}