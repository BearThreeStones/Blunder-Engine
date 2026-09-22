#include "gpu_driven_common.slang"

[[vk::binding(0, 0)]]
ConstantBuffer<GpuDrivenCullUniforms> ubo;

[[vk::binding(1, 0)]]
StructuredBuffer<GpuDrivenInstanceGpu> instances;

[[vk::binding(2, 0)]]
StructuredBuffer<GpuDrivenMeshletGpu> meshlets;

[[vk::binding(3, 0)]]
RWStructuredBuffer<DrawIndexedIndirectCommand> earlyCommands;

[[vk::binding(4, 0)]]
RWStructuredBuffer<DrawIndexedIndirectCommand> lateCommands;

[[vk::binding(5, 0)]]
Texture2D<float> hizPyramid;

[[vk::binding(6, 0)]]
SamplerState hizSampler;

[[vk::binding(7, 0)]]
RWStructuredBuffer<uint> earlyCounts;

[[vk::binding(8, 0)]]
RWStructuredBuffer<uint> lateCounts;

[[vk::binding(9, 0)]]
StructuredBuffer<uint> batchBases;

bool sphereInsideFrustum(float3 center, float radius)
{
    for (int i = 0; i < 6; ++i)
    {
        float4 plane = ubo.frustumPlanes[i];
        float distance = dot(plane.xyz, center) + plane.w;
        if (distance < -radius)
        {
            return false;
        }
    }
    return true;
}

bool coneCulled(float3 worldCenter, float3 worldAxis, float cutoff, float radius,
                float3 camera)
{
    if (cutoff <= 0.0)
    {
        return false;
    }
    float axisLenSq = dot(worldAxis, worldAxis);
    if (axisLenSq < 1e-12)
    {
        return false;
    }
    float3 fromCamera = worldCenter - camera;
    float dist = length(fromCamera);
    if (dist <= radius + 1e-3)
    {
        return false;
    }
    float3 axis = worldAxis * rsqrt(axisLenSq);
    return dot(fromCamera, axis) >= cutoff * dist + radius;
}

bool hizOccluded(float4 clip, float radiusWorld, float3 worldCenter)
{
    if (ubo.hizEnabled == 0u || clip.w <= 1e-6)
    {
        return false;
    }
    float3 toCenter = worldCenter - ubo.cameraPosition.xyz;
    float dist = length(toCenter);
    if (dist <= radiusWorld + 1e-3)
    {
        return false;
    }
    // Zoomed-out centimetre Sponza: previous-frame courtyard Hi-Z is many
    // radii in front of far meshlets and false-occludes the building. Overlay
    // gizmos skip Hi-Z, so Unique icons stay on the courtyard while one wing
    // survives in a corner. Close meshlets still use the pyramid.
    if (dist > 8.0 * max(radiusWorld, 1e-4))
    {
        return false;
    }
    float3 ndc = clip.xyz / clip.w;
    float ndcRadius = (2.0 * radiusWorld) / clip.w;
    float2 aabbMin = ndc.xy - ndcRadius;
    float2 aabbMax = ndc.xy + ndcRadius;
    if (aabbMax.x < -1.0 || aabbMin.x > 1.0 || aabbMax.y < -1.0 || aabbMin.y > 1.0)
    {
        return false;
    }
    float2 uvMin = float2(aabbMin.x * 0.5 + 0.5, 1.0 - (aabbMax.y * 0.5 + 0.5));
    float2 uvMax = float2(aabbMax.x * 0.5 + 0.5, 1.0 - (aabbMin.y * 0.5 + 0.5));
    uvMin = saturate(uvMin);
    uvMax = saturate(uvMax);
    float2 sizeUv = max(uvMax - uvMin, float2(1.0 / max(ubo.hizSize.x, 1.0),
                                              1.0 / max(ubo.hizSize.y, 1.0)));
    float diameterPx = max(sizeUv.x * ubo.hizSize.x, sizeUv.y * ubo.hizSize.y);
    if (diameterPx > 64.0)
    {
        return false;
    }
    float mip = 0.0;
    if (ubo.hizMipCount > 1u && diameterPx > 1.0)
    {
        mip = min(ceil(log2(diameterPx)), float(ubo.hizMipCount - 1u));
    }
    float z0 = hizPyramid.SampleLevel(hizSampler, uvMin, mip);
    float z1 = hizPyramid.SampleLevel(hizSampler, float2(uvMax.x, uvMin.y), mip);
    float z2 = hizPyramid.SampleLevel(hizSampler, float2(uvMin.x, uvMax.y), mip);
    float z3 = hizPyramid.SampleLevel(hizSampler, uvMax, mip);
    float sampled = max(max(z0, z1), max(z2, z3));
    if (sampled <= 1e-4)
    {
        return false;
    }
    float3 nearestPt = worldCenter - toCenter * (radiusWorld / dist);
    float4 nearestClip = mul(ubo.viewProjection, float4(nearestPt, 1.0));
    float nearestZ = ndc.z;
    if (nearestClip.w > 1e-6)
    {
        nearestZ = nearestClip.z / nearestClip.w;
    }
    return nearestZ > sampled + 1e-4;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint index = id.x;
    DrawIndexedIndirectCommand emptyCmd;
    emptyCmd.indexCount = 0;
    emptyCmd.instanceCount = 0;
    emptyCmd.firstIndex = 0;
    emptyCmd.vertexOffset = 0;
    emptyCmd.firstInstance = 0;
    emptyCmd.pad0 = 0;
    emptyCmd.pad1 = 0;
    emptyCmd.pad2 = 0;

    if (index >= ubo.meshletCount)
    {
        return;
    }

    GpuDrivenMeshletGpu meshlet = meshlets[index];
    GpuDrivenInstanceGpu inst = instances[meshlet.instanceIndex];
    float3 localCenter = meshlet.sphere.xyz;
    float3 scaleX = mul((float3x3)inst.world, float3(1.0, 0.0, 0.0));
    float3 scaleY = mul((float3x3)inst.world, float3(0.0, 1.0, 0.0));
    float3 scaleZ = mul((float3x3)inst.world, float3(0.0, 0.0, 1.0));
    float maxScale = max(max(length(scaleX), length(scaleY)), length(scaleZ));
    float radius = meshlet.sphere.w * max(maxScale, 1e-6);
    float4 worldCenter4 = mul(inst.world, float4(localCenter, 1.0));
    float3 worldCenter = worldCenter4.xyz;
    float3 axisLocal = meshlet.cone.xyz;
    float3 worldAxis = mul((float3x3)inst.world, axisLocal);

    float distToCamera = length(worldCenter - ubo.cameraPosition.xyz);
    bool visible = sphereInsideFrustum(worldCenter, radius);
    bool skipCone = ubo.skipCone != 0u ||
                    (meshlet.flags & kGpuDrivenFlagSkipCone) != 0u ||
                    distToCamera > radius * 8.0 || distToCamera > 1500.0;
    if (visible && !skipCone)
    {
        visible = !coneCulled(worldCenter, worldAxis, meshlet.cone.w, radius,
                              ubo.cameraPosition.xyz);
    }

    DrawIndexedIndirectCommand cmd;
    cmd.indexCount = meshlet.indexCount;
    cmd.instanceCount = 1;
    cmd.firstIndex = meshlet.firstIndex;
    cmd.vertexOffset = 0;
    cmd.firstInstance = meshlet.instanceIndex;
    cmd.pad0 = index - batchBases[meshlet.flags >> kGpuDrivenMeshletBatchShift];
    cmd.pad1 = 0;
    cmd.pad2 = 0;

    const bool compact = ubo.compactCommands != 0u;
    uint batchIndex = meshlet.flags >> kGpuDrivenMeshletBatchShift;
    if (!visible)
    {
        if (!compact)
        {
            earlyCommands[index] = emptyCmd;
            lateCommands[index] = emptyCmd;
        }
        return;
    }

    float4 clip = mul(ubo.viewProjection, float4(worldCenter, 1.0));
    bool occluded = hizOccluded(clip, radius, worldCenter);
    if (compact)
    {
        uint base = batchBases[batchIndex];
        uint dst;
        if (occluded)
        {
            InterlockedAdd(lateCounts[batchIndex], 1u, dst);
            lateCommands[base + dst] = cmd;
        }
        else
        {
            InterlockedAdd(earlyCounts[batchIndex], 1u, dst);
            earlyCommands[base + dst] = cmd;
        }
        return;
    }

    uint unused;
    if (occluded)
    {
        InterlockedAdd(lateCounts[batchIndex], 1u, unused);
        earlyCommands[index] = emptyCmd;
        lateCommands[index] = cmd;
    }
    else
    {
        InterlockedAdd(earlyCounts[batchIndex], 1u, unused);
        earlyCommands[index] = cmd;
        lateCommands[index] = emptyCmd;
    }
}
