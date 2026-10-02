#include "gpu_driven_common.slang"

// After meshlet_emit compact: turn per-batch surviving counts into one
// VkDrawMeshTasksIndirectCommandEXT {surviving, 1, 1} so Forward / G-buffer
// mesh can DispatchMesh only for packed survivors (not batch.meshlet_count).
// Draw-count buffer is 0/1 for DrawMeshTasksIndirectCountEXT (skip empty).

[[vk::binding(0, 0)]]
ConstantBuffer<GpuDrivenMeshTaskPackUniforms> ubo;

[[vk::binding(1, 0)]]
StructuredBuffer<uint> earlyCounts;

[[vk::binding(2, 0)]]
StructuredBuffer<uint> lateCounts;

[[vk::binding(3, 0)]]
RWStructuredBuffer<DrawMeshTasksIndirectCommand> earlyMeshTaskCmds;

[[vk::binding(4, 0)]]
RWStructuredBuffer<DrawMeshTasksIndirectCommand> lateMeshTaskCmds;

[[vk::binding(5, 0)]]
RWStructuredBuffer<uint> earlyMeshTaskDrawCounts;

[[vk::binding(6, 0)]]
RWStructuredBuffer<uint> lateMeshTaskDrawCounts;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint batch = id.x;
    if (batch >= ubo.batchCount)
    {
        return;
    }

    uint earlyN = earlyCounts[batch];
    uint lateN = lateCounts[batch];

    DrawMeshTasksIndirectCommand earlyCmd;
    earlyCmd.groupCountX = earlyN;
    earlyCmd.groupCountY = 1u;
    earlyCmd.groupCountZ = 1u;
    earlyCmd.pad0 = 0u;
    earlyMeshTaskCmds[batch] = earlyCmd;
    earlyMeshTaskDrawCounts[batch] = earlyN > 0u ? 1u : 0u;

    DrawMeshTasksIndirectCommand lateCmd;
    lateCmd.groupCountX = lateN;
    lateCmd.groupCountY = 1u;
    lateCmd.groupCountZ = 1u;
    lateCmd.pad0 = 0u;
    lateMeshTaskCmds[batch] = lateCmd;
    lateMeshTaskDrawCounts[batch] = lateN > 0u ? 1u : 0u;
}
