struct PickInstanceGpu
{
    uint entityId;
    uint parentId;
    uint flags;
    uint drawIndex;
    float3 aabbMin;
    float aabbPad0;
    float3 aabbMax;
    float aabbPad1;
};

struct PickBroadUniforms
{
    float3 rayOrigin;
    float rayPad0;
    float3 rayDir;
    float rayPad1;
    uint instanceCount;
    uint maxHits;
    uint pad0;
    uint pad1;
};

struct BroadHitRecord
{
    uint entityId;
    float t;
};

static const uint kPickableFlag = 1u << 0u;

[[vk::binding(0, 0)]]
ConstantBuffer<PickBroadUniforms> ubo;

[[vk::binding(1, 0)]]
StructuredBuffer<PickInstanceGpu> instances;

[[vk::binding(2, 0)]]
RWStructuredBuffer<BroadHitRecord> hitRecords;

[[vk::binding(3, 0)]]
RWStructuredBuffer<uint> hitCount;

bool intersectRayAabb(float3 origin, float3 dir, float3 boxMin, float3 boxMax, out float tNear)
{
    float3 invDir = 1.0 / dir;
    float3 t0 = (boxMin - origin) * invDir;
    float3 t1 = (boxMax - origin) * invDir;
    float3 tmin = min(t0, t1);
    float3 tmax = max(t0, t1);
    tNear = max(max(tmin.x, tmin.y), tmin.z);
    float tFar = min(min(tmax.x, tmax.y), tmax.z);
    return tNear <= tFar && tFar >= 0.0;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    const uint index = id.x;
    if (index >= ubo.instanceCount)
    {
        return;
    }

    PickInstanceGpu inst = instances[index];
    if ((inst.flags & kPickableFlag) == 0u)
    {
        return;
    }

    float tHit = 0.0;
    if (!intersectRayAabb(ubo.rayOrigin, ubo.rayDir, inst.aabbMin, inst.aabbMax, tHit))
    {
        return;
    }

  if (tHit < 0.0)
  {
    return;
  }

    uint slot = 0u;
    InterlockedAdd(hitCount[0], 1u, slot);
    if (slot < ubo.maxHits)
    {
        hitRecords[slot].entityId = inst.entityId;
        hitRecords[slot].t = tHit;
    }
}
