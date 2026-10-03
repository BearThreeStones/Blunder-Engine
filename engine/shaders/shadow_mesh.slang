#include "gpu_driven_common.slang"

struct ShadowMeshUniform
{
    float4x4 viewProjection;
    float4x4 cubeFaceVP[6];
    float4 lightPositionRange;
    uint mode;          // 0 VSM page, 1 point face, 2 spot, 3 layered cube
    uint layer;         // physical page or cube layer or spot slot
    uint faceMask;      // meshlet base for this instance
    uint meshletCount;
    uint4 linkCountPad;
    uint linkIds[16];
};

struct MeshPayload
{
    uint meshletIndex;
    uint instanceIndex;
    uint layer;
};

struct ShadowMeshPush
{
    uint instanceFirst;
    uint instanceCount;
    uint meshletBase;
    uint meshletCount;
};

[[vk::binding(0, 0)]]
ConstantBuffer<ShadowMeshUniform> ubo;

[[vk::push_constant]]
ConstantBuffer<ShadowMeshPush> push;

[[vk::binding(1, 0)]]
StructuredBuffer<GpuDrivenInstanceGpu> instances;

[[vk::binding(2, 0)]]
StructuredBuffer<GpuDrivenMeshletGpu> meshlets;

[[vk::binding(3, 0)]]
ByteAddressBuffer vertexBytes;

[[vk::binding(4, 0)]]
ByteAddressBuffer meshletVertices;

[[vk::binding(5, 0)]]
ByteAddressBuffer meshletTriangles;

struct VertexOutput
{
    float4 sv_position : SV_Position;
};

struct VertexOutputLayered
{
    float4 sv_position : SV_Position;
    uint layer : SV_RenderTargetArrayIndex;
};

float3 loadPosition(uint vertexIndex)
{
    return asfloat(vertexBytes.Load3(vertexIndex * kMeshVertexStride));
}

uint loadMeshletVertex(uint offset)
{
    return meshletVertices.Load(offset * 4u);
}

uint loadTriangleByte(uint byteOffset)
{
    uint word = meshletTriangles.Load((byteOffset / 4u) * 4u);
    uint shift = (byteOffset & 3u) * 8u;
    return (word >> shift) & 0xffu;
}

bool instanceLinked(uint entityId)
{
    if (ubo.linkCountPad.x == 0u)
    {
        return true;
    }
    [unroll]
    for (uint i = 0; i < 16u; ++i)
    {
        if (i >= ubo.linkCountPad.x)
        {
            break;
        }
        if (ubo.linkIds[i] == entityId)
        {
            return true;
        }
    }
    return false;
}

float4x4 faceViewProjection(uint face)
{
    if (ubo.mode == 3u)
    {
        return ubo.cubeFaceVP[face];
    }
    return ubo.viewProjection;
}

bool sphereInMatrix(float4x4 vp, float3 center, float radius)
{
    float4 clip = mul(vp, float4(center, 1.0));
    float w = max(abs(clip.w), 1e-4);
    float3 ndc = clip.xyz / w;
    float pad = radius / w;
    return ndc.x >= -1.0 - pad && ndc.x <= 1.0 + pad &&
           ndc.y >= -1.0 - pad && ndc.y <= 1.0 + pad &&
           ndc.z >= -pad && ndc.z <= 1.0 + pad;
}

float instanceMaxScale(float4x4 world)
{
    float3 scaleX = mul((float3x3)world, float3(1.0, 0.0, 0.0));
    float3 scaleY = mul((float3x3)world, float3(0.0, 1.0, 0.0));
    float3 scaleZ = mul((float3x3)world, float3(0.0, 0.0, 1.0));
    return max(max(length(scaleX), length(scaleY)), length(scaleZ));
}

[shader("amplification")]
[numthreads(1, 1, 1)]
void taskMain(uint3 dtid : SV_DispatchThreadID)
{
    // One task group per meshlet per caster in this unique-mesh batch.
    // X walks meshlets (×6 faces in layered cube mode); Y walks instances.
    MeshPayload payload;
    payload.meshletIndex = 0;
    payload.instanceIndex = 0;
    payload.layer = ubo.layer;
    uint visible = 0;
    if (dtid.y < push.instanceCount)
    {
        uint instanceIndex = push.instanceFirst + dtid.y;
        GpuDrivenInstanceGpu inst = instances[instanceIndex];
        uint within = dtid.x;
        uint localInChunk = within;
        uint face = 0;
        if (ubo.mode == 3u)
        {
            localInChunk = within / 6u;
            face = within % 6u;
        }
        uint meshletLocal = push.meshletBase + localInChunk;
        if (localInChunk < push.meshletCount && meshletLocal < inst.meshletCount)
        {
            uint meshletIndex = inst.meshletOffset + meshletLocal;
            payload.meshletIndex = meshletIndex;
            payload.instanceIndex = instanceIndex;
            payload.layer = (ubo.mode == 3u) ? face : ubo.layer;
            GpuDrivenMeshletGpu meshlet = meshlets[meshletIndex];
            if (instanceLinked(inst.pad0))
            {
                float radius = meshlet.sphere.w * max(instanceMaxScale(inst.world), 1e-6);
                float3 center = mul(inst.world, float4(meshlet.sphere.xyz, 1.0)).xyz;
                if (sphereInMatrix(faceViewProjection(face), center, radius))
                {
                    visible = 1;
                }
            }
        }
    }
    DispatchMesh(visible, 1, 1, payload);
}

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(128, 1, 1)]
void meshMain(uint gtid : SV_GroupThreadID, in payload MeshPayload payload,
              out vertices VertexOutput verts[64], out indices uint3 tris[124])
{
    GpuDrivenMeshletGpu meshlet = meshlets[payload.meshletIndex];
    uint vertCount = meshlet.vertexCount;
    uint triCount = meshlet.triangleCount;
    if (vertCount > 64u)
    {
        vertCount = 64u;
    }
    if (triCount > 124u)
    {
        triCount = 124u;
    }
    SetMeshOutputCounts(vertCount, triCount);
    GpuDrivenInstanceGpu inst = instances[payload.instanceIndex];
    if (gtid < vertCount)
    {
        uint index = loadMeshletVertex(meshlet.vertexOffset + gtid);
        float3 pos = loadPosition(index);
        float4 worldPos = mul(inst.world, float4(pos, 1.0));
        verts[gtid].sv_position = mul(faceViewProjection(0), worldPos);
    }
    if (gtid < triCount)
    {
        uint base = meshlet.triangleOffset + gtid * 3u;
        tris[gtid] = uint3(loadTriangleByte(base), loadTriangleByte(base + 1u),
                           loadTriangleByte(base + 2u));
    }
}

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(128, 1, 1)]
void meshMainLayered(uint gtid : SV_GroupThreadID, in payload MeshPayload payload,
                     out vertices VertexOutputLayered verts[64],
                     out indices uint3 tris[124])
{
    GpuDrivenMeshletGpu meshlet = meshlets[payload.meshletIndex];
    uint vertCount = meshlet.vertexCount;
    uint triCount = meshlet.triangleCount;
    if (vertCount > 64u)
    {
        vertCount = 64u;
    }
    if (triCount > 124u)
    {
        triCount = 124u;
    }
    SetMeshOutputCounts(vertCount, triCount);
    GpuDrivenInstanceGpu inst = instances[payload.instanceIndex];
    if (gtid < vertCount)
    {
        uint index = loadMeshletVertex(meshlet.vertexOffset + gtid);
        float3 pos = loadPosition(index);
        float4 worldPos = mul(inst.world, float4(pos, 1.0));
        verts[gtid].sv_position = mul(faceViewProjection(payload.layer), worldPos);
        verts[gtid].layer = payload.layer;
    }
    if (gtid < triCount)
    {
        uint base = meshlet.triangleOffset + gtid * 3u;
        tris[gtid] = uint3(loadTriangleByte(base), loadTriangleByte(base + 1u),
                           loadTriangleByte(base + 2u));
    }
}

[shader("fragment")]
void fragmentMain()
{
}
