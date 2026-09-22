#include "gpu_driven_common.slang"

[[vk::binding(0, 0)]]
ConstantBuffer<GpuDrivenEmitUniforms> ubo;

[[vk::binding(1, 0)]]
StructuredBuffer<GpuDrivenMeshletGpu> meshlets;

[[vk::binding(2, 0)]]
StructuredBuffer<uint> uniqueExpanded;

[[vk::binding(3, 0)]]
StructuredBuffer<uint> uniqueBatch;

[[vk::binding(4, 0)]]
StructuredBuffer<uint> batchBases;

[[vk::binding(5, 0)]]
StructuredBuffer<uint> uniqueFirsts;

[[vk::binding(6, 0)]]
StructuredBuffer<uint> compactBases;

[[vk::binding(7, 0)]]
StructuredBuffer<uint> earlyInstanceCounts;

[[vk::binding(8, 0)]]
StructuredBuffer<uint> lateInstanceCounts;

[[vk::binding(9, 0)]]
RWStructuredBuffer<DrawIndexedIndirectCommand> earlyCommands;

[[vk::binding(10, 0)]]
RWStructuredBuffer<DrawIndexedIndirectCommand> lateCommands;

[[vk::binding(11, 0)]]
RWStructuredBuffer<uint> earlyCounts;

[[vk::binding(12, 0)]]
RWStructuredBuffer<uint> lateCounts;

DrawIndexedIndirectCommand makeCmd(GpuDrivenMeshletGpu meshlet, uint n,
                                   uint compactBase, uint uniqueLocal)
{
    DrawIndexedIndirectCommand cmd;
    cmd.indexCount = meshlet.indexCount;
    cmd.instanceCount = n;
    cmd.firstIndex = meshlet.firstIndex;
    cmd.vertexOffset = 0;
    cmd.firstInstance = compactBase;
    cmd.pad0 = uniqueLocal;
    cmd.pad1 = 0;
    cmd.pad2 = 0;
    return cmd;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint u = id.x;
    if (u >= ubo.uniqueCount)
    {
        return;
    }

    uint expanded = uniqueExpanded[u];
    uint batch = uniqueBatch[u];
    uint uniqueLocal = expanded - batchBases[batch];
    GpuDrivenMeshletGpu meshlet = meshlets[expanded];
    uint compactBase = compactBases[u];
    uint earlyN = earlyInstanceCounts[u];
    uint lateN = lateInstanceCounts[u];
    const bool compact = ubo.compactCommands != 0u;

    if (compact)
    {
        if (earlyN > 0u)
        {
            uint dst;
            InterlockedAdd(earlyCounts[batch], 1u, dst);
            earlyCommands[uniqueFirsts[batch] + dst] =
                makeCmd(meshlet, earlyN, compactBase, uniqueLocal);
        }
        if (lateN > 0u)
        {
            uint dst;
            InterlockedAdd(lateCounts[batch], 1u, dst);
            lateCommands[uniqueFirsts[batch] + dst] =
                makeCmd(meshlet, lateN, compactBase, uniqueLocal);
        }
        return;
    }

    earlyCommands[uniqueFirsts[batch] + uniqueLocal] =
        makeCmd(meshlet, earlyN, compactBase, uniqueLocal);
    lateCommands[uniqueFirsts[batch] + uniqueLocal] =
        makeCmd(meshlet, lateN, compactBase, uniqueLocal);
    uint unused;
    if (earlyN > 0u)
    {
        InterlockedAdd(earlyCounts[batch], 1u, unused);
    }
    if (lateN > 0u)
    {
        InterlockedAdd(lateCounts[batch], 1u, unused);
    }
}
